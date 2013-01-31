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

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include <linux/gpio.h>

#include <mach/jz4780_nand.h>


static int __devinit jz_nand_probe(struct platform_device *pdev)
{
	return 0;
}

static int __devexit jz_nand_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver jz_nand_driver = {
	.probe = jz_nand_probe,
	.remove = __devexit_p(jz_nand_remove),
	.driver = {
		.name = "jz4780-nand",
		.owner = THIS_MODULE,
	},
};

static int __init jz_nand_init(void)
{
	return platform_driver_register(&jz_nand_driver);
}
module_init(jz_nand_init);

static void __exit jz_nand_exit(void)
{
	platform_driver_unregister(&jz_nand_driver);
}
module_exit(jz_nand_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fighter Sun <wanmyqawdr@126.com>");
MODULE_DESCRIPTION("NAND controller driver for JZ4780 SoC");
MODULE_ALIAS("platform:jz4780-nand");
