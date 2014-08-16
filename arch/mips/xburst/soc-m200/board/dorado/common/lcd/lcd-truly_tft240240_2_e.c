/*
 * Copyright (c) 2014 Engenic Semiconductor Co., Ltd.
 *              http://www.ingenic.com/
 *
 * JZ-M200 orion board lcd setup routines.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mm.h>
#include <linux/console.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/pwm_backlight.h>
#include <linux/lcd.h>
#include <linux/regulator/consumer.h>
#include <mach/jzfb.h>

#include "board.h"

/******GPIO PIN************/
#undef GPIO_LCD_CS
#undef GPIO_LCD_RD
#undef GPIO_BL_PWR_EN

#define GPIO_LCD_CS            GPIO_PC(14)
#define GPIO_LCD_RD            GPIO_PC(17)
#define GPIO_BL_PWR_EN         GPIO_PC(18)

/*ifdef is 18bit,6-6-6 ,ifndef default 5-6-6*/
//#define CONFIG_SLCD_TRULY_18BIT

#ifdef	CONFIG_SLCD_TRULY_18BIT
static int slcd_inited = 1;
#else
static int slcd_inited = 0;
#endif

struct truly_tft240240_power{
	struct regulator *vlcdio;
	struct regulator *vlcdvcc;
	int inited;
};

static struct truly_tft240240_power lcd_power = {
	NULL,
	NULL,
	0
};

int truly_tft240240_power_init(struct lcd_device *ld)
{
	int ret ;
	printk("======truly_tft240240_power_init==============\n");

	ret = gpio_request(GPIO_LCD_RST, "lcd rst");
	if (ret) {
		printk(KERN_ERR "can's request lcd rst\n");
		return ret;
	}

	ret = gpio_request(GPIO_LCD_CS, "lcd cs");
	if (ret) {
		printk(KERN_ERR "can's request lcd cs\n");
		return ret;
	}

	ret = gpio_request(GPIO_LCD_RD, "lcd rd");
	if (ret) {
		printk(KERN_ERR "can's request lcd rd\n");
		return ret;
	}

	printk("set lcd_power.inited  =======1 \n");
	lcd_power.inited = 1;
	return 0;
}

int truly_tft240240_power_reset(struct lcd_device *ld)
{
	if (!lcd_power.inited)
		return -EFAULT;
	gpio_direction_output(GPIO_LCD_RST, 0);
	mdelay(20);
	gpio_direction_output(GPIO_LCD_RST, 1);
	mdelay(10);

	return 0;
}

int truly_tft240240_power_on(struct lcd_device *ld, int enable)
{
	if (!lcd_power.inited && truly_tft240240_power_init(ld))
		return -EFAULT;

	if (enable) {
		gpio_direction_output(GPIO_LCD_CS, 1);
		gpio_direction_output(GPIO_LCD_RD, 1);

		truly_tft240240_power_reset(ld);

		mdelay(5);
		gpio_direction_output(GPIO_LCD_CS, 0);

	} else {
		gpio_direction_output(GPIO_BL_PWR_EN, 0);
		mdelay(5);
		gpio_direction_output(GPIO_LCD_CS, 0);
		gpio_direction_output(GPIO_LCD_RD, 0);
		gpio_direction_output(GPIO_LCD_RST, 0);
		gpio_direction_output(GPIO_LCD_DISP, 0);
		slcd_inited = 0;
	}
	return 0;
}

struct lcd_platform_data truly_tft240240_pdata = {
	.reset    = truly_tft240240_power_reset,
	.power_on = truly_tft240240_power_on,
};

/* LCD Panel Device */
struct platform_device truly_tft240240_device = {
	.name		= "truly_tft240240_slcd",
	.dev		= {
		.platform_data	= &truly_tft240240_pdata,
	},
};

static struct smart_lcd_data_table truly_tft240240_data_table[] = {
	/* LCD init code */
	{0, 0x01},  //soft reset, 120 ms = 120 000 us
	{2, 120000},
	{0, 0x11},
	{2, 5000},	  /* sleep out 5 ms  */

	{0, 0x36},
#ifdef	CONFIG_TRULY_240X240_ROTATE_180
	/*{0x36, 0xc0, 2, 0}, //40*/
	{1, 0xd0}, //40
#else
	{1, 0x00}, //40
#endif

	{0, 0x2a},
	{1, 0x00},
	{1, 0x00},
	{1, 0x00},
	{1, 0xef},

	{0, 0x2b},
	{1, 0x00},
	{1, 0x00},
	{1, 0x00},
	{1, 0xef},


	{0, 0x3a},
#if defined(CONFIG_SLCD_TRULY_18BIT)  //if 18bit/pixel unusual. try to use 16bit/pixel
	{1, 0x06}, //6-6-6
#else
	{1, 0x05}, //5-6-5
#endif
//	{1, 0x55},

	{0, 0xb2},
	{1, 0x7f},
	{1, 0x7f},
	{1, 0x01},
	{1, 0xde},
	{1, 0x33},

	{0, 0xb3},
	{1, 0x10},
	{1, 0x05},
	{1, 0x0f},

