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
#include <linux/irq.h>
#include <linux/interrupt.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include <linux/gpio.h>

#include <soc/gpemc.h>
#include <soc/bch.h>
#include <mach/jz4780_nand.h>

#define DRVNAME "jz4780-nand"

#define MAX_NUM_NAND_IF	7

/*
 * ******************************************************
 * 	NAND flash device name & ID
 * ******************************************************
 */
#define NAND_FLASH_K9GBG08U0A_NANE	"K9GBG08U0A"
#define NAND_FLASH_K9GBG08U0A_ID	0xd7

/*
 * ******************************************************
 * 	supported NAND flash devices table
 * ******************************************************
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
 * *****************************************************
 * 	supported NAND flash timings parameters table
 * 	it extent the upper table
 * *****************************************************
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

const char *label_wp_gpio[] = {
	"",
	"bank1-nand-wp",
	"bank2-nand-wp",
	"bank3-nand-wp",
	"bank4-nand-wp",
	"bank5-nand-wp",
	"bank6-nand-wp",
};

const char *label_busy_gpio[] = {
	"",
	"bank1-nand-busy",
	"bank2-nand-busy",
	"bank3-nand-busy",
	"bank4-nand-busy",
	"bank5-nand-busy",
	"bank6-nand-busy",
};

struct jz4780_nand {
	struct mtd_info mtd;
	struct mtd_partition *parts;
	struct nand_chip chip;

	nand_flash_info_t *nand_flash_info;

	nand_flash_if_t *nand_flash_if_table[MAX_NUM_NAND_IF];
	int num_nand_flash_if;

	int curr_nand_if;

	bch_request_t bch_req;
	struct completion bch_req_done;
	nand_ecc_type_t ecc_type;

	struct nand_flash_dev *nand_flash_table;
	int num_nand_flash;

	nand_flash_info_t *nand_flash_info_table;
	int num_nand_flash_info;

	nand_xfer_type_t xfer_type;

	struct jz4780_nand_platform_data *pdata;
	struct platform_device *pdev;
};

static int jz4780_nand_busy(nand_flash_if_t *nand_if)
{
	int low_assert;
	int gpio;

	low_assert = nand_if->busy_gpio_low_assert;
	gpio = nand_if->busy_gpio;

	return gpio_get_value_cansleep(gpio) ^ low_assert;
}

static void jz4780_nand_enable_wp(nand_flash_if_t *nand_if, int enable)
{
	int low_assert;
	int gpio;

	low_assert = nand_if->wp_gpio_low_assert;
	gpio = nand_if->wp_gpio;

	if (enable)
		gpio_set_value_cansleep(gpio, low_assert ^ 1);
	else
		gpio_set_value_cansleep(gpio, !(low_assert ^ 1));
}

static void jz4780_nand_hwctl(struct mtd_info *mtd, int mode)
{

}

static int jz4780_nand_calculate_ecc_bch(struct mtd_info *mtd,
		const uint8_t *dat, uint8_t *ecc_code)
{
	return 0;
}

static int jz4780_nand_correct_ecc_bch(struct mtd_info *mtd, uint8_t *dat,
		uint8_t *read_ecc, uint8_t *calc_ecc)
{
	return 0;
}

static irqreturn_t jz4780_nand_busy_isr(int irq, void *devid)
{
	nand_flash_if_t *nand_if = devid;

	complete(&nand_if->ready);

	return IRQ_HANDLED;
}

static int request_busy_irq(nand_flash_if_t *nand_if)
{
	int ret;
	unsigned long irq_flags;

	if (!gpio_is_valid(nand_if->busy_gpio))
		return -EINVAL;

	ret = gpio_request(nand_if->busy_gpio,
				label_busy_gpio[nand_if->bank]);
	if (ret)
		return ret;

	gpio_direction_input(ret);
	nand_if->busy_irq = gpio_to_irq(nand_if->busy_gpio);

	irq_flags = nand_if->busy_gpio_low_assert ?
			IRQF_TRIGGER_RISING :
				IRQF_TRIGGER_FALLING;

	ret = request_irq(nand_if->busy_gpio, jz4780_nand_busy_isr,
				irq_flags, label_busy_gpio[nand_if->bank], nand_if);

	if (ret) {
		gpio_free(nand_if->busy_gpio);
		return ret;
	}

	init_completion(&nand_if->ready);

	return 0;
}

#ifdef CONFIG_MTD_CMDLINE_PARTS
static const char *part_probes[] = {"cmdline", NULL};
#endif

static int __devinit jz4780_nand_probe(struct platform_device *pdev)
{
	int ret = 0;
	int bank = 0;
	int i = 0, j = 0, k = 0;
	int nand_dev_id = 0;
	int num_partitions = 0;

	struct nand_chip *chip;
	struct mtd_info *mtd;
	struct mtd_partition *partition;

	struct jz4780_nand *nand;
	struct jz4780_nand_platform_data *pdata;

	nand_flash_if_t *nand_if;
	nand_flash_info_t *nand_info;

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

	nand->num_nand_flash_if = pdata->num_nand_flash_if;
	nand->xfer_type = pdata->xfer_type;
	nand->ecc_type = pdata->ecc_type;

	/*
	 * request GPEMC banks
	 */
	for (i = 0; i < nand->num_nand_flash_if; i++) {
		nand_if = &pdata->nand_flash_if_table[i];
		nand->nand_flash_if_table[i] = nand_if;
		bank = nand_if->bank;
		ret = gpemc_request_cs(&nand_if->cs, bank);
		if (!ret) {
			dev_err(&pdev->dev,
					"Failed to request GPEMC bank%d.\n", bank);
			j = i;
			goto err_release_cs;
		}
	}

	/*
	 * request busy GPIO interrupt
	 */
	switch (nand->xfer_type) {
	case NAND_XFER_CPU_IRQ:
	case NAND_XFER_DMA_IRQ:
		for (i = 0; i < nand->num_nand_flash_if; i++) {
			nand_if = &pdata->nand_flash_if_table[i];
			ret = request_busy_irq(nand_if);
			if (ret) {
				dev_err(&pdev->dev,
						"Failed to request bank%d\n", bank);
				k = i;
				goto err_free_irq;
			}
		}

		break;

	case NAND_XFER_CPU_POLL:
	case NAND_XFER_DMA_POLL:

		break;

	default:
		BUG();
		break;
	}

	/*
	 * request WP GPIO
	 */
	for (i = 0; i < nand->num_nand_flash_if; i++) {
		nand_if = &pdata->nand_flash_if_table[i];
		if (nand_if->wp_gpio < 0)
			continue;

		if (gpio_is_valid(nand_if->wp_gpio)) {
			dev_err(&pdev->dev,
					"Invalid wp GPIO:%d\n",
					nand_if->wp_gpio);
			goto err_free_wp_gpio;
		}

		bank = nand_if->bank;
		ret = gpio_request(nand_if->wp_gpio, label_wp_gpio[bank]);
		if (ret) {
			dev_err(&pdev->dev,
					"Failed to request wp GPIO:%d\n",
					nand_if->wp_gpio);
			goto err_free_wp_gpio;
		}

		/* Write protect enabled by default */
		gpio_direction_output(nand_if->wp_gpio,
				nand_if->wp_gpio_low_assert ^ 1);
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
	 * attach to MTD subsystem
	 */
	chip             = &nand->chip;
	chip->chip_delay = 0;
	mtd              = &nand->mtd;
	mtd->priv        = chip;
	mtd->name        = DRVNAME;
	mtd->owner       = THIS_MODULE;

	switch (nand->ecc_type) {
	case NAND_ECC_TYPE_HW:
		chip->ecc.mode         = NAND_ECC_HW;
		chip->ecc.calculate    = jz4780_nand_calculate_ecc_bch;
		chip->ecc.correct      = jz4780_nand_correct_ecc_bch;
		chip->ecc.hwctl        = jz4780_nand_hwctl;

		break;

	case NAND_ECC_TYPE_SW:
		chip->ecc.mode         = NAND_ECC_SOFT;

		break;

	default :
		BUG();
		break;
	}

	/*
	 * Detect NAND flash chips
	 *
	 * switch to toggle NAND if common NAND detect fail
	 */

	/* step1. detect as common NAND */
	for (bank = 0; bank < nand->num_nand_flash_if; bank++) {
		nand_if = nand->nand_flash_if_table[bank];

		gpemc_set_bank_as_common_nand(&nand_if->cs);
		gpemc_relax_bank_timing(&nand_if->cs);
	}

	if (nand_scan_ident(mtd, nand->num_nand_flash_if,
			nand->nand_flash_table)) {
		/* step2. common NAND detect fail, retry as toggle NAND */
		for (bank = 0; bank < nand->num_nand_flash_if; bank++) {
			nand_if = nand->nand_flash_if_table[bank];

			gpemc_set_bank_as_toggle_nand(&nand_if->cs);
			gpemc_relax_bank_timing(&nand_if->cs);
		}

		if (nand_scan_ident(mtd, nand->num_nand_flash_if,
				nand->nand_flash_table)) {
			ret = -ENXIO;
			dev_err(&pdev->dev, "Failed to detect NAND flash.\n");
			goto err_free_wp_gpio;
		}
	}

	/*
	 * post configure bank timing by detected NAND device
	 */

	/* step1. read devid */
	chip->select_chip(mtd, 0);
	chip->cmdfunc(mtd, NAND_CMD_RESET, -1, -1);
	chip->cmdfunc(mtd, NAND_CMD_READID, 0x00, -1);
	nand_dev_id = chip->read_byte(mtd);
	nand_dev_id = chip->read_byte(mtd);

	/* step2. find NAND flash info */
	for (bank = 0; bank < nand->num_nand_flash_info; bank++)
		if (nand_dev_id ==
				nand->nand_flash_info_table[bank].nand_dev_id)
			break;

	if (bank == nand->num_nand_flash_info) {
		dev_err(&pdev->dev,
				"Failed to find NAND info for devid:%d\n", nand_dev_id);
		goto err_free_wp_gpio;
	}

	/* step3. configure bank timing */
	nand_info = &nand->nand_flash_info_table[bank];
	switch (nand_info->type) {
	case BANK_TYPE_NAND:
		for (bank = 0; bank < nand->num_nand_flash_info; bank ++) {
			nand_if = nand->nand_flash_if_table[bank];

			gpemc_fill_timing_from_nand(&nand_if->cs,
					&nand_info->nand_timing.common_nand_timing);
		}
		break;

	case BANK_TYPE_TOGGLE:
		for (bank = 0; bank < nand->num_nand_flash_info; bank ++) {
			nand_if = nand->nand_flash_if_table[bank];

			gpemc_fill_timing_from_toggle(&nand_if->cs,
					&nand_info->nand_timing.toggle_nand_timing);
		}
		break;

	default:
		BUG();
		break;
	}

	/*
	 * post initialize ECC control
	 */

	/* step1. configure ECC step */
	chip->ecc.size  = nand_info->ecc_step.data_size;
	chip->ecc.bytes = nand_info->ecc_step.ecc_size;

	/* step2. generate  ECC layout */


	/*
	 * second phase NAND scan
	 */
	if (nand_scan_tail(mtd)) {
		ret = -ENXIO;
		goto err_free_wp_gpio;
	}

	/*
	 * MTD register
	 */

	/* step1. command line probe */
#ifdef CONFIG_MTD_CMDLINE_PARTS
	num_partitions = parse_mtd_partitions(mtd, part_probes,
						&partition_info, 0);
#endif

	if (num_partitions <= 0 && pdata->part_table) {
		num_partitions = pdata->num_part;
		partition = pdata->part_table;
	} else {
		num_partitions = 0;
		partition = NULL;
	}

	ret = mtd_device_register(mtd, partition, num_partitions);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add mtd device\n");
		goto err_free_wp_gpio;
	}

	platform_set_drvdata(pdev, nand);

	dev_info(&pdev->dev,
		"Successfully registered JZ4780 SoC NAND controller driver.\n");

	return 0;

err_free_wp_gpio:
	for (bank = 0; bank < i; bank++) {
		nand_if = &pdata->nand_flash_if_table[bank];

		gpio_free(nand_if->wp_gpio);
	}

err_free_irq:
	for (bank = 0; bank < k; bank++) {
		nand_if = &pdata->nand_flash_if_table[bank];

		free_irq(nand_if->busy_irq, nand_if);
		gpio_free(nand_if->busy_gpio);
	}

err_release_cs:
	for (bank = 0; bank < j; bank++) {
		nand_if = &pdata->nand_flash_if_table[bank];

		gpemc_release_cs(&nand_if->cs);
	}

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
