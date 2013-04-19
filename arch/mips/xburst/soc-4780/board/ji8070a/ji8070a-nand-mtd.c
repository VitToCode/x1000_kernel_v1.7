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
#define SIZE_UDISK       (SIZE_ALL - OFFSET_UDISK - 1)


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

static nand_flash_if_t nand_interfaces[] = {
	{ COMMON_NAND_INTERFACE(1, GPIO_BUSY0, 1, GPIO_WP, 1) },
};

static nand_flash_info_t board_support_nand_info_table[] = {
	#define NAND_FLASH_K9GBG08U0A_NANE           "K9GBG08U0A"
	#define NAND_FLASH_K9GBG08U0A_ID             0xd7

	{
		/*
		 * Datasheet of K9GBG08U0A, Rev-1.3, P5, S1.2
		 * ECC : 24bit/1KB
		 *
		 * we assign 28bit/1KB here, the overs are usable when
		 * bitflips occur in OOB area
		 */
		COMMON_NAND_CHIP_INFO(
			NAND_FLASH_K9GBG08U0A_NANE, NAND_FLASH_K9GBG08U0A_ID,
			1024, 28,
			/*
			 * all timings adjust to +15ns
			 *
			 * we change this parameters
			 * because mtd_torturetest failed
			 * ******************************
			 */
			15,
			/*
			 * ******************************
			 */
			12, 5, 12, 5, 20, 5, 12, 5, 12, 10,
			25, 25, 300, 100, 100, 300, 12, 20, 300, 100,
			100, 200 * 1000, 1 * 1000, 200 * 1000,
			5 * 1000 * 1000, BUS_WIDTH_8)
	},
};

static struct jz4780_nand_platform_data nand_pdata = {
	.part_table = parts,
	.num_part = ARRAY_SIZE(parts),

	.nand_flash_if_table = nand_interfaces,
	.num_nand_flash_if = ARRAY_SIZE(nand_interfaces),

	/*
	 * only single thread soft BCH ECC have
	 * already got 20% speed improvement.
	 *
	 * TODO:
	 * does some guys who handle ECC hardware implementation
	 * to look at kernel soft BCH codes.
	 *
	 * I got the patch commits here:
	 *
	 * http://lists.infradead.org/pipermail/linux-mtd/2011-February/033846.html
	 *
	 * and a benchmark of "kernel soft BCH algorithm" VS "Chien search" on that page.
	 *
	 */
	.ecc_type = NAND_ECC_TYPE_SW,


	/*
	 * use polled type cause speed gain
	 * is about 10% ~ 15%
	 */
	.xfer_type = NAND_XFER_CPU_POLL,


	/*
	 * use board specific NAND timings because the timings
	 * need adjust for this board
	 *
	 * the NAND timings match schema is
	 *
	 * First,  fetch NAND timings from following table if it's exist
	 * Second, if board specific codes did not provide following table
	 *         or driver can not find any matched information from
	 *         following table, it will try match information from driver
	 *         build-in NAND timings table, if also can not match anything,
	 *         the NAND timings match will fail.
	 */
	.nand_flash_info_table = board_support_nand_info_table,
	.num_nand_flash_info = ARRAY_SIZE(board_support_nand_info_table),

	/*
	 * relocate hot functions like nand_command to TCSM
	 * that will help to speed improvement.
	 */
	.relocate_hot_functions = 1,
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

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fighter Sun <wanmyqawdr@126.com>");
MODULE_DESCRIPTION("NAND-MTD board specific support template for JZ4780 SoC");
