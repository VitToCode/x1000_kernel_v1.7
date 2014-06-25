#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/usb/otg.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/wakelock.h>

#include <linux/jz_dwc.h>
#include <soc/base.h>
#include <soc/cpm.h>

#include "core.h"
#include "gadget.h"

#define OTG_CLK_NAME		"otg1"
#define VBUS_REG_NAME		"vbus"

struct dwc2_jz {
	struct platform_device  dwc2;
	struct device		*dev;
	struct clk		*clk;

	spinlock_t		lock; /* protect between first power on op and irq op */

#if DWC2_DEVICE_MODE_ENABLE
	int 			dete_irq;
	struct jzdwc_pin 	*dete;
	int			pullup_on;
	struct delayed_work	work;
#endif

#if DWC2_HOST_MODE_ENABLE
	int			id_irq;
	struct jzdwc_pin 	*id_pin;
	struct wake_lock        id_resume_wake_lock;
	struct delayed_work	host_id_work;
	struct timer_list	host_id_timer;
	int 			host_id_dog_count;
#define DWC2_HOST_ID_TIMER_INTERVAL (HZ / 2)
#define DWC2_HOST_ID_MAX_DOG_COUNT  3

	struct regulator 	*vbus;
	struct mutex             vbus_lock;
#ifdef CONFIG_USB_DWC2_INPUT_EVDEV
	struct input_dev	*input;
#endif	/* CONFIG_USB_DWC2_INPUT_EVDEV */
#endif	/* DWC2_HOST_MODE_ENABLE */
};

#define to_dwc2_jz(dwc) container_of((dwc)->pdev, struct dwc2_jz, dwc2);

#if DWC2_DEVICE_MODE_ENABLE
struct jzdwc_pin __attribute__((weak)) dete_pin = {
	.num	      = -1,
	.enable_level = -1,
};
#endif

void (*set_charger_current)(int cur) = NULL;
EXPORT_SYMBOL(set_charger_current);

/* NOTE: the following global variables are only used by special late_initcall */
static struct dwc2_jz *g_dwc2_jz = NULL;

#if !DWC2_HOST_MODE_ENABLE
void jz_set_vbus(struct dwc2 *dwc, int is_on) {  }
int dwc2_get_id_level(struct dwc2 *dwc) { return 1; }

#else  /* DWC2_HOST_MODE_ENABLE */

static int g_ignore_vbus_on __read_mostly = 1;
static int g_vbus_on_pending = 0;

struct jzdwc_pin __attribute__((weak)) dwc2_id_pin = {
	.num	      = -1,
	.enable_level = -1
};

extern void dwc2_notifier_call_chain_sync(int state);

/* mA */
#define CHARGER_CURRENT_USB	228
#define CHARGER_CURRENT_CHARGER	500

static void charger_set_current(int cur) {
	if (set_charger_current) {
		set_charger_current(cur);
	}
}

static void __jz_set_vbus(struct dwc2_jz *jz, int is_on) {
	struct dwc2 *dwc = platform_get_drvdata(&jz->dwc2);
	int old_is_on = is_on;

	if (unlikely(g_ignore_vbus_on) && is_on) {
		g_vbus_on_pending = 1;
		return;
	}

	mutex_lock(&jz->vbus_lock);

	if (dwc->plugin) {
		old_is_on = is_on;
		if (is_on) {
			dwc->extern_vbus_mode = 1;
			dwc2_notifier_call_chain_sync(1);
		}
		is_on = 0;
	} else {
		int id_level = 0;

		if (gpio_is_valid(jz->id_pin->num)) {
			id_level = gpio_get_value(jz->id_pin->num);
		}

		if (id_level)
			is_on = 0;
	}

	dev_info(jz->dev, "set vbus %s(%s) for %s mode\n",
		is_on ? "on" : "off",
		old_is_on ? "on" : "off",
		dwc2_is_host_mode(dwc) ? "host" : "device");

	if (is_on && dwc2_is_host_mode(dwc)) {
		if (!regulator_is_enabled(jz->vbus))
			regulator_enable(jz->vbus);
	} else {	      /* off or device mode */
		if (regulator_is_enabled(jz->vbus))
			regulator_disable(jz->vbus);
	}

	if (dwc->plugin) {
		if (old_is_on) {
			charger_set_current(CHARGER_CURRENT_USB);
		} else {
			charger_set_current(CHARGER_CURRENT_CHARGER);
		}
	}

	mutex_unlock(&jz->vbus_lock);
}