	{0, 0xb4},
	{1, 0x0b},

	{0, 0xb7},
	{1, 0x35},

	{0, 0xbb},
	{1, 0x28}, //23

	{0, 0xbc},
	{1, 0xec},

	{0, 0xc0},
	{1, 0x2c},

	{0, 0xc2},
	{1, 0x01},

	{0, 0xc3},
	{1, 0x1e}, //14

	{0, 0xc4},
	{1, 0x20},

	{0, 0xc6},
	{1, 0x14},

	{0, 0xd0},
	{1, 0xa4},
	{1, 0xa1},

	{0, 0xe0},
	{1, 0xd0},
	{1, 0x00},
	{1, 0x00},
	{1, 0x08},
	{1, 0x07},
	{1, 0x05},
	{1, 0x29},
	{1, 0x54},
	{1, 0x41},
	{1, 0x3c},
	{1, 0x17},
	{1, 0x15},
	{1, 0x1a},
	{1, 0x20},

	{0, 0xe1},
	{1, 0xd0},
	{1, 0x00},
	{1, 0x00},
	{1, 0x08},
	{1, 0x07},
	{1, 0x04},
	{1, 0x29},
	{1, 0x44},
	{1, 0x42},
	{1, 0x3b},
	{1, 0x16},
	{1, 0x15},
	{1, 0x1b},
	{1, 0x1f},

	{0, 0x35}, // TE on
	{1, 0x00}, // TE mode: 0, mode1; 1, mode2
//	{0, 0x34}, // TE off

	{0, 0x29}, //Display ON

	/* set window size*/
//	{0, 0xcd},
	{0, 0x2a},
	{1, 0},
	{1, 0},
	{1, (239>> 8) & 0xff},
	{1, 239 & 0xff},
#ifdef	CONFIG_TRULY_240X240_ROTATE_180
	{0, 0x2b},
	{1, ((320-240)>>8)&0xff},
	{1, ((320-240)>>0)&0xff},
	{1, ((320-1)>>8) & 0xff},
	{1, ((320-1)>>0) & 0xff},
#else
	{0, 0x2b},
	{1, 0},
	{1, 0},
	{1, (239>> 8) & 0xff},
	{1, 239 & 0xff},
#endif
};

unsigned long truly_cmd_buf[]= {
	0x2C2C2C2C,
};

struct fb_videomode jzfb0_videomode = {
	.name = "240x240",
	.refresh = 60,
	.xres = 240,
	.yres = 240,
	.pixclock = KHZ2PICOS(30000),
	.left_margin = 0,
	.right_margin = 0,
	.upper_margin = 0,
	.lower_margin = 0,
	.hsync_len = 0,
	.vsync_len = 0,
	.sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};


struct jzfb_platform_data jzfb_pdata = {
	.num_modes = 1,
	.modes = &jzfb0_videomode,
	.lcd_type = LCD_TYPE_SLCD,
	.bpp    = 16,
	.width = 31,
	.height = 31,
	.pinmd  = 0,

	.smart_config.rsply_cmd_high       = 0,
	.smart_config.csply_active_high    = 0,
	.smart_config.newcfg_fmt_conv =  1,
	/* write graphic ram command, in word, for example 8-bit bus, write_gram_cmd=C3C2C1C0. */
	.smart_config.write_gram_cmd = truly_cmd_buf,
	.smart_config.length_cmd = ARRAY_SIZE(truly_cmd_buf),
	.smart_config.bus_width = 8,
	.smart_config.length_data_table =  ARRAY_SIZE(truly_tft240240_data_table),
	.smart_config.data_table = truly_tft240240_data_table,
	.dither_enable = 0,
};
/**************************************************************************************************/
#ifdef CONFIG_BACKLIGHT_PWM
static int backlight_init(struct device *dev)
{
	int ret;
	ret = gpio_request(GPIO_LCD_PWM, "Backlight");
	if (ret) {
		printk(KERN_ERR "failed to request GPF for PWM-OUT1\n");
		return ret;
	}

	ret = gpio_request(GPIO_BL_PWR_EN, "BL PWR");
	if (ret) {
		printk(KERN_ERR "failed to reqeust BL PWR\n");
		return ret;
	}
	gpio_direction_output(GPIO_BL_PWR_EN, 1);
	return 0;
}

static int backlight_notify(struct device *dev, int brightness)
{
	if (brightness)
		gpio_direction_output(GPIO_BL_PWR_EN, 1);
	else
		gpio_direction_output(GPIO_BL_PWR_EN, 0);

	return brightness;
}

static void backlight_exit(struct device *dev)
{
	gpio_free(GPIO_LCD_PWM);
}

static struct platform_pwm_backlight_data backlight_data = {
	.pwm_id		= 1,
	.max_brightness	= 255,
	.dft_brightness	= 120,
	.pwm_period_ns	= 30000,
	.init		= backlight_init,
	.exit		= backlight_exit,
	.notify		= backlight_notify,
};

struct platform_device backlight_device = {
	.name		= "pwm-backlight",
	.dev		= {
		.platform_data	= &backlight_data,
	},
};
#endif
