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
	captain,	1000000,	1400000);

/**
 * I/O Regulator.
 * It's the parent regulator of most of devices regulator on board.
 * Voltage was inited at bootloader.
 */
IO_REGULATOR_DEF(
	captain_vccio,
	"Vcc-IO",	3300000,	1);

/**
 * USB VBUS Regulators.
 * Switch of USB VBUS. It may be a actual or virtual regulator.
 */
VBUS_REGULATOR_DEF(
	captain,	"OTG-Vbus");

/**
 * Exclusive Regulators.
 * They are only used by one device each other.
 */
EXCLUSIVE_REGULATOR_DEF(
	captain_vwifi,
	"Wi-Fi",
	"vwifi",	NULL,		3300000);

EXCLUSIVE_REGULATOR_DEF(
	captain_vtsc,
	"Touch Screen",
	"vtsc",		NULL,		3300000);

EXCLUSIVE_REGULATOR_DEF(
	captain_vgsensor,
	"G-sensor",
	"vgsensor",	NULL,		3300000);

/**
 * Fixed voltage Regulators.
 * GPIO silulator regulators. Everyone is an independent device.
 */
FIXED_REGULATOR_DEF(
	captain_vcc5,
	"Vcc-5V",	5000000,	GPIO_PA(17),
	HIGH_ENABLE,	UN_AT_BOOT,	0,
	NULL,		"vhdmi",	"jz-hdmi");

FIXED_REGULATOR_DEF(
	captain_vbus,
	"OTG-Vbus",	5000000,	GPIO_PE(10),
	HIGH_ENABLE,	UN_AT_BOOT,	0,
	"Vcc-5V",	"vdrvvbus",	NULL);

FIXED_REGULATOR_DEF(
	captain_vcim,
	"Camera",	2800000,	GPIO_PB(27),
	HIGH_ENABLE,	UN_AT_BOOT,	0,
	NULL,		"vcim",		"jz-cim");

FIXED_REGULATOR_DEF(
	captain_vlcd,
	"LCD",		3300000,	GPIO_PE(9),
	HIGH_ENABLE,	EN_AT_BOOT,	0,
	NULL,		"vlcd",		NULL);
#ifndef CONFIG_NAND_JZ4780
FIXED_REGULATOR_DEF(
	captain_vmmc,
	"TF",		3300000,	GPIO_PF(19),
	HIGH_ENABLE,	UN_AT_BOOT,	0,
	NULL,		"vmmc.2",	NULL);
#else
FIXED_REGULATOR_DEF(
	captain_vmmc,
	"TF",		3300000,	GPIO_PF(19),
	HIGH_ENABLE,	UN_AT_BOOT,	0,
	NULL,		"vmmc.0",	NULL);
#endif
static struct platform_device *fixed_regulator_devices[] __initdata = {
	&captain_vcc5_regulator_device,
	&captain_vbus_regulator_device,
	&captain_vcim_regulator_device,
	&captain_vlcd_regulator_device,
	&captain_vmmc_regulator_device,
};

/*
 * Regulators definitions used in PMU.
 *
 * If +5V is supplied by PMU, please define "VBUS" here with init_data NULL,
 * otherwise it should be supplied by a exclusive DC-DC, and you should define
 * it as a fixed regulator.
 */
static struct regulator_info captain_pmu_regulators[] = {
	{"OUT1", &captain_vcore_init_data},
	{"OUT3", &captain_vccio_init_data},
	{"OUT6", &captain_vwifi_init_data},
	{"OUT7", &captain_vtsc_init_data},
	{"OUT8", &captain_vgsensor_init_data},
	{"VBUS", &captain_vbus_init_data},
};

//#if defined(CHARG_DETECT_EN)
static struct charger_board_info charger_board_info = {
	.gpio	= -1,		//GPIO_PB(2),
	.enable_level	= LOW_ENABLE,
};
//#endif

static struct pmu_platform_data captain_pmu_pdata = {
	.gpio = GPIO_PA(28),
	.num_regulators = ARRAY_SIZE(captain_pmu_regulators),
	.regulators = captain_pmu_regulators,
//#if defined(CHARG_DETECT_EN)
	.charger_board_info = &charger_board_info,
//#endif
};

#define PMU_I2C_BUSNUM 1

struct i2c_board_info captain_pmu_board_info = {
	I2C_BOARD_INFO("act8600", 0x5a),
	.platform_data = &captain_pmu_pdata,
};

static int __init captain_pmu_dev_init(void)
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

	client = i2c_new_device(adap, &captain_pmu_board_info);
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

subsys_initcall_sync(captain_pmu_dev_init);
