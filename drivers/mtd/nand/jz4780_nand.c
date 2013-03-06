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
#include <linux/delay.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include <linux/gpio.h>

#include <soc/gpemc.h>
#include <soc/bch.h>
#include <mach/jz4780_nand.h>

#define DRVNAME "jz4780-nand"

#define MAX_NUM_NAND_IF   7
#define MAX_RB_DELAY      50

/*
 * ******************************************************
 * 	NAND flash device name & ID
 * ******************************************************
 */

/*
 * !!!Caution
 * "K9GBG08U0A" may be with one of two ID sequences:
 * "EC D7 94 76" --- this one can not be detected properly
 *
 * "EC D7 94 7A" --- this one can be detected properly
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
			1024, 24,
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
	struct nand_ecclayout ecclayout;

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

static struct jz4780_nand *mtd_to_jz4780_nand(struct mtd_info *mtd)
{
	return container_of(mtd, struct jz4780_nand, mtd);
}

static int jz4780_nand_ready(nand_flash_if_t *nand_if)
{
	int low_assert;
	int gpio;

	low_assert = nand_if->busy_gpio_low_assert;
	gpio = nand_if->busy_gpio;

	return !(gpio_get_value_cansleep(gpio) ^ low_assert);
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

static int jz4780_nand_dev_ready(struct mtd_info *mtd)
{
	struct jz4780_nand *nand;
	nand_flash_if_t *nand_if;

	int ret = 0;

	nand = mtd_to_jz4780_nand(mtd);
	nand_if = nand->nand_flash_if_table[nand->curr_nand_if];

	if (nand->xfer_type == NAND_XFER_CPU_IRQ ||
		nand->xfer_type == NAND_XFER_DMA_IRQ) {

		wait_for_completion(&nand_if->ready);
	} else {

		if (nand_if->wp_gpio > 0) {
			ret = jz4780_nand_ready(nand_if);
		} else {
			udelay(MAX_RB_DELAY);
			ret = 1;
		}
	}

	return ret;
}

static void jz4780_nand_select_chip(struct mtd_info *mtd, int chip)
{
	struct nand_chip *this;
	struct jz4780_nand *nand;
	nand_flash_if_t *nand_if;

	this = mtd->priv;
	nand = mtd_to_jz4780_nand(mtd);

	/* deselect previous NAND flash chip */
	nand_if = nand->nand_flash_if_table[nand->curr_nand_if];
	gpemc_enable_nand_flash(&nand_if->cs, 0);

	if (chip != -1) {
		/* select new NAND flash chip */
		nand->curr_nand_if = chip;
		nand_if = nand->nand_flash_if_table[nand->curr_nand_if];
		gpemc_enable_nand_flash(&nand_if->cs, 1);

		this->IO_ADDR_R = nand_if->cs.io_nand_dat;
		this->IO_ADDR_W = nand_if->cs.io_nand_dat;
	}
}

static void
jz4780_nand_cmd_ctrl(struct mtd_info *mtd, int cmd, unsigned int ctrl)
{
	struct nand_chip *chip;
	struct jz4780_nand *nand;
	nand_flash_if_t *nand_if;

	if (cmd != NAND_CMD_NONE) {
		chip = mtd->priv;
		nand = mtd_to_jz4780_nand(mtd);
		nand_if = nand->nand_flash_if_table[nand->curr_nand_if];

		if (ctrl & NAND_CLE) {
			chip->IO_ADDR_R = nand_if->cs.io_nand_cmd;
			chip->IO_ADDR_W = nand_if->cs.io_nand_cmd;

		} else if (ctrl & NAND_ALE) {
			chip->IO_ADDR_R = nand_if->cs.io_nand_addr;
			chip->IO_ADDR_W = nand_if->cs.io_nand_addr;

		} else {
			chip->IO_ADDR_R = nand_if->cs.io_nand_dat;
			chip->IO_ADDR_W = nand_if->cs.io_nand_dat;
		}
	}
}

static void jz4780_nand_ecc_hwctl(struct mtd_info *mtd, int mode)
{

}

static int jz4780_nand_ecc_calculate_bch(struct mtd_info *mtd,
		const uint8_t *dat, uint8_t *ecc_code)
{
	struct nand_chip *chip;
	struct jz4780_nand *nand;
	bch_request_t *req;

	chip = mtd->priv;
	nand = mtd_to_jz4780_nand(mtd);
	req  = &nand->bch_req;

	req->blksz    = chip->ecc.size;
	req->raw_data = dat;
	req->dev      = &nand->pdev->dev;
	req->type     = BCH_REQ_ENCODE;
	req->ecc_data = ecc_code;

	bch_request_submit(req);

	wait_for_completion(&nand->bch_req_done);


	return 0;
}

static int jz4780_nand_ecc_correct_bch(struct mtd_info *mtd, uint8_t *dat,
		uint8_t *read_ecc, uint8_t *calc_ecc)
{
	struct nand_chip *chip;
	struct jz4780_nand *nand;
	bch_request_t *req;

	chip = mtd->priv;
	nand = mtd_to_jz4780_nand(mtd);
	req  = &nand->bch_req;

	req->blksz    = chip->ecc.size;
	req->raw_data = dat;
	req->dev      = &nand->pdev->dev;
	req->type     = BCH_REQ_DECODE_CORRECT;
	req->ecc_data = read_ecc;

	bch_request_submit(req);

	wait_for_completion(&nand->bch_req_done);

	if (req->ret_val == BCH_RET_OK) {
		return req->cnt_ecc_errors;
	}

	return -1;
}