void jz_set_vbus(struct dwc2 *dwc, int is_on)
{
	struct dwc2_jz	*jz;

	jz = container_of(dwc->pdev, struct dwc2_jz, dwc2);
	if ( (jz->vbus == NULL) || IS_ERR(jz->vbus) )
		return;

	__jz_set_vbus(jz, is_on);
}

static ssize_t jz_vbus_show(struct device *dev,
				struct device_attribute *attr,
				char *buf) {
	struct dwc2_jz *jz = dev_get_drvdata(dev);

	return sprintf(buf, "%s\n",
		regulator_is_enabled(jz->vbus) ? "on" : "off");
}

static ssize_t jz_vbus_set(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct dwc2_jz *jz = dev_get_drvdata(dev);
	struct dwc2 *dwc = platform_get_drvdata(&jz->dwc2);
	int is_on = 0;

	if (strncmp(buf, "on", 2) == 0) {
		int id_level = 0;

		if (dwc->extern_vbus_mode) {
			charger_set_current(CHARGER_CURRENT_USB);
			return count;
		}

		if (gpio_is_valid(jz->id_pin->num)) {
			id_level = gpio_get_value(jz->id_pin->num);
		}

		if (id_level) {
			return count;
		}

		is_on = 1;
	}

	__jz_set_vbus(jz, is_on);

	return count;
}

static DEVICE_ATTR(vbus, S_IWUSR | S_IRUSR,
		jz_vbus_show, jz_vbus_set);

static ssize_t jz_id_show(struct device *dev,
				struct device_attribute *attr,
				char *buf) {
	struct dwc2_jz *jz = dev_get_drvdata(dev);
	int id_level = 0;

	if (gpio_is_valid(jz->id_pin->num)) {
		id_level = gpio_get_value(jz->id_pin->num);
	}

	return sprintf(buf, "%s\n", id_level ? "1" : "0");
}

static DEVICE_ATTR(id, S_IRUSR |  S_IRGRP | S_IROTH,
		jz_id_show, NULL);
#endif	/* DWC2_HOST_MODE_ENABLE */

static struct attribute *dwc2_jz_attributes[] = {
#if DWC2_HOST_MODE_ENABLE
	&dev_attr_vbus.attr,
	&dev_attr_id.attr,
#endif	/* DWC2_HOST_MODE_ENABLE */
	NULL
};

static const struct attribute_group dwc2_jz_attr_group = {
	.attrs = dwc2_jz_attributes,
};

#ifdef CONFIG_USB_DWC2_INPUT_EVDEV
int dwc2_jz_input_init(struct dwc2_jz *jz) {
	struct input_dev *input;
	int ret;

	input = input_allocate_device();
	if (input == NULL)
		return -ENOMEM;

	input->name = "dwc2";
	input->dev.parent = jz->dev;

	__set_bit(EV_KEY, input->evbit);
	__set_bit(KEY_POWER2, input->keybit);

	if ((ret = input_register_device(input)) < 0)
		goto error;

	jz->input = input;

	return 0;

error:
	input_free_device(input);
	return ret;
}

static void dwc2_jz_input_cleanup(struct dwc2_jz *jz)
{
	if (jz->input)
		input_unregister_device(jz->input);
}

static void dwc2_jz_input_report_key(struct dwc2_jz *jz,
					unsigned int code, int value)
{
	if (jz->input) {
		input_report_key(jz->input, code, value);
		input_sync(jz->input);
	}
}

static void __dwc2_input_report_power2_key(struct dwc2_jz *jz) {
	dwc2_jz_input_report_key(jz, KEY_POWER2, 1);
	msleep(50);
	dwc2_jz_input_report_key(jz, KEY_POWER2, 0);
}

void dwc2_input_report_power2_key(struct dwc2 *dwc) {
	struct dwc2_jz	*jz;

	jz = container_of(dwc->pdev, struct dwc2_jz, dwc2);

	__dwc2_input_report_power2_key(jz);
}

