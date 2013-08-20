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
#include <linux/i2c-gpio.h>
#include <linux/input.h>
#include <linux/gsensor.h>
#include <linux/tsc.h>

#include <mach/platform.h>
#include <mach/jzmmc.h>
#include <gpio.h>

#include "urboard.h"

#if ((defined(CONFIG_I2C_GPIO) || defined(CONFIG_I2C1_JZ4780)) && defined(CONFIG_SENSORS_MMA8452))
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

	.ori_pr_swap = 0,
	.ori_pith_negate = 0,
	.ori_roll_negate = 1,
};
#endif

#if 0
#if ((defined(CONFIG_I2C_GPIO) || defined(CONFIG_I2C1_JZ4780)) && defined(CONFIG_SENSORS_LIS3DH))
static struct gsensor_platform_data lis3dh_platform_data = {
	.gpio_int = GPIO_LIS3DH_INT1,
	.poll_interval = 100,
       	.min_interval = 40,
	.max_interval = 400,
	.g_range = GSENSOR_2G,
	.axis_map_x = 1,
	.axis_map_y = 0,
	.axis_map_z = 2,
	.negate_x = 0,
	.negate_y = 1,
	.negate_z = 1,

	.ori_pr_swap = 0,
	.ori_pith_negate = 0,
	.ori_roll_negate = 1,
};
#endif
#endif

#if 0
#if ((defined(CONFIG_I2C_GPIO) || defined(CONFIG_I2C3_JZ4780)) && defined(CONFIG_JZ4780_SUPPORT_TSC))
static struct jztsc_pin urboard_tsc_gpio[] = {
	[0] = {GPIO_CTP_IRQ,		HIGH_ENABLE},
	[1] = {GPIO_CTP_WAKE_UP,	HIGH_ENABLE},
};
#endif

static struct jztsc_platform_data urboard_tsc_pdata = {
	.gpio		= urboard_tsc_gpio,
	.x_max		= 1024,
	.y_max		= 600,
};
#endif

#if (defined(CONFIG_I2C1_JZ4780) || defined(CONFIG_I2C_GPIO))
static struct i2c_board_info urboard_i2c1_devs[] __initdata = {
#if 0
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
#endif
};
#endif	/*I2C1*/

#if ((defined(CONFIG_I2C_GPIO) || defined(CONFIG_I2C2_JZ4780)) && defined(CONFIG_JZ_CIM))
struct cam_sensor_plat_data {
	int facing;
	int orientation;
	int mirror;   //camera mirror
	//u16	gpio_vcc;	/* vcc enable gpio */   remove the gpio_vcc   , DO NOT use this pin for sensor power up ,cim will controls this
	int16_t	gpio_rst;	/* reset  gpio */
	int16_t	gpio_en;	/* camera enable gpio */
	int16_t	gpio_pwdn;      /* sensor powerdown gpio */
	int cap_wait_frame;    /* filter n frames when capture image */
};

#ifdef CONFIG_OV5640
static struct cam_sensor_plat_data ov5640_pdata = {
	.facing = 1,
	.orientation = 0,
	.mirror = 0,
	.gpio_en = GPIO_OV5640_EN,
	.gpio_rst = GPIO_OV5640_RST,
	.gpio_pwdn = GPIO_OV5640_PWDN,
	.cap_wait_frame = 6,
};
#endif

#endif

#if (defined(CONFIG_I2C_GPIO) || defined(CONFIG_I2C2_JZ4780))
#ifdef CONFIG_JZ_CIM
static struct i2c_board_info urboard_i2c2_devs[] __initdata = {
#ifdef CONFIG_OV5640
	{
		I2C_BOARD_INFO("ov5640", 0x3c),
		.platform_data	= &ov5640_pdata,
	},
#endif
}
#elif defined(CONFIG_VIDEO_JZ4780_CIM_HOST)
struct i2c_board_info urboard_i2c2_devs_v4l2[2] = {

	[FRONT_CAMERA_INDEX] = {
#ifdef CONFIG_SOC_CAMERA_OV5640_FRONT
		I2C_BOARD_INFO("ov5640-front", 0x3c),
#endif
	},

