/*
 *  LCD control code for hhx070ml208cp2
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
#include <linux/earlysuspend.h>

#include <linux/hhx070ml208cp21.h>

#define POWER_IS_ON(pwr)        ((pwr) <= FB_BLANK_NORMAL)

#define DRIVER_NAME "jz4780-lcd"

#define eprint(f, arg...)		pr_err(DRIVER_NAME ": " f , ## arg)
#define wprint(f, arg...)		pr_warning(DRIVER_NAME ": " f , ## arg)
#define kprint(f, arg...)		pr_info(DRIVER_NAME ": " f , ## arg)
#define dprint(f, arg...)		pr_debug(DRIVER_NAME ": " f , ## arg)


struct hhx070ml208cp21_data {
	int lcd_power;
	struct lcd_device *lcd;
	struct platform_hhx070ml208cp21_data *pdata;
	struct regulator *lcd_vcc_reg;
#ifdef  CONFIG_Q8
#else
#ifdef CONFIG_HAS_EARLYSUSPEND
        struct early_suspend bk_early_suspend;
#endif
#endif
};
#ifdef  CONFIG_Q8
#else
static int is_bootup = 1;
#endif
static void hhx070ml208cp21_on(struct hhx070ml208cp21_data *dev)
{
#ifdef  CONFIG_Q8
#else
    if (is_bootup) {
        regulator_enable(dev->lcd_vcc_reg);
        is_bootup = 0;
        if (dev->pdata->notify_on)
            dev->pdata->notify_on(1);
    }
	dev->lcd_power = 1;
#endif
#ifdef  CONFIG_Q8
    regulator_enable(dev->lcd_vcc_reg);
    mdelay(20);

	if (dev->pdata->gpio_rest) {
		gpio_direction_output(dev->pdata->gpio_rest, 0);
		mdelay(100);
		gpio_direction_output(dev->pdata->gpio_rest, 1);
	}
#endif
}

static void hhx070ml208cp21_off(struct hhx070ml208cp21_data *dev)
{
        dev->lcd_power = 0;
#ifdef  CONFIG_Q8

        regulator_disable(dev->lcd_vcc_reg);
       // mdelay(20);
#endif
}

static int hhx070ml208cp21_set_power(struct lcd_device *lcd, int power)
{
	struct hhx070ml208cp21_data *dev= lcd_get_data(lcd);

	if (POWER_IS_ON(power) && !POWER_IS_ON(dev->lcd_power))
		hhx070ml208cp21_on(dev);

	if (!POWER_IS_ON(power) && POWER_IS_ON(dev->lcd_power))
		hhx070ml208cp21_off(dev);

	dev->lcd_power = power;

	return 0;
}

static int hhx070ml208cp21_get_power(struct lcd_device *lcd)
{
	struct hhx070ml208cp21_data *dev= lcd_get_data(lcd);

	return dev->lcd_power;
}

static int hhx070ml208cp21_set_mode(struct lcd_device *lcd, struct fb_videomode *mode)
{
	return 0;
}

static struct lcd_ops hhx070ml208cp21_ops = {
	.set_power = hhx070ml208cp21_set_power,
	.get_power = hhx070ml208cp21_get_power,
	.set_mode = hhx070ml208cp21_set_mode,
};
#ifdef  CONFIG_Q8
#else
#ifdef CONFIG_HAS_EARLYSUSPEND
static void bk_e_suspend(struct early_suspend *h)
{
        struct hhx070ml208cp21_data *dev = container_of(h, struct hhx070ml208cp21_data, bk_early_suspend);
        if (dev->pdata->notify_on)
            dev->pdata->notify_on(0);

        msleep(500);

        dev->lcd_power = 0;

        regulator_disable(dev->lcd_vcc_reg);
        mdelay(20);

        //if (dev->pdata->notify_on)
        //    dev->pdata->notify_on(0);
}

static void bk_l_resume(struct early_suspend *h)
{
        struct hhx070ml208cp21_data *dev = container_of(h, struct hhx070ml208cp21_data, bk_early_suspend);
        
        dev->lcd_power = 1;
        regulator_enable(dev->lcd_vcc_reg);
        mdelay(100);

        if (dev->pdata->gpio_rest) {
                gpio_direction_output(dev->pdata->gpio_rest, 0);
                mdelay(100);
                gpio_direction_output(dev->pdata->gpio_rest, 1);
        }

        if (dev->pdata->notify_on)
           dev->pdata->notify_on(1);
}
#endif
#endif
static int __devinit hhx070ml208cp21_probe(struct platform_device *pdev)
{
	int ret;
	struct hhx070ml208cp21_data *dev;

	kprint("%s", __func__);

	dev = kzalloc(sizeof(struct hhx070ml208cp21_data), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	dev->pdata = pdev->dev.platform_data;

	dev_set_drvdata(&pdev->dev, dev);

	
#ifdef  CONFIG_Q8
	dev->lcd_vcc_reg = regulator_get(NULL, "vq8lcd");
#else
	dev->lcd_vcc_reg = regulator_get(NULL, "vlcd");
#endif
	if (IS_ERR(dev->lcd_vcc_reg)) {
		dev_err(&pdev->dev, "failed to get regulator vlcd\n");
		return PTR_ERR(dev->lcd_vcc_reg);
	}
/*
	if(gpio_request(dev->pdata->gpio_backlight_en, "bk_en")){
		 dev_err(&pdev->dev,"request gc0308 gpio rst and en fail\n");
                return -1;
        }

	gpio_set_value(dev->pdata->gpio_backlight_en, 1);
*/

	if (dev->pdata->gpio_rest)
		gpio_request(dev->pdata->gpio_rest, "reset");

	dev->lcd = lcd_device_register("hhx070ml208cp21-lcd", &pdev->dev,
				       dev, &hhx070ml208cp21_ops);

	dev->lcd_power = FB_BLANK_POWERDOWN;
	hhx070ml208cp21_set_power(dev->lcd, FB_BLANK_UNBLANK);

	if (IS_ERR(dev->lcd)) {
		ret = PTR_ERR(dev->lcd);
		dev->lcd = NULL;
		dev_info(&pdev->dev, "lcd device register error: %d\n", ret);
	} else {
		dev_info(&pdev->dev, "lcd device register success\n");
	}
