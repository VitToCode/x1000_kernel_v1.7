/*
 *  LCD control code for byd_9177aa
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

#define  MIPI_RST_N   GPIO_PC(3)
static void byd_9177aa_on(struct byd_9177aa_data *dev)
{

}

static void byd_9177aa_off(struct byd_9177aa_data *dev)
{

}

static int byd_9177aa_set_power(struct lcd_device *lcd, int power)
{
	return 0;
}

static int byd_9177aa_get_power(struct lcd_device *lcd)
{
	return 0;
}

static int byd_9177aa_set_mode(struct lcd_device *lcd, struct fb_videomode *mode)
{
	return 0;
}

static struct lcd_ops byd_9177aa_ops = {
	.set_power = byd_9177aa_set_power,
	.get_power = byd_9177aa_get_power,
	.set_mode = byd_9177aa_set_mode,
};

static int __devinit byd_9177aa_probe(struct platform_device *pdev)
{
	int ret;
	printk("---------------%s\n", __func__);
	ret = gpio_request(MIPI_RST_N, "lcd mipi panel rst");
	if (ret) {
		printk(KERN_ERR "can's request lcd panel rst\n");
		return ret;
	}
	ret = gpio_request(GPIO_PC(2), "lcd mipi panel avcc");
	if (ret) {
		printk(KERN_ERR "can's request lcd panel rst\n");
		return ret;
	}
	ret = gpio_request(GPIO_PE(1), "lcd mipi panel pwm");
	if (ret) {
		printk(KERN_ERR "can's request lcd panel rst\n");
		return ret;
	}
	gpio_direction_output(GPIO_PC(2), 1); /* 2.8v en*/
	gpio_direction_output(GPIO_PE(1), 1); /* pwm en*/
	gpio_direction_output(MIPI_RST_N, 0);
	mdelay(10);

	gpio_direction_output(MIPI_RST_N, 1);
	mdelay(10);

	return 0;
}

static int __devinit byd_9177aa_remove(struct platform_device *pdev)
{
	return 0;
}

#ifdef CONFIG_PM
static int byd_9177aa_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	return 0;
}

static int byd_9177aa_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define byd_9177aa_suspend	NULL
#define byd_9177aa_resume	NULL
#endif

static struct platform_driver byd_9177aa_driver = {
	.driver		= {
		.name	= "byd_9177aa-lcd",
		.owner	= THIS_MODULE,
	},
	.probe		= byd_9177aa_probe,
	.remove		= byd_9177aa_remove,
	.suspend	= byd_9177aa_suspend,
	.resume		= byd_9177aa_resume,
};

static int __init byd_9177aa_init(void)
{
	return platform_driver_register(&byd_9177aa_driver);
}
module_init(byd_9177aa_init);

static void __exit byd_9177aa_exit(void)
{
	platform_driver_unregister(&byd_9177aa_driver);
}
module_exit(byd_9177aa_exit);

MODULE_DESCRIPTION("byd_9177aa lcd panel driver");
MODULE_LICENSE("GPL");
