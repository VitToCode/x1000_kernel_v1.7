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
#include <linux/lh155_init_mipi.h>

struct platform_device lh155_device = {
	.name		= "lh155-lcd",
	.dev		= {
		.platform_data = NULL,
	},
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
	.sync = ~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
};

struct jzdsi_platform_data jzdsi_pdata = {
	.video_config.no_of_lanes = 1,
	.video_config.virtual_channel = 0,
	.video_config.color_coding = COLOR_CODE_24BIT,
	.video_config.byte_clock = DEFAULT_BYTE_CLOCK / 8,	/* KHz  */
	.video_config.video_mode = VIDEO_BURST_WITH_SYNC_PULSES,
	.video_config.receive_ack_packets = 0,	/* enable receiving of ack packets */
	.video_config.pixel_clock = 100000,	/* dpi_clock */
	.video_config.is_18_loosely = 0,
	.video_config.data_en_polarity = 1,

	.video_config.h_polarity = 0,
	.video_config.h_active_pixels = 240,
	.video_config.h_sync_pixels = 0,	/* min 4 pixels */
	.video_config.h_back_porch_pixels = 0,
	.video_config.h_total_pixels = 240,
	.video_config.v_active_lines = 240,
	.video_config.v_polarity = 0,	/*1:active high, 0: active low */
	.video_config.v_sync_lines = 0,
	.video_config.v_back_porch_lines = 0,
	.video_config.v_total_lines = 240,

	.dsi_config.max_lanes = 4,
	.dsi_config.max_hs_to_lp_cycles = 100,
	.dsi_config.max_lp_to_hs_cycles = 40,
	.dsi_config.max_bta_cycles = 4095,
	.dsi_config.color_mode_polarity = 1,
	.dsi_config.shut_down_polarity = 1,

	.cmd_list = lh155_cmd_list,
	.cmd_packet_len = 30,

};

struct jzfb_platform_data jzfb0_pdata = {
	.num_modes = 1,
	.modes = &jzfb0_videomode,
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
