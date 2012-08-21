/*
 * act8600_charger.c - ACT8600 Charger driver, based on ACT8600 mfd driver.
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 * Author: Large Dipper <ykli@ingenic.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/power_supply.h>
#include <linux/power/jz4780-battery.h>
#include <linux/mfd/act8600-private.h>
#include <linux/mfd/pmu-common.h>

static enum power_supply_property act8600_power_properties[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static char *supply_list[] = {
	"battery",
};

struct act8600_charger {
	struct act8600 *iodev;
	struct delayed_work work;
	int irq;
	
	struct power_supply usb;
	struct power_supply ac;

#define USB_ONLINE	(1 << 0)
#define AC_ONLINE	(1 << 1)
	unsigned int	status;
};

static unsigned int act8600_update_status(struct act8600_charger *charger)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	struct jz_battery *jz_battery;
	struct act8600 *iodev = charger->iodev;
	unsigned char chgst,intr1,otg_con;
	unsigned int status_pre;

	jz_battery = container_of(psy, struct jz_battery, battery);
	status_pre = charger->status;
	charger->status = 0;

#if CONFIG_CHARGER_HAS_AC
	act8600_read_reg(iodev->client, APCH_INTR1, &intr1);

	if (((intr1 & INDAT) != 0)) {
		charger->status |= AC_ONLINE;
		act8600_write_reg(iodev->client, APCH_INTR1, INSTAT);
		act8600_write_reg(iodev->client, APCH_INTR2, INDIS);
	} else {
		act8600_write_reg(iodev->client, APCH_INTR1, INSTAT);
		act8600_write_reg(iodev->client, APCH_INTR2, INCON);
	}
#endif
#if CONFIG_CHARGER_HAS_USB
	act8600_read_reg(iodev->client, OTG_CON, &otg_con);

	if ((otg_con & VBUSDAT) && !(otg_con & ONQ1)) {
		charger->status |= USB_ONLINE;
		act8600_write_reg(iodev->client, OTG_INTR, INVBUSF);
	} else {
		act8600_write_reg(iodev->client, OTG_INTR, INVBUSR);
	}
#endif

	act8600_read_reg(iodev->client, APCH_STAT, &chgst);

	switch (chgst & CSTATE_MASK) {
	case CSTATE_EOC:
		jz_battery->status = POWER_SUPPLY_STATUS_FULL;
		break;
	case CSTATE_PRE:
	case CSTATE_CHAGE:
		jz_battery->status = POWER_SUPPLY_STATUS_CHARGING;
		break;
	case CSTATE_SUSPEND:		
		jz_battery->status = POWER_SUPPLY_STATUS_NOT_CHARGING;
		break;
	}

	return status_pre ^ charger->status;
}

static void act8600_charger_work(struct work_struct *work)
{
	unsigned int changed_psy;
	struct act8600_charger *charger;

	charger = container_of(work, struct act8600_charger, work.work);	
	changed_psy = act8600_update_status(charger);

	if (changed_psy & USB_ONLINE)
		power_supply_changed(&charger->usb);
	if (changed_psy & AC_ONLINE)
		power_supply_changed(&charger->ac);

	enable_irq(charger->irq);
}

static irqreturn_t act8600_charger_irq(int irq, void *data)
{
	struct act8600_charger *charger = data;

	disable_irq_nosync(charger->irq);

	cancel_delayed_work(&charger->work);
	schedule_delayed_work(&charger->work, 0);

	return IRQ_HANDLED;
}

static int act8600_get_property(struct power_supply *psy, 
		enum power_supply_property psp, 
		union power_supply_propval *val)
{
	struct act8600_charger *charger;

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		if (psy->type == POWER_SUPPLY_TYPE_MAINS) {
			charger = container_of(psy,
					       struct act8600_charger, ac);
			val->intval = (charger->status & AC_ONLINE ? 1 : 0);
		} else if(psy->type == POWER_SUPPLY_TYPE_USB) {
			charger = container_of(psy,
					       struct act8600_charger, usb);
			val->intval = (charger->status & USB_ONLINE ? 1 : 0);
		} else
			val->intval = 0;
		break;
	default:
		return -EINVAL;
	}
	
	return 0;
}

static void power_supply_init(struct act8600_charger *charger)
{
#define DEF_POWER(PSY, NAME, TYPE)						\
	charger->PSY.name = NAME;						\
	charger->PSY.type = TYPE;						\
	charger->PSY.supplied_to = supply_list;					\
	charger->PSY.num_supplicants = ARRAY_SIZE(supply_list);			\
	charger->PSY.properties = act8600_power_properties;			\
	charger->PSY.num_properties = ARRAY_SIZE(act8600_power_properties);	\
	charger->PSY.get_property = act8600_get_property

	DEF_POWER(usb, "usb", POWER_SUPPLY_TYPE_USB);
	DEF_POWER(ac, "ac", POWER_SUPPLY_TYPE_MAINS);
#undef DEF_POWER
}

static int charger_init(struct act8600_charger *charger)
{
	struct act8600 *iodev = charger->iodev;
	unsigned char intr1, intr2, otgcon;

	act8600_read_reg(iodev->client, APCH_INTR1, &intr1);
	act8600_write_reg(iodev->client, APCH_INTR1,
			  intr1 | CHGSTAT | INSTAT | TEMPSTAT);

	act8600_read_reg(iodev->client, APCH_INTR2, &intr2);
	act8600_write_reg(iodev->client, APCH_INTR2,
			  intr2 | CHGEOCOUT | CHGEOCIN
			  | INDIS | INCON | TEMPOUT | TEMPIN);
	
	act8600_write_reg(iodev->client, OTG_INTR, INVBUSF | INVBUSR);

	act8600_read_reg(iodev->client, OTG_CON, &otgcon);
	act8600_write_reg(iodev->client, OTG_CON, otgcon | VBUSSTAT);

	return 0;
}

static int __devinit act8600_charger_probe(struct platform_device *pdev)
{
	struct act8600 *iodev = dev_get_drvdata(pdev->dev.parent);
	struct pmu_platform_data *pdata = dev_get_platdata(iodev->dev);
	struct act8600_charger *charger;
	int ret = 0;

	if (!pdata) {
		dev_err(&pdev->dev, "No platform_data supplied\n");
		return -ENXIO;
	}

	charger = kzalloc(sizeof(struct act8600_charger), GFP_KERNEL);
	if (!charger) {
		dev_err(&pdev->dev, "Failed to allocate driver structure\n");
		return -ENOMEM;
	}

	charger->irq = gpio_to_irq(pdata->gpio);
	if (charger->irq < 0) {
		ret = charger->irq;
		dev_err(&pdev->dev, "Failed to get platform irq: %d\n", ret);
		goto err_free;
	}

	ret = request_irq(charger->irq, act8600_charger_irq, 0, pdev->name,
			  charger);
	if (ret) {
		dev_err(&pdev->dev, "Failed to request irq %d\n", ret);
		goto err_free_irq;
	}
	disable_irq(charger->irq);

	charger->iodev = iodev;

	charger_init(charger);
	power_supply_init(charger);

	INIT_DELAYED_WORK(&charger->work, act8600_charger_work);
	platform_set_drvdata(pdev, charger);
	schedule_delayed_work(&charger->work, 0);

	return 0;
err_free_irq:
	free_irq(charger->irq, charger);
err_free:
	kfree(charger);
	return ret;
}

static int __devexit act8600_charger_remove(struct platform_device *pdev)
{
	struct act8600_charger *charger = platform_get_drvdata(pdev);

	cancel_delayed_work_sync(&charger->work);

	power_supply_unregister(&charger->usb);
	power_supply_unregister(&charger->ac);

	free_irq(charger->irq, charger);
	kfree(charger);

	return 0;
}

#ifdef CONFIG_PM
static int act8600_charger_suspend(struct device *dev)
{
	return 0;
}

static int act8600_charger_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops act8600_charger_pm_ops = {
	.suspend	= act8600_charger_suspend,
	.resume		= act8600_charger_resume,
};

#define ACT8600_CHARGER_PM_OPS (&act8600_charger_pm_ops)
#else
#define ACT8600_CHARGER_PM_OPS NULL
#endif

static struct platform_driver act8600_charger_driver = {
	.probe		= act8600_charger_probe,
	.remove		= __devexit_p(act8600_charger_remove),
	.driver = {
		.name = "act8600-charger",
		.owner = THIS_MODULE,
		.pm = ACT8600_CHARGER_PM_OPS,
	},
};

static int __init act8600_charger_init(void)
{
	return platform_driver_register(&act8600_charger_driver);
}
module_init(act8600_charger_init);

static void __exit act8600_charger_exit(void)
{
	platform_driver_unregister(&act8600_charger_driver);
}
module_exit(act8600_charger_exit);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Large Dipper <ykli@ingenic.cn>");
MODULE_DESCRIPTION("act8600 charger driver for JZ battery");
