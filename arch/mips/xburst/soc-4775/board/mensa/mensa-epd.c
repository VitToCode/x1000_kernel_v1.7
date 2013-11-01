/*
 * Copyright (c) 2012 Engenic Semiconductor Co., Ltd.
 *              http://www.ingenic.com/
 *
 * JZ4780 orion board lcd setup routines.
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

#include <mach/jzepd.h>

#include "board.h"

static void mensa_epd_power_init(void)
{
	gpio_request(GPIO_EPD_SIG_CTRL_N, "epd enable");
	gpio_request(GPIO_EPD_PWR0, "epd pwr0");
	gpio_request(GPIO_EPD_PWR1, "epd pwr1");
	gpio_request(GPIO_EPD_PWR2, "epd pwr2");
	gpio_request(GPIO_EPD_PWR3, "epd pwr3");
	gpio_request(GPIO_EPD_PWR4, "epd pwr4");
}

//EPD power up sequence function for epd driver
static void mensa_epd_power_on(void)
{
	gpio_direction_output(GPIO_EPD_SIG_CTRL_N, 0);
	/*mensa_vdd_power_up*/
	gpio_direction_output(GPIO_EPD_PWR0, 1);
	udelay(100);
	/*mensa_vneg_power_up(); */
	gpio_direction_output(GPIO_EPD_PWR1, 1);
	/*mensa_vee_power_up*/
	mdelay(1);
	/*mensa_vpos_power_up*/
	gpio_direction_output(GPIO_EPD_PWR3, 1);
	/*mensa_vgg_power_up*/
	gpio_direction_output(GPIO_EPD_PWR2, 1);
	udelay(100);
	/*mensa_vcom_power_up*/
	gpio_direction_output(GPIO_EPD_PWR4, 1);
	mdelay(1);
}

//EPD power down sequence function for epd driver
static void mensa_epd_power_off(void)
{
	udelay(100);
	/*mensa_vcom_power_down*/
	gpio_direction_output(GPIO_EPD_PWR4, 0);
	/*mensa_vgg_power_down*/
	gpio_direction_output(GPIO_EPD_PWR2, 0);
	/*mensa_vpos_power_down*/
	gpio_direction_output(GPIO_EPD_PWR3, 0);
	/*mensa_vneg_power_down*/
	gpio_direction_output(GPIO_EPD_PWR1, 0);
	/*mensa_vee_power_down*/
	/*mensa_vdd_power_down*/
	gpio_direction_output(GPIO_EPD_PWR0, 0);
	gpio_direction_output(GPIO_EPD_SIG_CTRL_N, 1);
}

struct jz_epd_platform_data jz_epd_pdata = {
	.epd_power_ctrl.epd_power_init = mensa_epd_power_init,
	.epd_power_ctrl.epd_power_on = mensa_epd_power_on,
	.epd_power_ctrl.epd_power_off = mensa_epd_power_off,
};

