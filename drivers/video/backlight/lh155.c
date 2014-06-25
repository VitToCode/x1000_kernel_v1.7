/*
 *  LCD control code for LH155
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


static void lh155_on(struct lh155_data *dev)
{
}

static void lh155_off(struct lh155_data *dev)
{
	mdelay(30);
}

static int lh155_set_power(struct lcd_device *lcd, int power)
{
	return 0;
}

static int lh155_get_power(struct lcd_device *lcd)
{
	return 0;
}

static int lh155_set_mode(struct lcd_device *lcd, struct fb_videomode *mode)
{
	return 0;
}

static struct lcd_ops lh155_ops = {
	.set_power = lh155_set_power,
	.get_power = lh155_get_power,
	.set_mode = lh155_set_mode,
};

static int __devinit lh155_probe(struct platform_device *pdev)
{

	int ret = 0;
	printk("==============%s, %d\n", __func__, __LINE__);
#define GPIO_LCD_BLK        GPIO_PC(17)
#define GPIO_LCD_RST        GPIO_PA(12)
	ret = gpio_request(GPIO_LCD_RST, "lcd rst");
	if (ret) {
		printk(KERN_ERR "can's request lcd rst\n");
		return ret;
	}
	ret = gpio_request(GPIO_LCD_BLK, "lcd blk");
	if (ret) {
		printk(KERN_ERR "can's request lcd rst\n");
		return ret;
	}

	gpio_direction_output(GPIO_LCD_BLK, 1);
	gpio_direction_output(GPIO_LCD_RST, 1);
	mdelay(300);
	gpio_direction_output(GPIO_LCD_RST, 0);  //reset active low
	mdelay(10);
	gpio_direction_output(GPIO_LCD_RST, 1);
	mdelay(5);
	return 0;
}

static int __devinit lh155_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM
static int lh155_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	return 0;
}

static int lh155_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define lh155_suspend	NULL
#define lh155_resume	NULL
#endif

static struct platform_driver lh155_driver = {
	.driver		= {
		.name	= "lh155-lcd",
		.owner	= THIS_MODULE,
	},
	.probe		= lh155_probe,
	.remove		= lh155_remove,
	.suspend	= lh155_suspend,
	.resume		= lh155_resume,
};

static int __init lh155_init(void)
{
	return platform_driver_register(&lh155_driver);
}
module_init(lh155_init);

static void __exit lh155_exit(void)
{
	platform_driver_unregister(&lh155_driver);
}
module_exit(lh155_exit);

MODULE_DESCRIPTION("lh155 lcd panel driver");
MODULE_LICENSE("GPL");