#else
#define dwc2_jz_input_init(dev) do {  } while(0)
#define dwc2_jz_input_cleanup(dev) do {  } while(0)
#define dwc2_jz_input_report_key(dev, code, value) do {  } while(0)
void dwc2_input_report_power2_key(struct dwc2 *dwc) {  }
static void __attribute__((unused)) __dwc2_input_report_power2_key(struct dwc2_jz *jz) {  }
#endif /* CONFIG_USB_DWC2_INPUT_EVDEV */

#if DWC2_DEVICE_MODE_ENABLE
static int get_pin_status(struct jzdwc_pin *pin)
{
	int val;

	if (pin->num < 0)
		return -1;
	val = gpio_get_value(pin->num);

	if (pin->enable_level == LOW_ENABLE)
		return !val;
	return val;
}

extern void dwc2_gadget_plug_change(int plugin);

static void __usb_plug_change(struct dwc2_jz *jz) {
	int insert;

	insert = get_pin_status(jz->dete);

	pr_info("USB %s\n", insert ? "connect" : "disconnect");
	dwc2_gadget_plug_change(insert);
}

static void usb_plug_change(struct dwc2_jz *jz) {

	spin_lock(&jz->lock);
	__usb_plug_change(jz);
	spin_unlock(&jz->lock);
}

static void usb_detect_work(struct work_struct *work)
{
	struct dwc2_jz	*jz;

	jz = container_of(work, struct dwc2_jz, work.work);

	usb_plug_change(jz);

	enable_irq(jz->dete_irq);
}

static irqreturn_t usb_detect_interrupt(int irq, void *dev_id)
{
	struct dwc2_jz	*jz = (struct dwc2_jz *)dev_id;

	disable_irq_nosync(irq);
	schedule_delayed_work(&jz->work, msecs_to_jiffies(100));

	return IRQ_HANDLED;
}
#endif	/* DWC2_DEVICE_MODE_ENABLE */
void dwc2_clk_enable(struct dwc2 *dwc) {
	struct dwc2_jz *jz = to_dwc2_jz(dwc);

	clk_enable(jz->clk);
}

void dwc2_clk_disable(struct dwc2 *dwc) {
	struct dwc2_jz *jz = to_dwc2_jz(dwc);

	clk_disable(jz->clk);
}

int dwc2_clk_is_enabled(struct dwc2 *dwc) {
	struct dwc2_jz *jz = to_dwc2_jz(dwc);

	return clk_is_enabled(jz->clk);
}


#if DWC2_HOST_MODE_ENABLE
static int __dwc2_get_id_level(struct dwc2_jz *jz) {
	int			 id_level = 1;

	if (gpio_is_valid(jz->id_pin->num)) {
		id_level = gpio_get_value(jz->id_pin->num);
	}

	return id_level;
}

int dwc2_get_id_level(struct dwc2 *dwc) {
	struct dwc2_jz	*jz;

	jz = container_of(dwc->pdev, struct dwc2_jz, dwc2);

	return __dwc2_get_id_level(jz);
}

static void usb_host_id_timer(unsigned long _jz) {
	struct dwc2_jz *jz = (struct dwc2_jz *)_jz;

	if (gpio_get_value(jz->id_pin->num) == 0) /* host */
		jz_otg_phy_suspend(0);

	if (jz->host_id_dog_count > 0) {
		jz->host_id_dog_count --;
		mod_timer(&jz->host_id_timer, jiffies + DWC2_HOST_ID_TIMER_INTERVAL);
	}
}

static int __usb_host_id_change(struct dwc2_jz *jz) {
	struct dwc2 *dwc = platform_get_drvdata(&jz->dwc2);
	int is_host = 0;
	// printk("==============>enter %s, id pin level=%d\n",
	//	__func__, gpio_get_value(jz->id_pin->num));

	if (gpio_get_value(jz->id_pin->num) == 0) { /* host */
		jz_otg_phy_suspend(0);
		is_host = 1;
		if (!dwc2_clk_is_enabled(dwc))
			dwc2_enable_clk(dwc);
	}

	jz->host_id_dog_count = DWC2_HOST_ID_MAX_DOG_COUNT;
	mod_timer(&jz->host_id_timer, jiffies + DWC2_HOST_ID_TIMER_INTERVAL);

	return is_host;
}

