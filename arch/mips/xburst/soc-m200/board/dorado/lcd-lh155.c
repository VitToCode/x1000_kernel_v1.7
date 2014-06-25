/*
 * Copyright (c) 2012 Ingenic Semiconductor Co., Ltd.
 *              http://www.ingenic.com/
 *
 * JZ4785 dorado board lcd setup routines.
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
/*
#include <linux/digital_pulse_backlight.h>
#include <linux/at070tn93.h>
*/

#include <mach/jzfb.h>
#include "board.h"

struct dsi_cmd_packet lh155_cmd_list[] = {
	{0x39, 0x03, 0x00, 0x0, {0xf1, 0x5a, 0x5a}},
	{0x39, 0x12, 0x0, 0x0, {0xf2, 0x00, 0xd7, 0x03, 0x22, 0x23, 0x00, 0x01, 0x01, 0x12, 0x01, 0x08, 0x57, 0x00, 0x00, 0xd7, 0x22, 0x23}},
	{0x39, 0xf, 0x0, 0x0,{0xf4, 0x07, 0x00, 0x00, 0x00, 0x21, 0x4f, 0x01, 0x02, 0x2a, 0x66, 0x02, 0x2a, 0x00, 0x02}},
	{0x39, 0xb, 0x0, 0x0, {0xf5, 0x00, 0x38, 0x5c, 0x00, 0x00, 0x19, 0x00, 0x00, 0x04, 0x04}},
	{0x39, 0xa, 0x0, 0x0, {0xf6, 0x03, 0x01, 0x06, 0x00, 0x0a, 0x04, 0x0a, 0xf4, 0x06}},
	{0x39, 0x2, 0x0, 0x0, {0xf7, 0x40}},
	{0x39, 0x3, 0x0, 0x0, {0xf8, 0x33, 0x00}},
	{0x39, 0x2, 0x0, 0x0, {0x0, 0xf9, 0x00}},
	{0x39, 0x1d, 0x0, 0x0, {0xfa, 0x00, 0x04, 0x0e, 0x03, 0x0e, 0x20, 0x2a, 0x30, 0x38, 0x3a,
							0x34, 0x37, 0x44, 0x3b, 0x00, 0x0b, 0x00, 0x07, 0x0b, 0x17, 0x1b,
							0x1a, 0x22, 0x2b, 0x34, 0x40, 0x50, 0x3f}},
	{0x39, 0x1d, 0x0, 0x0, {0xfa, 0x00, 0x1b, 0x11, 0x03, 0x08, 0x16, 0x1d, 0x20, 0x29, 0x2c,
							0x29, 0x2d, 0x3d, 0x3c, 0x00, 0x0b, 0x12, 0x0e, 0x11, 0x1e, 0x22,
							0x25, 0x2e, 0x39, 0x40, 0x48, 0x40, 0x3f}},
	{0x39, 0xf, 0x0, 0x0, {0xfa, 0x00, 0x27, 0x12, 0x02, 0x04, 0x0f, 0x15, 0x17, 0x21, 0x27,
							0x24, 0x29, 0x31, 0x3c}},
	{0x39, 0x10, 0x0, 0x0, {0xfa, 0x00, 0x0c, 0x1e, 0x11, 0x17, 0x21, 0x27, 0x2b, 0x37, 0x43,
							0x49, 0x4f, 0x59, 0x3e, 0x00}},
	{0x39, 0x1, 0x0, 0x0, {0x11}},
	{0x39, 0x2, 0x0, 0x0, {0x36, 0xd8}},
	{0x39, 0x2, 0x0, 0x0, {0x3a, 0x07}},
	{0x39, 0x1, 0x0, 0x0, {0x29}},
	{0x39, 0x5, 0x0, 0x0, {0x2a, 0x00, 0x00, (239 & 0xff00) >> 8, 239 & 0xff}},
	{0x39, 0x5, 0x0, 0x0, {0x0, 0x2b, 0x00, 0x00, (239 & 0xff00) >> 8, (239 & 0xff)}},
	{0x39, 0x1, 0x0, 0x0, {0x2c}},
};

struct fb_videomode jzfb_videomode = {
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
	.sync = ~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};


struct platform_device lh155_device = {
	.name		= "lh155-lcd",
	.dev		= {
		.platform_data = NULL,
	},
};


struct jzdsi_platform_data jzdsi_pdata = {
	.modes = &jzfb_videomode,
	.video_config.no_of_lanes = 1,
	.video_config.virtual_channel = 0,
	.video_config.color_coding = COLOR_CODE_24BIT,
	.video_config.byte_clock = DEFAULT_DATALANE_BPS / 8,
	.video_config.video_mode = VIDEO_BURST_WITH_SYNC_PULSES,
	.video_config.receive_ack_packets = 0,	/* enable receiving of ack packets */
	.video_config.is_18_loosely = 0, /*loosely: R0R1R2R3R4R5__G0G1G2G3G4G5G6__B0B1B2B3B4B5B6, not loosely: R0R1R2R3R4R5G0G1G2G3G4G5B0B1B2B3B4B5*/
	.video_config.data_en_polarity = 1,

	.dsi_config.max_lanes = 2,
	.dsi_config.max_hs_to_lp_cycles = 100,
	.dsi_config.max_lp_to_hs_cycles = 40,
	.dsi_config.max_bta_cycles = 4095,
	.dsi_config.color_mode_polarity = 1,
	.dsi_config.shut_down_polarity = 1,

	.cmd_list = lh155_cmd_list,
	.cmd_packet_len = ARRAY_SIZE(lh155_cmd_list),

};

struct jzfb_platform_data jzfb_pdata = {
	.num_modes = 1,
	.modes = &jzfb_videomode,
	.dsi_pdata = &jzdsi_pdata,

	.lcd_type = LCD_TYPE_LCM,
	.bpp = 18,
	.width = 31,
	.height = 31,

	.pixclk_falling_edge = 0,
	.date_enable_active_low = 0,

	.alloc_vidmem = 1,

	.smart_config.smart_type = SMART_LCD_TYPE_PARALLEL,
	.smart_config.cmd_width = SMART_LCD_CWIDTH_8_BIT_ONCE,
	.smart_config.data_width = SMART_LCD_DWIDTH_8_BIT_ONCE_PARALLEL_SERIAL,
	.smart_config.clkply_active_rising = 0,
	.smart_config.rsply_cmd_high = 0,
	.smart_config.csply_active_high = 0,
	.smart_config.write_gram_cmd = 0x2C2C,
	.smart_config.bus_width = 8,
	.dither_enable = 1,
	.dither.dither_red = 1,	/* 6bit */
	.dither.dither_red = 1,	/* 6bit */
	.dither.dither_red = 1,	/* 6bit */
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

//	gpio_direction_output(GPIO_BL_PWR_EN, 1);

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
