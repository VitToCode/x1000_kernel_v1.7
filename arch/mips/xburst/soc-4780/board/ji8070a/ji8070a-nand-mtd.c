/*
 *  Copyright (C) 2013 Fighter Sun <wanmyqawdr@126.com>
 *  NAND-MTD support template
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

#include <gpio.h>

#include <mach/jz4780_nand.h>

#define DRVNAME "jz4780-nand"

#define GPIO_BUSY0       GPIO_PA(20)
#define GPIO_WP          GPIO_PF(22)

#define SIZE_MB          (1024 * 1024LL)
#define SIZE_ALL         (8192 * SIZE_MB)

#define OFFSET_XBOOT     (0)
#define SIZE_XBOOT       (8      * SIZE_MB)

#define OFFSET_BOOT      (SIZE_XBOOT)
#define SIZE_BOOT        (16     * SIZE_MB)

#define OFFSET_SYSTEM    (OFFSET_BOOT + SIZE_BOOT)
#define SIZE_SYSTEM      (512    * SIZE_MB)

#define OFFSET_DATA      (OFFSET_SYSTEM + SIZE_SYSTEM)
#define SIZE_DATA        (1024   * SIZE_MB)

#define OFFSET_CACHE     (OFFSET_DATA + SIZE_DATA)
#define SIZE_CACHE       (128    * SIZE_MB)

#define OFFSET_RECOVERY  (OFFSET_CACHE + SIZE_CACHE)
#define SIZE_RECOVERY    (16     * SIZE_MB)

#define OFFSET_MISC      (OFFSET_RECOVERY + SIZE_RECOVERY)
#define SIZE_MISC        (16     * SIZE_MB)

#define OFFSET_UDISK     (OFFSET_MISC + SIZE_MISC)
#define SIZE_UDISK       (SIZE_ALL - OFFSET_UDISK)


static struct mtd_partition parts[] = {
	{
		.name = "xboot",
		.offset = OFFSET_XBOOT,
		.size = SIZE_XBOOT,
	}, {
		.name = "boot",
		.offset = OFFSET_BOOT,
		.size = SIZE_BOOT,
	}, {
		.name = "system",
		.offset = OFFSET_SYSTEM,
		.size = SIZE_SYSTEM,
	}, {
		.name = "data",
		.offset = OFFSET_DATA,
		.size = SIZE_DATA,
	}, {
		.name = "cache",
		.offset = OFFSET_CACHE,
		.size = SIZE_CACHE,
	}, {
		.name = "recovery",
		.offset = OFFSET_RECOVERY,
		.size = SIZE_RECOVERY,
	}, {
		.name = "misc",
		.offset = OFFSET_MISC,
		.size = SIZE_MISC,
	}, {
		.name = "udisk",
		.offset = OFFSET_UDISK,
		.size = SIZE_UDISK,
	}
};

static nand_interface_t nand_interfaces[] = {
	{
		.bank = 1,
		.type = BANK_TYPE_NAND,

		.busy_gpio = GPIO_BUSY0,
		.busy_gpio_low_assert = 1,

		.wp_gpio = GPIO_WP,
		.wp_gpio_low_assert = 1,
	}
};

static struct jz4780_nand_platform_data nand_pdata = {
	.part_table = parts,
	.num_part = ARRAY_SIZE(parts),

	.nand_interface_table = nand_interfaces,
	.num_nand_interface = ARRAY_SIZE(nand_interfaces),

	.xfer_type = NAND_XFER_CPU_IRQ,
};

static struct platform_device nand_dev = {
	.name = DRVNAME,
	.id = 0,
};

static __init int nand_mtd_device_register(void)
{
	platform_device_add_data(&nand_dev, &nand_pdata, sizeof(nand_pdata));
	return platform_device_register(&nand_dev);
}
arch_initcall(nand_mtd_device_register);