static int usb_host_id_change(struct dwc2_jz *jz) {
	int is_host;

	spin_lock(&jz->lock);
	is_host = __usb_host_id_change(jz);
	spin_unlock(&jz->lock);

	return is_host;
}

static void usb_host_id_work(struct work_struct *work) {
	struct dwc2_jz	*jz;
	int			 report_key = 0;

	jz = container_of(work, struct dwc2_jz, host_id_work.work);

	report_key = usb_host_id_change(jz);

	enable_irq(jz->id_irq);

	if (report_key) {
		__dwc2_input_report_power2_key(jz);
	}
}

static irqreturn_t usb_host_id_interrupt(int irq, void *dev_id) {
	struct dwc2_jz	*jz = (struct dwc2_jz *)dev_id;

	disable_irq_nosync(irq);
#if 0
	if (gpio_get_value(jz->id_pin->num) == 0) { /* host */
		jz_otg_phy_suspend(0);
	}

	printk("===>id pin level=%d\n", gpio_get_value(jz->id_pin->num));
#endif
	wake_lock_timeout(&jz->id_resume_wake_lock, 3 * HZ);

	/* 50ms dither filter */
	schedule_delayed_work(&jz->host_id_work, msecs_to_jiffies(50));

	return IRQ_HANDLED;
}
#endif	/* DWC2_HOST_MODE_ENABLE */

void jz_usb_set_mode(void) {
#if (DWC2_DEVICE_MODE_ENABLE) && !(DWC2_HOST_MODE_ENABLE)
	jz_otg_set_device_only_mode();
#else
	jz_otg_set_dual_mode();
#endif
}

static u64 dwc2_jz_dma_mask = DMA_BIT_MASK(32);

