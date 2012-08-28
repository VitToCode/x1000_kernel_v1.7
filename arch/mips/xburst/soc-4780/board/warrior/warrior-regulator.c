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
	warrior,	1100000,	1450000);

/**
 * I/O Regulator.
 * It's the parent regulator of most of devices regulator on board.
 * Voltage was inited at bootloader.
 */
IO_REGULATOR_DEF(
	warrior_vccio,
	"vccio",	3300000,	1);

/**
 * Exclusive Regulators.
 * They are only used by one device each other.
 */
EXCLUSIVE_REGULATOR_DEF(
	warrior_vwifi,
	"vwifi",	NULL,		3300000);

EXCLUSIVE_REGULATOR_DEF(
	warrior_vtsc,
	"vtsc",		"ft5x0x_tsc",	3300000);

EXCLUSIVE_REGULATOR_DEF(
	warrior_vgsensor,
	"vgsensor",	"g_sensor",	3300000);

/**
 * Fixed voltage Regulators.
 * GPIO silulator regulators. Everyone is an independent device.
 */
FIXED_REGULATOR_DEF(
	warrior_vcc5,
	"vcc5v",	5000000,
	GPIO_PA(17),	HIGH_ENABLE,	0,
	NULL,		"vhdmi",	"jz-hdmi");

FIXED_REGULATOR_DEF(
	warrior_vbus,
	"otg-Vbus",	5000000,
	GPIO_PE(10),	HIGH_ENABLE,	0,
	"vcc5v",	"vbus",		"lm0");

FIXED_REGULATOR_DEF(
	warrior_vmotor,
	"motor",	3300000,
	GPIO_PB(25),	HIGH_ENABLE,	0,
	"vccio",	"vmotor",	"jz_motor");

FIXED_REGULATOR_DEF(
	warrior_vcim,
	"cim",		2800000,
	GPIO_PB(27),	HIGH_ENABLE,	0,
	NULL,		"vcim",		"jz-cim");

FIXED_REGULATOR_DEF(
	warrior_vlcd,
	"lcd",		3300000,
	GPIO_PB(23),	HIGH_ENABLE,	0,
	NULL,		"vlcd",		"kr070la0s_270-lcd");

static struct platform_device *fixed_regulator_devices[] __initdata = {
	&warrior_vcc5_regulator_device,
	&warrior_vbus_regulator_device,
	&warrior_vmotor_regulator_device,
	&warrior_vcim_regulator_device,
	&warrior_vlcd_regulator_device,
};

/*
 * Regulators definitions used in PMU.
 *
 * If +5V is supplied by PMU, please define "VBUS" here with init_data NULL,
 * otherwise it should be supplied by a exclusive DC-DC, and you should define
 * it as a fixed regulator.
 */
static struct regulator_info warrior_pmu_regulators[] = {
	{"OUT1", &warrior_vcore_init_data},
	{"OUT2", &warrior_vccio_init_data},
	{"OUT6", &warrior_vwifi_init_data},
	{"OUT7", &warrior_vtsc_init_data},
	{"OUT8", &warrior_vgsensor_init_data},
};

static struct pmu_platform_data warrior_pmu_pdata = {
	.gpio = GPIO_PF(18),
	.num_regulators = ARRAY_SIZE(warrior_pmu_regulators),
	.regulators = warrior_pmu_regulators,
};

#define PMU_I2C_BUSNUM 0

struct i2c_board_info warrior_pmu_board_info = {
	I2C_BOARD_INFO("act8600", 0x5a),
	.platform_data = &warrior_pmu_pdata,
};

static int __init warrior_pmu_dev_init(void)
{
	struct i2c_adapter *adap;
//	struct i2c_client *client;
	int busnum = PMU_I2C_BUSNUM;
	printk("-----------%s:%d\n", __func__, __LINE__);
	adap = i2c_get_adapter(busnum);
	if (!adap) {
		pr_err("failed to get adapter i2c%d\n", busnum);
		return -1;
	}
#if 0
	client = i2c_new_device(adap, &warrior_pmu_board_info);
	if (!client) {
		pr_err("failed to register pmu to i2c%d\n", busnum);
		return -1;
	}
#endif
	i2c_put_adapter(adap);

//	return platform_add_devices(fixed_regulator_devices,
//				    ARRAY_SIZE(fixed_regulator_devices));
	return platform_add_devices(&fixed_regulator_devices[1],
				    1);
//	return 0;
}

subsys_initcall_sync(warrior_pmu_dev_init);