#ifdef  CONFIG_Q8
#else

#ifdef CONFIG_HAS_EARLYSUSPEND
    dev->bk_early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;//EARLY_SUSPEND_LEVEL_STOP_DRAWING;
    dev->bk_early_suspend.suspend = bk_e_suspend;
    dev->bk_early_suspend.resume = bk_l_resume;
#endif

    register_early_suspend(&dev->bk_early_suspend);
#endif
	return 0;
}

static int __devinit hhx070ml208cp21_remove(struct platform_device *pdev)
{
	struct hhx070ml208cp21_data *dev = dev_get_drvdata(&pdev->dev);

	lcd_device_unregister(dev->lcd);
	hhx070ml208cp21_off(dev);

	regulator_put(dev->lcd_vcc_reg);

	if (dev->pdata->gpio_rest)
		gpio_free(dev->pdata->gpio_rest);

	dev_set_drvdata(&pdev->dev, NULL);
	kfree(dev);

	return 0;
}

#ifdef CONFIG_PM
static int hhx070ml208cp21_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	return 0;
}

static int hhx070ml208cp21_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define hhx070ml208cp21_suspend	NULL
#define hhx070ml208cp21_resume	NULL
#endif

static struct platform_driver hhx070ml208cp21_driver = {
	.driver		= {
		.name	= "hhx070ml208cp21-lcd",
		.owner	= THIS_MODULE,
	},
	.probe		= hhx070ml208cp21_probe,
	.remove		= hhx070ml208cp21_remove,
	.suspend	= hhx070ml208cp21_suspend,
	.resume		= hhx070ml208cp21_resume,
};

static int __init hhx070ml208cp21_init(void)
{
	return platform_driver_register(&hhx070ml208cp21_driver);
}
module_init(hhx070ml208cp21_init);

static void __exit hhx070ml208cp21_exit(void)
{
	platform_driver_unregister(&hhx070ml208cp21_driver);
}
module_exit(hhx070ml208cp21_exit);

MODULE_DESCRIPTION("hhx070ml208cp21 lcd panel driver");
MODULE_LICENSE("GPL");

