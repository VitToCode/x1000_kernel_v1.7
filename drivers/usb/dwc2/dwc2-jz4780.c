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

#include <linux/jz_dwc.h>
#include <soc/base.h>
#include <soc/cpm.h>

#include "core.h"
#include "gadget.h"

#define OTG_CLK_NAME		"otg1"
#define VBUS_REG_NAME		"vbus"
#define UCHARGER_REG_NAME	"ucharger"

#define USBRDT_VBFIL_LD_EN		25
#define USBPCR_TXPREEMPHTUNE		6
#define USBPCR_POR			22
#define USBPCR_USB_MODE			31
#define USBPCR_VBUSVLDEXT		24
#define USBPCR_VBUSVLDEXTSEL		23
#define USBPCR_OTG_DISABLE		20
#define USBPCR_IDPULLUP_MASK		28
#define OPCR_SPENDN0			7
#define USBPCR1_USB_SEL			28
#define USBPCR1_WORD_IF0		19
#define USBPCR1_WORD_IF1		18

struct dwc2_jz4780 {
	/* device lock */
	spinlock_t		lock;

	struct platform_device  dwc2;
	struct device		*dev;

	struct clk		*clk;
	int 			irq;
	struct jzdwc_pin 	*dete;
	struct delayed_work	work;
	struct delayed_work	charger_delay_work;

	struct regulator 	*vbus;
	struct regulator 	*ucharger;

	struct mutex		mutex;
	int			pullup_on;
};

struct jzdwc_pin __attribute__((weak)) dete_pin = {
	.num				= -1,
	.enable_level			= -1,
};

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

/* TODO: used by host, we current only re-work the device mode */
void jz4780_set_vbus(int is_on)
{
#if 0
	int ret = 0;

	if (jz4780 == NULL || jz4780->vbus == NULL) {
		return;
	}

	mutex_lock(&jz4780->mutex);
	if (is_on) {
		if (!regulator_is_enabled(jz4780->vbus))
			ret = regulator_enable(jz4780->vbus);
	} else {
		if (regulator_is_enabled(jz4780->vbus))
			ret = regulator_disable(jz4780->vbus);
	}
	mutex_unlock(&jz4780->mutex);
#endif
}

static inline void jz4780_usb_phy_init(struct dwc2_jz4780 *jz4780)
{
	pr_debug("init PHY\n");

	spin_lock(&jz4780->lock);
	cpm_set_bit(USBPCR_POR, CPM_USBPCR);
	spin_unlock(&jz4780->lock);

	msleep(1);

	spin_lock(&jz4780->lock);
	cpm_clear_bit(USBPCR_POR, CPM_USBPCR);
	spin_unlock(&jz4780->lock);

	msleep(1);
}

static inline void jz4780_usb_phy_switch(struct dwc2_jz4780 *jz4780, int is_on)
{
	unsigned int value;

	if (is_on) {
		spin_lock(&jz4780->lock);
		value = cpm_inl(CPM_OPCR);
		cpm_outl(value | (1 << OPCR_SPENDN0), CPM_OPCR);
		spin_unlock(&jz4780->lock);

		/* Wait PHY Clock Stable. */
		msleep(1);
		pr_info("enable PHY\n");

	} else {
		spin_lock(&jz4780->lock);
		value = cpm_inl(CPM_OPCR);
		cpm_outl(value & ~OPCR_SPENDN0, CPM_OPCR);
		spin_unlock(&jz4780->lock);

		pr_info("disable PHY\n");
	}
}

static inline void jz4780_usb_set_device_only_mode(void)
{
	cpm_clear_bit(USBPCR_USB_MODE, CPM_USBPCR);
	cpm_clear_bit(USBPCR_OTG_DISABLE, CPM_USBPCR);
}

static inline void jz4780_usb_set_dual_mode(void)
{
	unsigned int tmp;

	cpm_outl((1 << USBPCR_USB_MODE)
		| (1 << USBPCR_VBUSVLDEXT)
		| (1 << USBPCR_VBUSVLDEXTSEL),
		CPM_USBPCR);
	cpm_clear_bit(USBPCR_OTG_DISABLE, CPM_USBPCR);
	tmp = cpm_inl(CPM_USBPCR);
	cpm_outl(tmp & ~(0x03 << USBPCR_IDPULLUP_MASK), CPM_USBPCR);
}

