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

/*
 * **********************************************
 * NAND flash device name & ID
 * **********************************************
 */
#define NAND_FLASH_K9GBG08U0A_NANE	"K9GBG08U0A"
#define NAND_FLASH_K9GBG08U0A_ID	0xd7

/*
 * **********************************************
 * supported NAND flash devices table
 * **********************************************
 */
static struct nand_flash_dev builtin_nand_flash_table[] = {
	/*
	 * These are the new chips with large page size. The pagesize and the
	 * erasesize is determined from the extended id bytes
	 */

	{
		NAND_FLASH_K9GBG08U0A_NANE, NAND_FLASH_K9GBG08U0A_ID,
		0, 4096, 0, LP_OPTIONS
	},
};

/*
 * **********************************************
 * supported NAND flash timings parameters table
 * it extent the upper table
 * **********************************************
 */
static nand_flash_info_t builtin_nand_info_table[] = {
	{
		COMMON_NAND_CHIP_INFO(
			NAND_FLASH_K9GBG08U0A_NANE, NAND_FLASH_K9GBG08U0A_ID,
			1024, 40,
			12, 5, 12, 5, 20, 5, 12, 5, 12, 10,
			25, 300, 120, 300, 12,20, 300, 100,
			100, 100, 100 * 1000, 1 * 1000, 90 * 1000,
			5 * 1000 * 1000, BUS_WIDTH_8)
	},
};

struct jz4780_nand {
	struct mtd_info mtd;
	struct mtd_partition *parts;
	struct nand_chip chip;

	gpemc_bank_t banks[7];
	nand_timing_t *nand_timing;

	bch_request_t bch_req;

	struct completion *op_done;
	int busy_irq;

	struct nand_flash_dev *nand_flash_table;
	int num_nand_flash;

	nand_flash_info_t *nand_flash_info_table;
	int num_nand_flash_info;

	struct jz4780_nand_platform_data *pdata;
	struct platform_device *pdev;
};

static int __devinit jz4780_nand_probe(struct platform_device *pdev)
{
	int ret;
	int tmp;
	int i;

	struct nand_chip *chip;
	struct mtd_info *mtd;
	struct mtd_partition *partition;

	struct jz4780_nand *nand;
	struct jz4780_nand_platform_data *pdata;

	/*
	 * sanity check
	 */
	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
		dev_err(&pdev->dev, "Failed to get platform_data.\n");
		return -ENXIO;
	}

	nand = kzalloc(sizeof(struct jz4780_nand), GFP_KERNEL);
	if (!nand) {
		dev_err(&pdev->dev,
				"Failed to allocate jz4780_nand.\n");
		return -ENOMEM;
	}

	/*
	 * request NAND interfaces
	 */
	for (i = 0; i < pdata->num_nand_interface; i++) {
		tmp = pdata->nand_interface_table[i].bank;
		ret = gpemc_request_cs(&nand->banks[tmp], tmp);
		if (!ret) {
			dev_err(&pdev->dev,
					"Failed request GPEMC bank%d.\n", tmp);
			goto err_release_cs;
		}
	}

	/*
	 * NAND flash device & info override
	 */
	nand->nand_flash_table = pdata->nand_flash_table ?
		pdata->nand_flash_table : builtin_nand_flash_table;
	nand->num_nand_flash = pdata->nand_flash_table ?
		pdata->num_nand_flash :
			ARRAY_SIZE(builtin_nand_flash_table);

	nand->nand_flash_info_table = pdata->nand_flash_info_table ?
		pdata->nand_flash_info_table : builtin_nand_info_table;
	nand->num_nand_flash_info = pdata->nand_flash_info_table ?
		pdata->num_nand_flash_info :
			ARRAY_SIZE(builtin_nand_info_table);

	/*
	 * MTD subsystem register
	 */
	mtd = &nand->mtd;
	chip = &nand->chip;
	mtd->priv = chip;
	mtd->name = DRVNAME;
	mtd->owner = THIS_MODULE;


	return 0;

err_release_cs:
	for (tmp = 0; tmp < i; tmp++) {
		ret = pdata->nand_interface_table[tmp].bank;
		gpemc_release_cs(&nand->banks[ret]);
	}

err_free_jz4780_nand:
	kfree(nand);

	return ret;
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
