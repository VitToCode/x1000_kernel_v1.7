/*
 * Copyright (c) 2012 Engenic Semiconductor Co., Ltd.
 *              http://www.ingenic.com/
 *
 * JZ4780 board lcd setup routines.
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
#include <video/platform_lcd.h>
#include <linux/pwm_backlight.h>

#include <mach/jzfb.h>
#include <mach/fb_hdmi_modes.h>

#include <linux/regulator/consumer.h>

#define LCD_DISP_N	GPIO_PD(11)
#define LCD_RESET_N	GPIO_PD(10)
#define LCD_PWM		GPIO_PE(0)

#define LCD_PW_EN	GPIO_PE(9)

static struct regulator *lcd_vcc_reg = NULL;

static void board_lcd_power_on(void)
{
	int ret = 0;

	printk("=========================>enter %s\n", __func__);
	/* NOTE: take care of the power on sequence */

	lcd_vcc_reg = regulator_get(NULL, "vlcd");
	if (IS_ERR(lcd_vcc_reg)) {
		printk("==>#####failed to get regulator vlcd\n");
		return;// PTR_ERR(lcd_vcc_reg);
	}
	regulator_enable(lcd_vcc_reg);

	ret = gpio_request(LCD_PW_EN, "LCD_PW_EN");
	gpio_direction_output(LCD_PW_EN, 1);
	//gpio_free(LCD_PW_EN);

	gpio_request(LCD_DISP_N, "LCD_DISP_N");
	gpio_direction_output(LCD_DISP_N, 1);
	//gpio_free(LCD_DISP_N);
#if 1
	gpio_request(LCD_RESET_N, "LCD_RESET_N");
	gpio_direction_output(LCD_RESET_N, 1);
	//gpio_free(LCD_RESET_N);
#endif
	gpio_request(LCD_PWM, "LCD_PWM");
	gpio_direction_output(LCD_PWM, 1);
	//gpio_free(LCD_PWM);
}

static void board_lcd_power_off(void)
{
        regulator_disable(lcd_vcc_reg);
	msleep(60);
	regulator_put(lcd_vcc_reg);
}

static void board_lcd_set_power(struct plat_lcd_data *pd, unsigned int power)
{
	if (power) {
		board_lcd_power_on();
	} else {
		board_lcd_power_off();
	}
}


static struct plat_lcd_data board_lcd_platform_data = {
	.set_power		= board_lcd_set_power,
};

struct platform_device grus_lcd_device = {
	.name			= "platform-lcd",
	.id			= -1,
	.dev			= {
		.platform_data	= &board_lcd_platform_data,
	},
};

/* LCD Controller 1 output to LVDS TFT panel */
static struct fb_videomode jzfb1_videomode[] = {
	{
		.name = "800*480",
		.refresh = 60,
		.xres = 800,
		.yres = 480,
		.left_margin = 215,
		.right_margin = 40,
		.upper_margin = 34,
		.lower_margin = 10,
		.hsync_len = 1,
		.vsync_len = 1,
		.sync = 0 | 0, /* FB_SYNC_HOR_HIGH_ACT:0, FB_SYNC_VERT_HIGH_ACT:0 */
		.vmode = FB_VMODE_NONINTERLACED,
		.flag = 0
	},
};

struct jzfb_platform_data jzfb1_pdata = {
	.num_modes = ARRAY_SIZE(jzfb1_videomode),
	.modes = jzfb1_videomode,

	.lcd_type = LCD_TYPE_GENERIC_24_BIT,
	.bpp = 24,
//	.width = 165,
//	.height = 125,
	.width = 800,
	.height = 480,

	.pixclk_falling_edge = 1,
	.date_enable_active_low = 0,

	.alloc_vidmem = 1,
	.dither_enable = 0,

#ifdef CONFIG_DISABLE_LVDS_FUNCTION
	.lvds = 0,
#else
	.lvds = 1,
	.txctrl.data_format = VESA,
	.txctrl.clk_edge_falling_7x = 0,
	.txctrl.clk_edge_falling_1x = 1,
	.txctrl.data_start_edge = START_EDGE_4,
	.txctrl.operate_mode = LVDS_1X_CLKOUT,
	.txctrl.edge_delay = DELAY_0_1NS,
	.txctrl.output_amplitude = VOD_350MV,

	.txpll0.ssc_enable = 0,
	.txpll0.ssc_mode_center_spread = 0,
	.txpll0.post_divider = POST_DIV_1,
	.txpll0.feedback_divider = 70,
	.txpll0.input_divider_bypass = 0,
	.txpll0.input_divider = 10,

	.txpll1.charge_pump = CHARGE_PUMP_10UA,
	.txpll1.vco_gain = VCO_GAIN_150M_400M,
	.txpll1.vco_biasing_current = VCO_BIASING_2_5UA,
	.txpll1.sscn = 0,
	.txpll1.ssc_counter = 0,

	.txectrl.emphasis_level = 0,
	.txectrl.emphasis_enable = 0,
	.txectrl.ldo_output_voltage = LDO_OUTPUT_1_1V,
	.txectrl.phase_interpolator_bypass = 1,
	.txectrl.fine_tuning_7x_clk = 0,
	.txectrl.coarse_tuning_7x_clk = 0,
#endif
};

static int board_backlight_init(struct device *dev)
{
	int gpio = LCD_PWM;//GPIO_PD(29);
	printk("=========================>enter %s\n", __func__);
	gpio_request(gpio, "LCD_WEN");
	gpio_direction_output(gpio, 1);
	//gpio_free(gpio);
	return 0;
}

static void board_backlight_exit(struct device *dev)
{
}

static int board_notify(struct device *dev, int brightness)
{
	brightness = 50 + (brightness * 4)/5;
	return brightness;
}

static struct platform_pwm_backlight_data board_backlight_data = {
	.pwm_id		= 0,
	.max_brightness	= 255,
	.dft_brightness	= 120,
	.pwm_period_ns	= 1000000, /* 1 kHZ */
	.init		= board_backlight_init,
	.exit		= board_backlight_exit,
	.notify     = board_notify,
};

/* Backlight Device */
struct platform_device grus_backlight_device = {
	.name		= "pwm-backlight",
	.dev		= {
		.platform_data	= &board_backlight_data,
	},
};