void jz4780_set_charger_current(struct dwc2 *dwc, int pullup_on) {
	struct dwc2_jz4780	*jz4780;
	int			 insert;
	int			 curr_limit;
	dsts_data_t		 dsts;
	int			 frame_num;

	jz4780 = container_of(dwc->pdev, struct dwc2_jz4780, dwc2);
	jz4780->pullup_on = pullup_on;

	curr_limit = regulator_get_current_limit(jz4780->ucharger);
	printk("Before changed: the current is %d\n", curr_limit);

	insert = get_pin_status(jz4780->dete);

	/* read current frame/microframe number from DSTS register */
	dsts.d32 = readl(&dwc->dev_if.dev_global_regs->dsts);
	frame_num = dsts.b.soffn;

	if ((insert == 1) && pullup_on &&
		(frame_num == 0) &&
		(dwc->op_state == DWC2_B_PERIPHERAL)) {
		printk("upstream is not PC, it's a USB charger\n");
		regulator_set_current_limit(jz4780->ucharger, 0, 800000);
	}
	curr_limit = regulator_get_current_limit(jz4780->ucharger);
	printk("After changed: the current is %d\n", curr_limit);
}

static void set_charger_current_work(struct work_struct *work) {
	struct dwc2_jz4780 *jz4780;
	struct dwc2 *dwc;

	jz4780 = container_of(work, struct dwc2_jz4780, charger_delay_work.work);
	dwc = platform_get_drvdata(&jz4780->dwc2);

	if (dwc)
		jz4780_set_charger_current(dwc, jz4780->pullup_on);
	else		      /* re-schedule */
		schedule_delayed_work(&jz4780->charger_delay_work, msecs_to_jiffies(600));
}

static void usb_detect_work(struct work_struct *work)
{
	struct dwc2_jz4780 *jz4780;
	int insert;

	jz4780 = container_of(work, struct dwc2_jz4780, work.work);
	insert = get_pin_status(jz4780->dete);

	pr_info("USB %s\n", insert ? "connect" : "disconnect");
	jz4780_usb_phy_switch(jz4780, insert);

	if (!IS_ERR(jz4780->ucharger)) {
		if (insert) {
			schedule_delayed_work(&jz4780->charger_delay_work,
					msecs_to_jiffies(600));
		} else {
			regulator_set_current_limit(jz4780->ucharger, 0, 400000);
			printk("Now recovery 400mA\n");
		}
	}
	enable_irq(jz4780->irq);
}

static irqreturn_t usb_detect_interrupt(int irq, void *dev_id)
{
	struct dwc2_jz4780 *jz4780 = (struct dwc2_jz4780 *)dev_id;

	disable_irq_nosync(irq);
	schedule_delayed_work(&jz4780->work, msecs_to_jiffies(100));

	return IRQ_HANDLED;
}

static void usb_cpm_init(void) {
	unsigned int ref_clk_div = CONFIG_EXTAL_CLOCK / 24;
	unsigned int usbpcr1;

	/* select dwc otg */
	cpm_set_bit(USBPCR1_USB_SEL, CPM_USBPCR1);

	/* select utmi data bus width of port0 to 16bit/30M */
	cpm_set_bit(USBPCR1_WORD_IF0, CPM_USBPCR1);

	usbpcr1 = cpm_inl(CPM_USBPCR1);
	usbpcr1 &= ~(0x3 << 24);
	usbpcr1 |= (ref_clk_div << 24);
	cpm_outl(usbpcr1, CPM_USBPCR1);

	/* fil */
	cpm_outl(0, CPM_USBVBFIL);

	/* rdt */
	cpm_outl(0x96, CPM_USBRDT);

	/* rdt - filload_en */
	cpm_set_bit(USBRDT_VBFIL_LD_EN, CPM_USBRDT);

	/* TXRISETUNE & TXVREFTUNE. */
	cpm_outl(0x3f, CPM_USBPCR);
	cpm_outl(0x35, CPM_USBPCR);

	/* enable tx pre-emphasis */
	cpm_set_bit(USBPCR_TXPREEMPHTUNE, CPM_USBPCR);

	/* OTGTUNE adjust */
	cpm_outl(7 << 14, CPM_USBPCR);
}

static u64 dwc2_jz4780_dma_mask = DMA_BIT_MASK(32);

