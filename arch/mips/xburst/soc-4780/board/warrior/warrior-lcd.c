/*
 * Copyright (c) 2012 Engenic Semiconductor Co., Ltd.
 *              http://www.ingenic.com/
 *
 * JZ4780 warrior board lcd setup routines.
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

#include <mach/jzfb.h>
#include <mach/fb_hdmi_modes.h>

#ifdef CONFIG_LCD_KR070LA0S_270
#include <linux/kr070la0s_270.h>
static struct platform_kr070la0s_270_data kr070la0s_270_pdata= {
/* gpio had been hardware control */
};

/* LCD Panel Device */
struct platform_device kr070la0s_270_device = {
	.name		= "kr070la0s_270-lcd",
	.dev		= {
		.platform_data	= &kr070la0s_270_pdata,
	},
};
#endif

#ifdef CONFIG_LCD_EK070TN93
#include <linux/ek070tn93.h>
static struct platform_ek070tn93_data ek070tn93_pdata= {
	.gpio_de = GPIO_PC(9),
	.gpio_vs = GPIO_PC(19),
	.gpio_hs = GPIO_PC(18),
//	.gpio_reset = GPIO_PB(22),
};

/* LCD Panel Device */
struct platform_device ek070tn93_device = {
	.name		= "ek070tn93-lcd",
	.dev		= {
		.platform_data	= &ek070tn93_pdata,
	},
};
#endif

/* LCD Controller 0 output to HDMI */
static struct fb_videomode jzfb0_videomode[] = {
	ADD_HDMI_VIDEO_MODE(HDMI_640X480_P_60HZ_4X3),
	ADD_HDMI_VIDEO_MODE(HDMI_720X480_P_60HZ_4X3),
	ADD_HDMI_VIDEO_MODE(HDMI_720X480_P_60HZ_16X9),
	ADD_HDMI_VIDEO_MODE(HDMI_1280X720_P_60HZ_16X9),
	ADD_HDMI_VIDEO_MODE(HDMI_1920X1080_I_60HZ_16X9),
	/* add other mode later */
};

struct jzfb_platform_data jzfb0_pdata = {
	.num_modes = ARRAY_SIZE(jzfb0_videomode),
	.modes = jzfb0_videomode,

	.lcd_type = LCD_TYPE_GENERIC_24_BIT,
	.bpp = 24,
	.width = 0,
	.height = 0,

	.pixclk_falling_edge = 1,
	.date_enable_active_low = 0,

	.alloc_vidmem = 0,
};

/* LCD Controller 1 output to LVDS TFT panel */
static struct fb_videomode jzfb1_videomode = {
#ifdef CONFIG_LCD_KR070LA0S_270
	.name = "1024x600",
	.refresh = 60,
	.xres = 1024,
	.yres = 600,
	.pixclock = KHZ2PICOS(51200),
	.left_margin = 320,
	.right_margin = 0,
	.upper_margin = 35,
	.lower_margin = 0,
	.hsync_len = 0,
	.vsync_len = 0,
	.sync = ~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
#endif
#ifdef CONFIG_LCD_EK070TN93
	.name = "800x480",
	.refresh = 60,
	.xres = 800,
	.yres = 480,
	.pixclock = KHZ2PICOS(33300),
	.left_margin = 28,
	.right_margin = 210,
	.upper_margin = 15,
	.lower_margin = 22,
	.hsync_len = 18,
	.vsync_len = 8,
	.sync = ~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,
	.vmode = FB_VMODE_NONINTERLACED,
	.flag = 0,
#endif
};

struct jzfb_platform_data jzfb1_pdata = {
#ifdef CONFIG_LCD_KR070LA0S_270
	.num_modes = 1,
	.modes = &jzfb1_videomode,

	.lcd_type = LCD_TYPE_GENERIC_24_BIT,
	.bpp = 24,
	.width = 154,
	.height = 90,

	.pixclk_falling_edge = 0,
	.date_enable_active_low = 0,

	.alloc_vidmem = 1,

	.lvds = 1,
	.txctrl.data_format = VESA,
	.txctrl.clk_edge_falling_7x = 0,
	.txctrl.clk_edge_falling_1x = 1,
	.txctrl.data_start_edge = START_EDGE_1,
	.txctrl.operate_mode = LVDS_1X_CLKOUT,
	.txctrl.edge_delay = DELAY_1_5NS,
	.txctrl.output_amplitude = VOD_350MV,

	.txpll0.ssc_enable = 0,
	.txpll0.ssc_mode_center_spread = 0,
	.txpll0.post_divider = POST_DIV_1,
	.txpll0.feedback_divider = 42,
	.txpll0.input_divider = 6,

	.txpll1.charge_pump = CHARGE_PUMP_10UA,
	.txpll1.vco_gain = VCO_GAIN_150M_400M,
	.txpll1.vco_biasing_current = VCO_BIASING_2_5UA,
	.txpll1.sscn = 0,
	.txpll1.ssc_counter = 0,

	.txectrl.emphasis_level = 2,
	.txectrl.emphasis_enable = 1,
	.txectrl.ldo_output_voltage = LDO_OUTPUT_1_2V,
	.txectrl.fine_tuning_7x_clk = 3,
	.txectrl.coarse_tuning_7x_clk = 2,

	.dither_enable = 0,
#endif
#ifdef CONFIG_LCD_EK070TN93
	.num_modes = 1,
	.modes = &jzfb1_videomode,

	.lcd_type = LCD_TYPE_GENERIC_24_BIT,
	.bpp = 24,
	.width = 154,
	.height = 86,

	.pixclk_falling_edge = 0,
	.date_enable_active_low = 0,

	.alloc_vidmem = 1,
	.lvds = 0,
	.dither_enable = 0,
#endif
};

#ifdef CONFIG_BACKLIGHT_PWM
static int warrior_backlight_init(struct device *dev)
{
	return 0;
}

static void warrior_backlight_exit(struct device *dev)
{
}

static struct platform_pwm_backlight_data warrior_backlight_data = {
	.pwm_id		= 0,
	.max_brightness	= 255,
	.dft_brightness	= 80,
	.pwm_period_ns	= 10000, /* 100 kHZ */
	.init		= warrior_backlight_init,
	.exit		= warrior_backlight_exit,
};

/* Backlight Device */
struct platform_device warrior_backlight_device = {
	.name		= "pwm-backlight",
	.dev		= {
		.platform_data	= &warrior_backlight_data,
	},
};

#endif
