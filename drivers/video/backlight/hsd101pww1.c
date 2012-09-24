/*
 *  LCD control code for HSD101PWW1
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/lcd.h>
#include <linux/fb.h>
#include <linux/regulator/consumer.h>

#include <linux/hsd101pww1.h>

#define POWER_IS_ON(pwr)        ((pwr) <= FB_BLANK_NORMAL)

#define DRIVER_NAME "jz4770-lcd"

#define eprint(f, arg...)		pr_err(DRIVER_NAME ": " f , ## arg)
#define wprint(f, arg...)		pr_warning(DRIVER_NAME ": " f , ## arg)
#define kprint(f, arg...)		pr_info(DRIVER_NAME ": " f , ## arg)
#define dprint(f, arg...)		pr_debug(DRIVER_NAME ": " f , ## arg)


struct hsd101pww1_data {
	int lcd_power;
	struct lcd_device *lcd;
	struct platform_hsd101pww1_data *pdata;
	struct regulator *lcd_vcc_reg;
};

static void hsd101pww1_on(struct hsd101pww1_data *dev)
{
    dev->lcd_power = 1;
    regulator_enable(dev->lcd_vcc_reg);

	if (dev->pdata->gpio_rest) {
		gpio_direction_output(dev->pdata->gpio_rest, 0);
		mdelay(100);
		gpio_direction_output(dev->pdata->gpio_rest, 1);
	}
}

static void hsd101pww1_off(struct hsd101pww1_data *dev)
{
        dev->lcd_power = 0;
        regulator_disable(dev->lcd_vcc_reg);
}

static int hsd101pww1_set_power(struct lcd_device *lcd, int power)
{
	struct hsd101pww1_data *dev= lcd_get_data(lcd);

	if (!power && !(dev->lcd_power)) {
                hsd101pww1_on(dev);
        } else if (power && (dev->lcd_power)) {
                hsd101pww1_off(dev);
        }
	return 0;
}

static int hsd101pww1_get_power(struct lcd_device *lcd)
{
	struct hsd101pww1_data *dev= lcd_get_data(lcd);

	return dev->lcd_power;
}

static int hsd101pww1_set_mode(struct lcd_device *lcd, struct fb_videomode *mode)
{
	return 0;
}

static struct lcd_ops hsd101pww1_ops = {
	.set_power = hsd101pww1_set_power,
	.get_power = hsd101pww1_get_power,
	.set_mode = hsd101pww1_set_mode,
};

static int __devinit hsd101pww1_probe(struct platform_device *pdev)
{
	int ret;
	struct hsd101pww1_data *dev;

	eprint("%s", __func__);

	dev = kzalloc(sizeof(struct hsd101pww1_data), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->pdata = pdev->dev.platform_data;

	dev_set_drvdata(&pdev->dev, dev);

	dev->lcd_vcc_reg = regulator_get(NULL, "vlcd");
	if (IS_ERR(dev->lcd_vcc_reg)) {
		dev_err(&pdev->dev, "failed to get regulator vlcd\n");
		return PTR_ERR(dev->lcd_vcc_reg);
	}

	if (dev->pdata->gpio_rest)
		gpio_request(dev->pdata->gpio_rest, "reset");

	hsd101pww1_on(dev);

	dev->lcd = lcd_device_register("hsd101pww1-lcd", &pdev->dev,
				       dev, &hsd101pww1_ops);

	if (IS_ERR(dev->lcd)) {
		ret = PTR_ERR(dev->lcd);
		dev->lcd = NULL;
		dev_info(&pdev->dev, "lcd device register error: %d\n", ret);
	} else {
		dev_info(&pdev->dev, "lcd device register success\n");
	}

	return 0;
}

static int __devinit hsd101pww1_remove(struct platform_device *pdev)
{
	struct hsd101pww1_data *dev = dev_get_drvdata(&pdev->dev);

	lcd_device_unregister(dev->lcd);
	hsd101pww1_off(dev);

	regulator_put(dev->lcd_vcc_reg);

	if (dev->pdata->gpio_rest)
		gpio_free(dev->pdata->gpio_rest);

	dev_set_drvdata(&pdev->dev, NULL);
	kfree(dev);

	return 0;
}

#ifdef CONFIG_PM
static int hsd101pww1_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	return 0;
}

static int hsd101pww1_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define hsd101pww1_suspend	NULL
#define hsd101pww1_resume	NULL
#endif

static struct platform_driver hsd101pww1_driver = {
	.driver		= {
		.name	= "hsd101pww1-lcd",
		.owner	= THIS_MODULE,
	},
	.probe		= hsd101pww1_probe,
	.remove		= hsd101pww1_remove,
	.suspend	= hsd101pww1_suspend,
	.resume		= hsd101pww1_resume,
};

static int __init hsd101pww1_init(void)
{
	return platform_driver_register(&hsd101pww1_driver);
}
module_init(hsd101pww1_init);

static void __exit hsd101pww1_exit(void)
{
	platform_driver_unregister(&hsd101pww1_driver);
}
module_exit(hsd101pww1_exit);

MODULE_DESCRIPTION("hsd101pww1 lcd panel driver");
MODULE_LICENSE("GPL");

