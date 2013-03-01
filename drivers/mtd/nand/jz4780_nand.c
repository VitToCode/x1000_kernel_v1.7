/*
 *  Copyright (C) 2013 Fighter Sun <wanmyqawdr@126.com>
 *  JZ4780 SoC NAND controller driver
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

#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/completion.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include <linux/gpio.h>

#include <soc/gpemc.h>
#include <soc/bch.h>
#include <mach/jz4780_nand.h>

#define DRVNAME "jz4780-nand"

struct jz4780_nand {
	struct mtd_info mtd;
	struct mtd_partition *parts;
	struct nand_chip chip;

	gpemc_bank_t bank;
	gpemc_bank_timing_t bank_timing;
	common_nand_timing_t common_timing;
	toggle_nand_timing_t toggle_nand_timing;

	bch_request_t bch_req;

	struct completion *op_done;

	int busy_irq;

	struct jz4780_nand_platform_data *pdata;
	struct platform_device *pdev;
};

static int __devinit jz4780_nand_probe(struct platform_device *pdev)
{
	return 0;
}

static int __devexit jz4780_nand_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver jz4780_nand_driver = {
	.probe = jz4780_nand_probe,
	.remove = __devexit_p(jz4780_nand_remove),
	.driver = {
		.name = DRVNAME,
		.owner = THIS_MODULE,
	},
};

static int __init jz4780_nand_init(void)
{
	return platform_driver_register(&jz4780_nand_driver);
}
rootfs_initcall(jz4780_nand_init);

static void __exit jz4780_nand_exit(void)
{
	platform_driver_unregister(&jz4780_nand_driver);
}
module_exit(jz4780_nand_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fighter Sun <wanmyqawdr@126.com>");
MODULE_DESCRIPTION("NAND controller driver for JZ4780 SoC");
MODULE_ALIAS("platform:"DRVNAME);
