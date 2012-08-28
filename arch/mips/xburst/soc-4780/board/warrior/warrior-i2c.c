/*
 * [board]-i2c.c - This file defines i2c devices on the board.
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/gsensor.h>
#include <linux/tsc.h>

#include <mach/platform.h>
#include <mach/jzmmc.h>
#include <gpio.h>

#include "warrior.h"

#if defined(CONFIG_SENSORS_MMA8452) && defined(CONFIG_I2C1_JZ4780)
static struct gsensor_platform_data mma8452_platform_pdata = {
	.gpio_int = GPIO_MMA8452_INT1,
	.poll_interval = 100,
	.min_interval = 40,
	.max_interval = 200,
	.g_range = GSENSOR_2G,
	.axis_map_x = 1,
	.axis_map_y = 0,
	.axis_map_z = 2,
	.negate_x = 1,
	.negate_y = 0,
	.negate_z = 1,
};
#endif

#if defined(CONFIG_SENSORS_LIS3DH) && defined(CONFIG_I2C1_JZ4780)
static struct gsensor_platform_data lis3dh_platform_data = {
	.gpio_int = GPIO_LIS3DH_INT1, 
	.poll_interval = 100,
       	.min_interval = 40,
	.max_interval = 200,
	.g_range = GSENSOR_2G,
	.axis_map_x = 1,
	.axis_map_y = 0,
	.axis_map_z = 2,							        
	.negate_x = 1,
	.negate_y = 0,
	.negate_z = 1,
};
#endif

#if defined(CONFIG_JZ4780_SUPPORT_TSC) && defined(CONFIG_I2C3_JZ4780)
static struct jztsc_pin warrior_tsc_gpio[] = {
	[0] = {GPIO_CTP_IRQ,		HIGH_ENABLE},
	[1] = {GPIO_CTP_WAKE_UP,	HIGH_ENABLE},
};

static struct jztsc_platform_data warrior_tsc_pdata = {
	.gpio		= warrior_tsc_gpio,
};
#endif

#ifdef CONFIG_I2C1_JZ4780 /*I2C1*/
static struct i2c_board_info warrior_i2c1_devs[] __initdata = {
#ifdef CONFIG_SENSORS_MMA8452
	{
		I2C_BOARD_INFO("gsensor_mma8452",0x1c),
		.platform_data = &mma8452_platform_pdata,
	},
#endif
#ifdef CONFIG_SENSORS_LIS3DH
	{
	       	I2C_BOARD_INFO("gsensor_lis3dh",0x18),
		.platform_data = &lis3dh_platform_data,
	},						        
#endif
};
#endif	/*I2C1*/

#ifdef CONFIG_I2C3_JZ4780 /*I2C3*/
static struct i2c_board_info warrior_i2c3_devs[] __initdata = {
#ifdef CONFIG_JZ4780_SUPPORT_TSC
	{
		I2C_BOARD_INFO("ft5x0x_tsc", 0x36),
		.platform_data	= &warrior_tsc_pdata,
	},
#endif
};
#endif /*I2C3*/

extern struct i2c_board_info warrior_pmu_board_info;


static int __init warrior_i2c_dev_init(void)
{
	printk("-----------%s:%d\n", __func__, __LINE__);
	i2c_register_board_info(0, &warrior_pmu_board_info, 1);

#ifdef CONFIG_I2C1_JZ4780
	i2c_register_board_info(1, warrior_i2c1_devs, ARRAY_SIZE(warrior_i2c1_devs));
#endif
#ifdef CONFIG_I2C3_JZ4780
	i2c_register_board_info(3, warrior_i2c3_devs, ARRAY_SIZE(warrior_i2c3_devs));
#endif	
	return 0;
}

arch_initcall(warrior_i2c_dev_init);