	[BACK_CAMERA_INDEX] = {
#ifdef CONFIG_SOC_CAMERA_OV5640_BACK
		I2C_BOARD_INFO("ov5640-back", 0x3c + 1),
#endif
	},
};
#endif
#endif	/*I2C2*/

#if (defined(CONFIG_I2C3_JZ4780) || defined(CONFIG_I2C_GPIO))
static struct i2c_board_info urboard_i2c3_devs[] __initdata = {
#ifdef CONFIG_TOUCHSCREEN_LDWZIC
	{
		I2C_BOARD_INFO("ldwzic_ts", 0x01),
		.platform_data	= &urboard_tsc_pdata,
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_FT5X06
	{
		I2C_BOARD_INFO("ft5x06_tsc", 0x38),
		.platform_data	= &urboard_tsc_pdata,
	},
#endif
};
#endif /*I2C3*/

/*define gpio i2c,if you use gpio i2c,please enable gpio i2c and disable i2c controller*/
#ifdef CONFIG_I2C_GPIO /*CONFIG_I2C_GPIO*/

#define DEF_GPIO_I2C(NO,GPIO_I2C_SDA,GPIO_I2C_SCK)		\
static struct i2c_gpio_platform_data i2c##NO##_gpio_data = {	\
	.sda_pin	= GPIO_I2C_SDA,				\
	.scl_pin	= GPIO_I2C_SCK,				\
};								\
static struct platform_device i2c##NO##_gpio_device = {     	\
	.name	= "i2c-gpio",					\
	.id	= NO,						\
	.dev	= { .platform_data = &i2c##NO##_gpio_data,},	\
};


#ifndef CONFIG_I2C0_JZ4780
DEF_GPIO_I2C(0,GPIO_PD(30),GPIO_PD(31));
#endif
#ifndef CONFIG_I2C1_JZ4780
DEF_GPIO_I2C(1,GPIO_PE(30),GPIO_PE(31));
#endif
#ifndef CONFIG_I2C2_JZ4780
DEF_GPIO_I2C(2,GPIO_PF(16),GPIO_PF(17));
#endif
#if 0
#ifndef CONFIG_I2C3_JZ4780
DEF_GPIO_I2C(3,GPIO_PD(10),GPIO_PD(11));
#endif
#endif
#ifndef CONFIG_I2C4_JZ4780
DEF_GPIO_I2C(4,GPIO_PE(12),GPIO_PE(13));
#endif

#endif /*CONFIG_I2C_GPIO*/


static int __init urboard_i2c_dev_init(void)
{
#ifdef CONFIG_I2C_GPIO

#ifndef CONFIG_I2C0_JZ4780
	platform_device_register(&i2c0_gpio_device);
#endif
#ifndef CONFIG_I2C1_JZ4780
	platform_device_register(&i2c1_gpio_device);
#endif
#ifndef CONFIG_I2C2_JZ4780
	platform_device_register(&i2c2_gpio_device);
#endif
#ifndef CONFIG_I2C3_JZ4780
	platform_device_register(&i2c3_gpio_device);
#endif
#ifndef CONFIG_I2C4_JZ4780
	platform_device_register(&i2c4_gpio_device);
#endif

#endif


#if (defined(CONFIG_I2C1_JZ4780) || defined(CONFIG_I2C_GPIO))
	i2c_register_board_info(1, urboard_i2c1_devs, ARRAY_SIZE(urboard_i2c1_devs));
#endif

#if ((defined(CONFIG_I2C_GPIO) || defined(CONFIG_I2C2_JZ4780)) && defined(CONFIG_JZ_CIM))
#ifdef CONFIG_JZ_CIM
	i2c_register_board_info(2, urboard_i2c2_devs, ARRAY_SIZE(urboard_i2c2_devs));
#endif
#endif

#if (defined(CONFIG_I2C3_JZ4780) || defined(CONFIG_I2C_GPIO))
	i2c_register_board_info(3, urboard_i2c3_devs, ARRAY_SIZE(urboard_i2c3_devs));
#endif
	return 0;
}

arch_initcall(urboard_i2c_dev_init);