void jz4780_nand_bch_req_complete(struct bch_request *req)
{
	struct jz4780_nand *nand;

	nand = container_of(req, struct jz4780_nand, bch_req);

	complete(&nand->bch_req_done);
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
				"Invalid wp GPIO:%d\n", nand_if->wp_gpio);
			goto err_free_wp_gpio;
		}

		bank = nand_if->bank;
		ret = gpio_request(nand_if->wp_gpio, label_wp_gpio[bank]);
		if (ret) {
			dev_err(&pdev->dev,
				"Failed to request wp GPIO:%d\n", nand_if->wp_gpio);
			goto err_free_wp_gpio;
		}

		gpio_direction_output(nand_if->wp_gpio, 0);

		/* Write protect disabled by default */
		jz4780_nand_enable_wp(nand_if, 0);
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
	chip              = &nand->chip;
	chip->chip_delay  = 0;
	chip->dev_ready   = jz4780_nand_dev_ready;
	chip->select_chip = jz4780_nand_select_chip;
	chip->cmd_ctrl    = jz4780_nand_cmd_ctrl;

	/*
	 * TODO: skip BBT scan when i completed FS-xburst-tools
	 * chip->option |= NAND_SKIP_BBTSCAN;
	 */

	mtd              = &nand->mtd;
	mtd->priv        = chip;
	mtd->name        = DRVNAME;
	mtd->owner       = THIS_MODULE;

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
			"Failed to find NAND info for devid:%d\n",
				nand_dev_id);
		goto err_free_wp_gpio;
	}

	/* step3. configure bank timing */
	nand_info = &nand->nand_flash_info_table[bank];
	switch (nand_info->type) {
	case BANK_TYPE_NAND:
		for (bank = 0; bank < nand->num_nand_flash_info; bank++) {
			nand_if = nand->nand_flash_if_table[bank];

			gpemc_fill_timing_from_nand(&nand_if->cs,
					&nand_info->nand_timing.common_nand_timing);
			gpemc_config_bank_timing(&nand_if->cs);
		}

		break;

	case BANK_TYPE_TOGGLE:
		for (bank = 0; bank < nand->num_nand_flash_info; bank++) {
			nand_if = nand->nand_flash_if_table[bank];

			gpemc_fill_timing_from_toggle(&nand_if->cs,
					&nand_info->nand_timing.toggle_nand_timing);
			gpemc_config_bank_timing(&nand_if->cs);
		}

		break;

	default:
		BUG();

		break;
	}

	/*
	 * initialize ECC control
	 */

	/* step1. configure ECC step */
	switch (nand->ecc_type) {
	case NAND_ECC_SOFT_BCH:
		chip->ecc.mode         = NAND_ECC_SOFT;
		chip->ecc.size         = nand_info->ecc_step.data_size;
		chip->ecc.bytes        =
			(13 * (nand_info->ecc_step.ecc_size * 8 / 14) + 7) / 8;

		break;

	case NAND_ECC_TYPE_HW:
		chip->ecc.mode         = NAND_ECC_HW;
		chip->ecc.calculate    = jz4780_nand_ecc_calculate_bch;
		chip->ecc.correct      = jz4780_nand_ecc_correct_bch;
		chip->ecc.hwctl        = jz4780_nand_ecc_hwctl;
		chip->ecc.size         = nand_info->ecc_step.data_size;
		chip->ecc.bytes        = nand_info->ecc_step.ecc_size;

		/*
		 * TODO assign ecc.strength when completed MTD update
		 *
		 * chip->ecc.strength  = ecc_level / 2;
		 *
		 */

		nand->bch_req.complete  = jz4780_nand_bch_req_complete;
		nand->bch_req.ecc_level =
			nand_info->ecc_step.ecc_size * 8 / 14;

#ifdef BCH_REQ_ALLOC_ECC_DATA_BUFFER
		nand->bch_req.ecc_data  = kzalloc(MAX_ECC_DATA_SIZE,
				GFP_KERNEL);
		if (!nand->bch_req.ecc_data) {
			dev_err(&pdev->dev,
				"Failed to allocate ECC ecc_data buffer\n");
			ret = -ENOMEM;

			goto err_free_wp_gpio;
		}
#endif

		nand->bch_req.ecc_data_width = sizeof(uint32_t);
		nand->bch_req.raw_data_width = sizeof(uint32_t);

		nand->bch_req.errrept_data = kzalloc(MAX_ERRREPT_DATA_SIZE,
				GFP_KERNEL);
		if (!nand->bch_req.errrept_data) {
			dev_err(&pdev->dev,
				"Failed to allocate ECC errrept_data buffer\n");
			ret = -ENOMEM;

			kfree(nand->bch_req.ecc_data);
			goto err_free_wp_gpio;
		}

		init_completion(&nand->bch_req_done);

		break;

	default :
		BUG();

		break;
	}

	/* step2. generate ECC layout */
	nand->ecclayout.eccbytes =
		mtd->writesize / chip->ecc.size * chip->ecc.bytes;

	for (bank = 0; bank < nand->ecclayout.eccbytes; bank++)
		nand->ecclayout.eccpos[bank] = chip->badblockpos + bank + 1;

	nand->ecclayout.oobfree->offset =
			nand->ecclayout.eccbytes + chip->badblockpos + 1;
	nand->ecclayout.oobfree->length =
		mtd->oobsize - (nand->ecclayout.eccbytes + chip->badblockpos + 1);

	chip->ecc.layout = &nand->ecclayout;

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
			&partition, 0);
#endif

	/* step2. board specific parts info override */
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
