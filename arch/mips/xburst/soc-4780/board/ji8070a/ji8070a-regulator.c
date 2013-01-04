/*
 * [board]-pmu.c - This file defines PMU board information.
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 * Author: Large Dipper <ykli@ingenic.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/pmu-common.h>
#include <linux/i2c.h>
#include <gpio.h>

/**
 * Core voltage Regulator.
 * Couldn't be modified.
 * Voltage was inited at bootloader.
 */
CORE_REGULATOR_DEF(
	ji8070a,	1000000,	1400000);

/**
 * I/O Regulator.
 * It's the parent regulator of most of devices regulator on board.
 * Voltage was inited at bootloader.
 */
IO_REGULATOR_DEF(
	ji8070a_vccio,
	"Vcc-IO",	3300000,	1);

/**
 * USB VBUS Regulators.
 * Switch of USB VBUS. It may be a actual or virtual regulator.
 */
VBUS_REGULATOR_DEF(
	ji8070a,		"OTG-Vbus");

/**
 * Exclusive Regulators.
 * They are only used by one device each other.
 */
EXCLUSIVE_REGULATOR_DEF(
	ji8070a_vwifi,
	"Wi-Fi",
	"vwifi",	NULL,		3300000);

EXCLUSIVE_REGULATOR_DEF(
	ji8070a_vtsc,
	"Touch Screen",
	"vtsc",		NULL,		3300000);


EXCLUSIVE_REGULATOR_DEF(
	ji8070a_vgsensor,
	"G-sensor",
	"vgsensor",	NULL,		3300000);

/**
 * Fixed voltage Regulators.
 * GPIO silulator regulators. Everyone is an independent device.
 */
#ifdef CONFIG_Q8
EXCLUSIVE_REGULATOR_DEF(
	ji8070a_vcc5,
	"Vcc-5V",
	"vhdmi",	"jz-hdmi",	5000000);
#else
FIXED_REGULATOR_DEF(
	ji8070a_vcc5,
	"Vcc-5V",	5000000,
	GPIO_PA(17),	HIGH_ENABLE,	UN_AT_BOOT,0,
	NULL,		"vhdmi",	"jz-hdmi");
#endif

/* FIXME! when board fixed, remove it */
FIXED_REGULATOR_DEF(
	ji8070a_vbus,
	"OTG-Vbus",	5000000,GPIO_PE(10),	
	HIGH_ENABLE,UN_AT_BOOT,	0,
	NULL,	"vdrvvbus",	NULL);

FIXED_REGULATOR_DEF(
	ji8070a_vcim,
	"Camera",	2800000,GPIO_PB(27),	
	HIGH_ENABLE,UN_AT_BOOT,	0,
	NULL,		"vcim",		"jz-cim");

#ifdef CONFIG_Q8

	EXCLUSIVE_REGULATOR_DEF(
		ji8070a_vq8lcd,
		"vlcd",
		"vlcd",NULL,	3300000);

	FIXED_REGULATOR_DEF(
		ji8070a_vlcd,
		"vbklight",		3300000,	GPIO_PB(23),
		HIGH_ENABLE,	EN_AT_BOOT,	0,
		NULL,		"vbklight",		NULL);

#else

	FIXED_REGULATOR_DEF(
		ji8070a_vlcd,
		"LCD",		3300000,GPIO_PB(23),
		HIGH_ENABLE,EN_AT_BOOT,	0,
		NULL,		"vlcd",		NULL);

#endif

static struct platform_device *fixed_regulator_devices[] __initdata = {
#ifndef CONFIG_Q8
	&ji8070a_vcc5_regulator_device,
#endif
	&ji8070a_vbus_regulator_device,
	&ji8070a_vcim_regulator_device,
	&ji8070a_vlcd_regulator_device,
};

/*
 * Regulators definitions used in PMU.
 *
 * If +5V is supplied by PMU, please define "VBUS" here with init_data NULL,
 * otherwise it should be supplied by a exclusive DC-DC, and you should define
 * it as a fixed regulator.
 */
static struct regulator_info ji8070a_pmu_regulators[] = {
	{"OUT1", &ji8070a_vcore_init_data},
	{"OUT3", &ji8070a_vccio_init_data},
#ifdef CONFIG_Q8
	{"OUT4", &ji8070a_vcc5_init_data},
#endif
	{"OUT6", &ji8070a_vwifi_init_data},
	{"OUT7", &ji8070a_vtsc_init_data},
#ifdef  CONFIG_Q8
	{"OUT8", &ji8070a_vq8lcd_init_data},
#else
	{"OUT8", &ji8070a_vgsensor_init_data},
#endif
	{"VBUS", &ji8070a_vbus_init_data},
};

static struct charger_board_info charger_board_info = {
	.gpio	= GPIO_PB(2),
	.enable_level	= LOW_ENABLE,
};

static struct pmu_platform_data ji8070a_pmu_pdata = {
	.gpio = GPIO_PA(28),
	.num_regulators = ARRAY_SIZE(ji8070a_pmu_regulators),
	.regulators = ji8070a_pmu_regulators,
	.charger_board_info = &charger_board_info,
};

#define PMU_I2C_BUSNUM 0

struct i2c_board_info ji8070a_pmu_board_info = {
	I2C_BOARD_INFO("act8600", 0x5a),
	.platform_data = &ji8070a_pmu_pdata,
};

static int __init ji8070a_pmu_dev_init(void)
{
	struct i2c_adapter *adap;
	struct i2c_client *client;
	int busnum = PMU_I2C_BUSNUM;
	int i;

	adap = i2c_get_adapter(busnum);
	if (!adap) {
		pr_err("failed to get adapter i2c%d\n", busnum);
		return -1;
	}

	client = i2c_new_device(adap, &ji8070a_pmu_board_info);
	if (!client) {
		pr_err("failed to register pmu to i2c%d\n", busnum);
		return -1;
	}

	i2c_put_adapter(adap);

	for (i = 0; i < ARRAY_SIZE(fixed_regulator_devices); i++)
		fixed_regulator_devices[i]->id = i;

	return platform_add_devices(fixed_regulator_devices,
				    ARRAY_SIZE(fixed_regulator_devices));
}

subsys_initcall_sync(ji8070a_pmu_dev_init);