static int dwc2_jz4780_probe(struct platform_device *pdev) {
	struct platform_device	*dwc2;
	struct dwc2_jz4780	*jz4780;
	int			ret = -ENOMEM;

	jz4780 = kzalloc(sizeof(*jz4780), GFP_KERNEL);
	if (!jz4780) {
		dev_err(&pdev->dev, "not enough memory\n");
		goto err0;
	}

	/*
	 * Right now device-tree probed devices don't get dma_mask set.
	 * Since shared usb code relies on it, set it here for now.
	 * Once we move to full device tree support this will vanish off.
	 */
	if (!pdev->dev.dma_mask)
		pdev->dev.dma_mask = &dwc2_jz4780_dma_mask;

	platform_set_drvdata(pdev, jz4780);

	dwc2 = &jz4780->dwc2;
	dwc2->name = "dwc2";
	dwc2->id = -1;
	device_initialize(&dwc2->dev);
	dma_set_coherent_mask(&dwc2->dev, pdev->dev.coherent_dma_mask);
	dwc2->dev.parent = &pdev->dev;
	dwc2->dev.dma_mask = pdev->dev.dma_mask;
	dwc2->dev.dma_parms = pdev->dev.dma_parms;

	spin_lock_init(&jz4780->lock);
	mutex_init(&jz4780->mutex);
	jz4780->dev	= &pdev->dev;
	jz4780->dete = &dete_pin;

	jz4780->clk = clk_get(NULL, OTG_CLK_NAME);
	if (IS_ERR(jz4780->clk)) {
		dev_err(&pdev->dev, "clk gate get error\n");
		goto err2;
	}
	clk_enable(jz4780->clk);

#ifdef CONFIG_REGULATOR
	jz4780->vbus = regulator_get(NULL, VBUS_REG_NAME);

	if (IS_ERR(jz4780->vbus)) {
		dev_err(&pdev->dev, "regulator %s get error\n", VBUS_REG_NAME);
		goto err3;
	}
	if (regulator_is_enabled(jz4780->vbus))
		regulator_disable(jz4780->vbus);

	jz4780->ucharger = regulator_get(NULL, UCHARGER_REG_NAME);

	if (IS_ERR(jz4780->ucharger)) {
		dev_err(&pdev->dev, "regulator %s get error\n", UCHARGER_REG_NAME);
		goto err4;
	} else {
		INIT_DELAYED_WORK(&jz4780->charger_delay_work, set_charger_current_work);
	}
#else
#error DWC OTG driver can NOT work without regulator!
#endif

	if (gpio_request_one(jz4780->dete->num,
				GPIOF_DIR_IN, "usb-charger-detect")) {
		dev_err(&pdev->dev, "OTG detect pin is busy!\n");
		goto err5;
	} else {
		int ret;

		dev_info(&pdev->dev, "request GPIO_USB_DETE: %d\n", jz4780->dete->num);

		ret = request_irq(gpio_to_irq(jz4780->dete->num),
				usb_detect_interrupt,
				IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
				"usb-detect", jz4780);
		if (ret) {
			dev_err(&pdev->dev, "request usb-detect fail\n");
			goto err6;
		} else {
			jz4780->irq = gpio_to_irq(jz4780->dete->num);
			disable_irq_nosync(jz4780->irq);
			INIT_DELAYED_WORK(&jz4780->work, usb_detect_work);
		}
	}

	usb_cpm_init();

	//jz4780_usb_set_device_only_mode();
	jz4780_usb_set_dual_mode();

	jz4780_usb_phy_init(jz4780);
	jz4780_usb_phy_switch(jz4780, 1);

	/*
	 * Close VBUS detect in DWC-OTG PHY.
	 */
	*(unsigned int*)0xb3500000 |= 0xc;

	ret = platform_device_add_resources(dwc2, pdev->resource,
					pdev->num_resources);
	if (ret) {
		dev_err(&pdev->dev, "couldn't add resources to dwc2 device\n");
		goto err7;
	}

	ret = platform_device_add(dwc2);
	if (ret) {
		dev_err(&pdev->dev, "failed to register dwc2 device\n");
		goto err7;
	}

	enable_irq(jz4780->irq);

	return 0;

err7:
	free_irq(gpio_to_irq(jz4780->dete->num), jz4780);

err6:
	gpio_free(jz4780->dete->num);

err5:
	regulator_put(jz4780->ucharger);

err4:
	regulator_put(jz4780->vbus);

err3:
	clk_disable(jz4780->clk);
	clk_put(jz4780->clk);

err2:
	platform_device_put(dwc2);
err1:
	kfree(jz4780);
err0:
	return ret;
}

static int dwc2_jz4780_remove(struct platform_device *pdev) {
	struct dwc2_jz4780	*jz4780 = platform_get_drvdata(pdev);

	free_irq(jz4780->irq, jz4780);
	gpio_free(jz4780->dete->num);

	regulator_put(jz4780->ucharger);
	regulator_put(jz4780->vbus);

	clk_disable(jz4780->clk);
	clk_put(jz4780->clk);

	platform_device_unregister(&jz4780->dwc2);
	kfree(jz4780);

	return 0;
}


static struct platform_driver dwc2_jz4780_driver = {
	.probe		= dwc2_jz4780_probe,
	.remove		= dwc2_jz4780_remove,
	.driver		= {
		.name	= "jz4780-dwc2",
		.owner =  THIS_MODULE,
	},
};


static int __init dwc2_jz4780_init(void)
{
	return platform_driver_register(&dwc2_jz4780_driver);
}

static void __exit dwc2_jz4780_exit(void)
{
	platform_driver_unregister(&dwc2_jz4780_driver);
}

module_init(dwc2_jz4780_init);
module_exit(dwc2_jz4780_exit);

MODULE_ALIAS("platform:jz4780-dwc2");
MODULE_AUTHOR("Lutts Cao <slcao@ingenic.cn>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("DesignWare USB2.0 JZ4780 Glue Layer");