static int dwc2_jz_probe(struct platform_device *pdev) {
	struct platform_device		*dwc2;
	struct dwc2_jz		*jz;
	struct dwc2_platform_data	*dwc2_plat_data;
	int				 ret = -ENOMEM;

	jz = kzalloc(sizeof(*jz), GFP_KERNEL);
	if (!jz) {
		dev_err(&pdev->dev, "not enough memory\n");
		goto out;
	}

	/*
	 * Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we move to full device tree support this will vanish off.
	 */
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &dwc2_jz_dma_mask;

	platform_set_drvdata(pdev, jz);

	dwc2 = &jz->dwc2;
	dwc2->name = "dwc2";
	dwc2->id = -1;
	device_initialize(&dwc2->dev);
	dma_set_coherent_mask(&dwc2->dev, pdev->dev.coherent_dma_mask);
	dwc2->dev.parent = &pdev->dev;
	dwc2->dev.dma_mask = pdev->dev.dma_mask;
	dwc2->dev.dma_parms = pdev->dev.dma_parms;

	dwc2->dev.platform_data = kzalloc(sizeof(struct dwc2_platform_data), GFP_KERNEL);
	if (!dwc2->dev.platform_data) {
		goto fail_alloc_dwc2_plat_data;
	}
	dwc2_plat_data = dwc2->dev.platform_data;

	jz->dev	= &pdev->dev;
	spin_lock_init(&jz->lock);

	jz->clk = clk_get(NULL, OTG_CLK_NAME);
	if (IS_ERR(jz->clk)) {
		dev_err(&pdev->dev, "clk gate get error\n");
		goto fail_get_clk;
	}
	clk_enable(jz->clk);

#if DWC2_DEVICE_MODE_ENABLE
	jz->dete = &dete_pin;
	jz->dete_irq = -1;
	ret = gpio_request_one(jz->dete->num,
			GPIOF_DIR_IN, "usb-charger-detect");
	if (ret == 0) {
		jz->dete_irq = gpio_to_irq(jz->dete->num);
		INIT_DELAYED_WORK(&jz->work, usb_detect_work);
	}
#ifdef CONFIG_BOARD_HAS_NO_DETE_FACILITY
	if (jz->dete_irq < 0) {
		dwc2_plat_data->keep_phy_on = 1;
	}
#endif	/* CONFIG_BOARD_HAS_NO_DETE_FACILITY */
#endif	/* DWC2_DEVICE_MODE_ENABLE */

#if DWC2_HOST_MODE_ENABLE
	jz->vbus = NULL;
#ifdef CONFIG_REGULATOR
	jz->vbus = regulator_get(NULL, VBUS_REG_NAME);

	if (IS_ERR(jz->vbus)) {
		jz->vbus = NULL;
		dev_err(&pdev->dev, "regulator %s get error\n", VBUS_REG_NAME);
	}
#endif

	mutex_init(&jz->vbus_lock);
	jz->id_pin = &dwc2_id_pin;
	jz->id_irq = -1;
	ret = gpio_request_one(jz->id_pin->num,
			GPIOF_DIR_IN, "usb-host-id-detect");
	if (ret == 0) {
		jz->id_irq = gpio_to_irq(jz->id_pin->num);
		INIT_DELAYED_WORK(&jz->host_id_work, usb_host_id_work);
		setup_timer(&jz->host_id_timer, usb_host_id_timer,
			(unsigned long)jz);
		jz->host_id_dog_count = 0;

		wake_lock_init(&jz->id_resume_wake_lock, WAKE_LOCK_SUSPEND, "otgid-resume");
	} else {
		dwc2_plat_data->keep_phy_on = 1;
	}
#endif	/* DWC2_HOST_MODE_ENABLE */

	dwc2_jz_input_init(jz);

	otg_cpm_init();

	jz_usb_set_mode();

	jz_otg_phy_init();

	jz_otg_phy_suspend(!dwc2_plat_data->keep_phy_on);

	/*
	 * Close VBUS detect in DWC-OTG PHY.
	 */
	*(unsigned int*)0xb3500000 |= 0xc;

	ret = platform_device_add_resources(dwc2, pdev->resource,
					pdev->num_resources);
	if (ret) {
		dev_err(&pdev->dev, "couldn't add resources to dwc2 device\n");
		goto fail_add_dwc2_res;
	}

	ret = platform_device_add(dwc2);
	if (ret) {
		dev_err(&pdev->dev, "failed to register dwc2 device\n");
		goto fail_add_dwc2_dev;
	}

#if DWC2_DEVICE_MODE_ENABLE
	if (jz->dete_irq >= 0) {
		ret = request_irq(gpio_to_irq(jz->dete->num),
				usb_detect_interrupt,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"usb-detect", jz);
		if (ret) {
			jz->dete_irq = -1;
			dev_err(&pdev->dev, "request usb-detect fail\n");
			goto fail_req_dete_irq;
		} else {
			usb_plug_change(jz);
		}
	}

#ifdef CONFIG_BOARD_HAS_NO_DETE_FACILITY
	if (jz->dete_irq < 0) {
		dwc2_gadget_plug_change(1);
	}
#endif
#endif	/* DWC2_DEVICE_MODE_ENABLE */

#if DWC2_HOST_MODE_ENABLE
	if (jz->id_irq >= 0) {
		ret = request_irq(gpio_to_irq(jz->id_pin->num),
				usb_host_id_interrupt,
				IRQF_TRIGGER_FALLING,
				"usb-host-id", jz);
		if (ret) {
			jz->id_irq = -1;
			dev_err(&pdev->dev, "request host id interrupt fail!\n");
			goto fail_req_id_irq;
		} else {
			usb_host_id_change(jz);
		}
	}
#endif	/* DWC2_HOST_MODE_ENABLE */

	ret = sysfs_create_group(&pdev->dev.kobj, &dwc2_jz_attr_group);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to create sysfs group\n");
	}

	g_dwc2_jz = jz;

	return 0;

#if DWC2_HOST_MODE_ENABLE
fail_req_id_irq:
#endif

#if DWC2_DEVICE_MODE_ENABLE
	free_irq(gpio_to_irq(jz->dete->num), jz);
fail_req_dete_irq:
#endif	/* DWC2_DEVICE_MODE_ENABLE */

fail_add_dwc2_dev:
fail_add_dwc2_res:
#if DWC2_DEVICE_MODE_ENABLE
	if (gpio_is_valid(jz->dete->num))
		gpio_free(jz->dete->num);
#endif

#if DWC2_HOST_MODE_ENABLE
	if (gpio_is_valid(jz->id_pin->num))
		gpio_free(jz->id_pin->num);

#ifdef CONFIG_REGULATOR
	if(jz->vbus)
	regulator_put(jz->vbus);

#endif	/* CONFIG_REGULATOR */
#endif	/* DWC2_HOST_MODE_ENABLE */
	clk_disable(jz->clk);
	clk_put(jz->clk);

fail_get_clk:
	kfree(dwc2->dev.platform_data);

fail_alloc_dwc2_plat_data:
	kfree(jz);
out:
	return ret;
}

static int dwc2_jz_remove(struct platform_device *pdev) {
	struct dwc2_jz	*jz = platform_get_drvdata(pdev);

	dwc2_jz_input_cleanup(jz);

#if DWC2_DEVICE_MODE_ENABLE
	if (jz->dete_irq >= 0) {
		free_irq(jz->dete_irq, jz);
		gpio_free(jz->dete->num);
	}
#endif

#if DWC2_HOST_MODE_ENABLE
	if (jz->id_irq >= 0) {
		free_irq(jz->id_irq, jz);
		gpio_free(jz->id_pin->num);
	}

	if (!IS_ERR(jz->vbus))
		regulator_put(jz->vbus);
#endif

	clk_disable(jz->clk);
	clk_put(jz->clk);

	platform_device_unregister(&jz->dwc2);
	kfree(jz);

	return 0;
}

static int dwc2_jz_suspend(struct platform_device *pdev, pm_message_t state) {
	struct dwc2_jz	*jz = platform_get_drvdata(pdev);
#if DWC2_DEVICE_MODE_ENABLE
	if (jz->dete_irq >= 0)
		enable_irq_wake(jz->dete_irq);
#endif

#if DWC2_HOST_MODE_ENABLE
	if (jz->id_irq >= 0)
		enable_irq_wake(jz->id_irq);
#endif

	return 0;
}

static int dwc2_jz_resume(struct platform_device *pdev) {
	struct dwc2_jz	*jz = platform_get_drvdata(pdev);
#if DWC2_DEVICE_MODE_ENABLE
	if (jz->dete_irq >= 0)
		disable_irq_wake(jz->dete_irq);
#endif

#if DWC2_HOST_MODE_ENABLE
	if (jz->id_irq >= 0)
		disable_irq_wake(jz->id_irq);
#endif

	return 0;
}

static struct platform_driver dwc2_jz_driver = {
	.probe		= dwc2_jz_probe,
	.remove		= dwc2_jz_remove,
	.suspend	= dwc2_jz_suspend,
	.resume		= dwc2_jz_resume,
	.driver		= {
		.name	= "jz-dwc2",
		.owner =  THIS_MODULE,
	},
};


static int __init dwc2_jz_init(void)
{
	return platform_driver_register(&dwc2_jz_driver);
}

static void __exit dwc2_jz_exit(void)
{
	platform_driver_unregister(&dwc2_jz_driver);
}

/* make us init after usbcore and i2c (transceivers, regulators, etc)
 * and before usb gadget and host-side drivers start to register
 */
fs_initcall(dwc2_jz_init);
module_exit(dwc2_jz_exit);

static int __init dwc2_jz_late_init(void) {
	// printk("===>enter %s:%d: jz = %p, pending = %d\n",
	//	__func__, __LINE__, g_dwc2_jz, g_vbus_on_pending);
#if DWC2_HOST_MODE_ENABLE
	if (g_dwc2_jz) {
		g_ignore_vbus_on = 0;
		if (g_vbus_on_pending) {
			if (__dwc2_get_id_level(g_dwc2_jz) == 0) {
				__jz_set_vbus(g_dwc2_jz, 1);
			}

			g_vbus_on_pending = 0;
		}
	}
#endif

	return 0;
}

late_initcall(dwc2_jz_late_init);

MODULE_ALIAS("platform:jz-dwc2");
MODULE_AUTHOR("Lutts Cao <slcao@ingenic.cn>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DesignWare USB2.0 JZ Glue Layer");
