/*
 *  Copyright (C) 2013 Fighter Sun <wanmyqawdr@126.com>
 *  JZ4780 SoC NAND controller driver
 *
 *
 *  TODO:
 *  relocate hot points functions
 *  dual threads soft BCH ECC
 *  support toggle NAND
 *
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
#include <linux/init.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/completion.h>
#include <linux/dmaengine.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/kallsyms.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>

#include <linux/gpio.h>

#include <soc/gpemc.h>
#include <soc/bch.h>
#include <mach/jzdma.h>
#include <mach/jz4780_nand.h>

#define DRVNAME "jz4780-nand"

/*
 * this is ugly but got great speed gain
 * this stuff implement none-interrupt DMA transfer
 * and raw NAND read speed gain is about 40%
 */
#define CONFIG_JZ4780_NAND_USE_RAW_DMA

#define MAX_RB_DELAY_US             50
#define MAX_RB_TIMOUT_MS            50

#define MAX_RESET_DELAY_MS          50

#define MAX_DMA_TRANSFER_TIMOUT_MS  1000

/* root entry to debug */
static struct dentry *debugfs_root;

/*
 * ******************************************************
 * 	NAND flash chip name & ID
 * ******************************************************
 *
 *
 * !!!Caution
 * "K9GBG08U0A" may be with one of two ID sequences:
 * "EC D7 94 76" --- this one can not be detected properly
 *
 * "EC D7 94 7A" --- this one can be detected properly
 */
#define NAND_FLASH_K9GBG08U0A_NANE           "K9GBG08U0A"
#define NAND_FLASH_K9GBG08U0A_ID             0xd7


/*
 * Detected by rules of ONFI v2.2
 */
#define NAND_FLASH_MT29F32G08CBACAWP         "MT29F32G08CBACAWP"
#define NAND_FLASH_MT29F32G08CBACAWP_ID      0x68

/*
 * ******************************************************
 * 	Supported NAND flash chips
 * ******************************************************
 *
 */
static struct nand_flash_dev builtin_nand_flash_table[] = {
	/*
	 * These are the new chips with large page size. The pagesize and the
	 * erasesize is determined from the extended id bytes
	 */

	/*
	 * K9GBG08U0A
	 *
	 * !!!Caution
	 * please do not use busy pin IRQ over "K9GBG08U0A"
	 * the chip is running under very rigorous timings
	 */
	{
		NAND_FLASH_K9GBG08U0A_NANE, NAND_FLASH_K9GBG08U0A_ID,
		0, 4096, 0, LP_OPTIONS
	},



	/*
	 * MT29F32G08CBACA(WP) --- support ONFI v2.2
	 *
	 * it was detected by rules of ONFI v2.2
	 * so you can complete remove this match entry
	 *
	 */
	{
		NAND_FLASH_MT29F32G08CBACAWP, NAND_FLASH_MT29F32G08CBACAWP_ID,
		0, 4096, 0, LP_OPTIONS
	},


	{NULL,}
};


/*
 * ******************************************************
 * 	Supported NAND flash chips' timings parameters table
 * 	it extents the upper table
 * ******************************************************
 */
static nand_flash_info_t builtin_nand_info_table[] = {
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
			1024, 28, 0,
			12, 5, 12, 5, 20, 5, 12, 5, 12, 10,
			25, 25, 300, 100, 100, 300, 12, 20, 300, 100,
			100, 200 * 1000, 1 * 1000, 200 * 1000,
			5 * 1000 * 1000, BUS_WIDTH_8)
	},

	{
		/*
		 * Datasheet of MT29F32G08CBACA(WP), Rev-E, P109, Table-17
		 * ECC : 24bit/1080bytes
		 *
		 * we assign 28bit/1KB here, the overs are usable when
		 * bitflips occur in OOB area
		 */
		COMMON_NAND_CHIP_INFO(
			NAND_FLASH_MT29F32G08CBACAWP, NAND_FLASH_MT29F32G08CBACAWP_ID,
			1024, 28, 0,
			10, 5, 10, 5, 15, 5, 7, 5, 10, 7,
			20, 20, 70, 100, 60, 60, 10, 20, 0, 100,
			100, 100 * 1000, 0, 0, 0, BUS_WIDTH_8)
	},
};


const char *label_wp_gpio[] = {
	DRVNAME"-THIS-IS-A-BUG",
	"bank1-nand-wp",
	"bank2-nand-wp",
	"bank3-nand-wp",
	"bank4-nand-wp",
	"bank5-nand-wp",
	"bank6-nand-wp",
};

const char *label_busy_gpio[] = {
	DRVNAME"-THIS-IS-A-BUG",
	"bank1-nand-busy",
	"bank2-nand-busy",
	"bank3-nand-busy",
	"bank4-nand-busy",
	"bank5-nand-busy",
	"bank6-nand-busy",
};

struct jz4780_nand {
	struct mtd_info mtd;

	struct nand_chip chip;
	struct nand_ecclayout ecclayout;

	nand_flash_if_t *nand_flash_if_table[MAX_NUM_NAND_IF];
	int num_nand_flash_if;
	int curr_nand_flash_if;

	bch_request_t bch_req;
	struct completion bch_req_done;
	nand_ecc_type_t ecc_type;

	struct nand_flash_dev *nand_flash_table;
	int num_nand_flash;

	nand_flash_info_t *curr_nand_flash_info;

	nand_xfer_type_t xfer_type;
	int use_dma;
	int busy_poll;

	struct {
		struct dma_chan *chan;
		struct dma_slave_config cfg;
		enum jzdma_type type;
		struct scatterlist sg;

		struct completion comp;
	} dma_pipe;

	struct jz4780_nand_platform_data *pdata;
	struct platform_device *pdev;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_entry;
#endif
};

static struct jz4780_nand *mtd_to_jz4780_nand(struct mtd_info *mtd)
{
	return container_of(mtd, struct jz4780_nand, mtd);
}

static int jz4780_nand_chip_is_ready(nand_flash_if_t *nand_if)
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

static int jz4780_nand_dev_is_ready(struct mtd_info *mtd)
{
	struct jz4780_nand *nand;
	nand_flash_if_t *nand_if;

	int ret = 0;

	nand = mtd_to_jz4780_nand(mtd);
	nand_if = nand->nand_flash_if_table[nand->curr_nand_flash_if];

	if (!nand->busy_poll) {

		ret = wait_for_completion_timeout(&nand_if->ready,
				msecs_to_jiffies(nand_if->ready_timout_ms));

		WARN(!ret, "%s: Timeout when wait NAND chip ready for bank%d,"
				" when issue command: 0x%x\n",
				dev_name(&nand->pdev->dev),
				nand_if->bank,
				nand_if->curr_command);

		ret = 1;
	} else {

		if (nand_if->busy_gpio != -1) {
			ret = jz4780_nand_chip_is_ready(nand_if);
		} else {
			udelay(MAX_RB_DELAY_US);
			ret = 1;
		}
	}

	/*
	 * Apply this short delay always to ensure that we do wait tRR in
	 * any case on any machine.
	 */
	ndelay(100);

	return ret;
}

static void jz4780_nand_select_chip(struct mtd_info *mtd, int chip)
{
	struct nand_chip *this;
	struct jz4780_nand *nand;
	nand_flash_if_t *nand_if;

	this = mtd->priv;
	nand = mtd_to_jz4780_nand(mtd);

	if (chip == -1) {
		/*
		 * Apply this short delay always to ensure that we do wait tCH in
		 * any case on any machine.
		 */
		ndelay(100);

		/* deselect current NAND flash chip */
		nand_if = nand->nand_flash_if_table[nand->curr_nand_flash_if];
		gpemc_enable_nand_flash(&nand_if->cs, 0);
	} else {
		/* select new NAND flash chip */
		nand_if = nand->nand_flash_if_table[chip];
		gpemc_enable_nand_flash(&nand_if->cs, 1);

		this->IO_ADDR_R = nand_if->cs.io_nand_dat;
		this->IO_ADDR_W = nand_if->cs.io_nand_dat;

		/* reconfigure DMA */
		if (nand->use_dma &&
				nand->curr_nand_flash_if != chip) {
			nand->dma_pipe.cfg.src_addr =
					(dma_addr_t)CPHYSADDR(this->IO_ADDR_R);
			nand->dma_pipe.cfg.dst_addr =
					(dma_addr_t)CPHYSADDR(this->IO_ADDR_W);
			dmaengine_slave_config(nand->dma_pipe.chan, &nand->dma_pipe.cfg);
		}

		nand->curr_nand_flash_if = chip;

		/*
		 * Apply this short delay always to ensure that we do wait tCS in
		 * any case on any machine.
		 */
		ndelay(100);
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
		nand_if = nand->nand_flash_if_table[nand->curr_nand_flash_if];

		if (ctrl & NAND_CLE) {
			writeb(cmd, nand_if->cs.io_nand_cmd);
		} else if (ctrl & NAND_ALE) {
			writeb(cmd, nand_if->cs.io_nand_addr);
		}
	}
}

static void jz4780_nand_ecc_hwctl(struct mtd_info *mtd, int mode)
{
	/*
	 * TODO: need consider NAND r/w state ?
	 */
}

static void jz4780_nand_command(struct mtd_info *mtd, unsigned int command,
			 int column, int page_addr)
{
	register struct nand_chip *chip = mtd->priv;
	int ctrl = NAND_CTRL_CLE | NAND_CTRL_CHANGE;

	int old_busy_poll;
	struct jz4780_nand *nand;
	nand_flash_if_t *nand_if;
	nand_flash_info_t *nand_info;

	nand = mtd_to_jz4780_nand(mtd);
	old_busy_poll = nand->busy_poll;

	nand_if = nand->nand_flash_if_table[nand->curr_nand_flash_if];
	nand_if->curr_command = command;

	nand_info = nand->curr_nand_flash_info;

	/*
	 * R/B# polling policy
	 */
	switch (command) {
	case NAND_CMD_READID:
	case NAND_CMD_RESET:
		if (!nand->busy_poll)
			nand->busy_poll = 1;

		break;
	}

	/* Write out the command to the device */
	if (command == NAND_CMD_SEQIN) {
		int readcmd;

		if (column >= mtd->writesize) {
			/* OOB area */
			column -= mtd->writesize;
			readcmd = NAND_CMD_READOOB;
		} else if (column < 256) {
			/* First 256 bytes --> READ0 */
			readcmd = NAND_CMD_READ0;
		} else {
			column -= 256;
			readcmd = NAND_CMD_READ1;
		}
		chip->cmd_ctrl(mtd, readcmd, ctrl);
		ctrl &= ~NAND_CTRL_CHANGE;
	}
	chip->cmd_ctrl(mtd, command, ctrl);

	/* Address cycle, when necessary */
	ctrl = NAND_CTRL_ALE | NAND_CTRL_CHANGE;
	/* Serially input address */
	if (column != -1) {
		chip->cmd_ctrl(mtd, column, ctrl);
		ctrl &= ~NAND_CTRL_CHANGE;
	}
	if (page_addr != -1) {
		chip->cmd_ctrl(mtd, page_addr, ctrl);
		ctrl &= ~NAND_CTRL_CHANGE;
		chip->cmd_ctrl(mtd, page_addr >> 8, ctrl);
		/* One more address cycle for devices > 32MiB */
		if (chip->chipsize > (32 << 20))
			chip->cmd_ctrl(mtd, page_addr >> 16, ctrl);

		switch (nand_if->curr_command) {
		case NAND_CMD_SEQIN:
			/*
			 * Apply this short delay to meet Tadl
			 */
			if (nand_info) {
				if (nand_info->type == BANK_TYPE_NAND)
					ndelay(nand_info->nand_timing.
							common_nand_timing.busy_wait_timing.Tadl);
				else {
					/*
					 * TODO
					 */
					WARN(1, "TODO: implement Tadl delay\n");
				}
			}
			break;

		case NAND_CMD_RNDIN:
			/*
			 * Apply this short delay to meet Tcwaw
			 */
			if (nand_info) {
				if (nand_info->type == BANK_TYPE_NAND)
					ndelay(nand_info->nand_timing.
							common_nand_timing.busy_wait_timing.Tcwaw);
				else {
					/*
					 * TODO
					 */
					WARN(1, "TODO: implement Tcwaw delay\n");
				}
			}
			break;

		default:
			break;
		}
	}
	chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);

	/*
	 * Program and erase have their own busy handlers status and sequential
	 * in needs no delay
	 */
	switch (command) {

	case NAND_CMD_PAGEPROG:
	case NAND_CMD_ERASE1:
	case NAND_CMD_ERASE2:
	case NAND_CMD_SEQIN:
	case NAND_CMD_STATUS:
		return;

	case NAND_CMD_RESET:
		if (chip->dev_ready) {
			/*
			 * Apply this short delay always to ensure that we do wait tRST in
			 * any case on any machine.
			 */
			mdelay(MAX_RESET_DELAY_MS);
			break;
		}

		udelay(chip->chip_delay);
		chip->cmd_ctrl(mtd, NAND_CMD_STATUS,
			       NAND_CTRL_CLE | NAND_CTRL_CHANGE);
		chip->cmd_ctrl(mtd,
			       NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);
		while (!(chip->read_byte(mtd) & NAND_STATUS_READY))
				;
		return;

		/* This applies to read commands */
	default:
		/*
		 * If we don't have access to the busy pin, we apply the given
		 * command delay
		 */
		if (!chip->dev_ready) {
			udelay(chip->chip_delay);
			return;
		}
	}
	/*
	 * Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine.
	 */
	ndelay(100);

	nand_wait_ready(mtd);

	nand->busy_poll = old_busy_poll;
}

static void jz4780_nand_command_readoob_lp(struct mtd_info *mtd,
		int column, int page_addr)
{
	register struct nand_chip *chip = mtd->priv;
	unsigned int command = NAND_CMD_READ0;

	chip->cmd_ctrl(mtd, command & 0xff,
			   NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);

	if (column != -1 || page_addr != -1) {
		int ctrl = NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE;

		/* Serially input address */
		if (column != -1) {
			/*
			 * Datesheet K9GBG08U0A, V1.3, P35, S4.8
			 * Note1: A0 column address should be fixed to "0"
			 * in 1-plane read operation.
			 */
			chip->cmd_ctrl(mtd, 0, ctrl);
			ctrl &= ~NAND_CTRL_CHANGE;
			chip->cmd_ctrl(mtd, 0, ctrl);
		}
		if (page_addr != -1) {
			chip->cmd_ctrl(mtd, page_addr, ctrl);
			chip->cmd_ctrl(mtd, page_addr >> 8,
					   NAND_NCE | NAND_ALE);
			/* One more address cycle for devices > 128MiB */
			if (chip->chipsize > (128 << 20))
				chip->cmd_ctrl(mtd, page_addr >> 16,
						   NAND_NCE | NAND_ALE);
		}
	}

	chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);

	chip->cmd_ctrl(mtd, NAND_CMD_READSTART,
			   NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
	chip->cmd_ctrl(mtd, NAND_CMD_NONE,
			   NAND_NCE | NAND_CTRL_CHANGE);

	if (!chip->dev_ready) {
		udelay(chip->chip_delay);
		return;
	}

	/*
	 * Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine.
	 */
	ndelay(100);

	nand_wait_ready(mtd);

	/*
	 * Emulate NAND_CMD_RNDOUT seek to OOB column
	 */
	chip->cmdfunc(mtd, NAND_CMD_RNDOUT, column, page_addr);
}

static void jz4780_nand_command_lp(struct mtd_info *mtd, unsigned int command,
			    int column, int page_addr)
{
	register struct nand_chip *chip = mtd->priv;

	struct jz4780_nand *nand;
	nand_flash_if_t *nand_if;
	nand_flash_info_t *nand_info;

	nand = mtd_to_jz4780_nand(mtd);
	nand_if = nand->nand_flash_if_table[nand->curr_nand_flash_if];
	nand_if->curr_command = command;

	nand_info = nand->curr_nand_flash_info;

	/* Emulate NAND_CMD_READOOB */
	if (command == NAND_CMD_READOOB) {
		column += mtd->writesize;
		return jz4780_nand_command_readoob_lp(mtd, column, page_addr);
	}

	/* Command latch cycle */
	chip->cmd_ctrl(mtd, command & 0xff,
		       NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);

	if (column != -1 || page_addr != -1) {
		int ctrl = NAND_CTRL_CHANGE | NAND_NCE | NAND_ALE;

		/* Serially input address */
		if (column != -1) {
			chip->cmd_ctrl(mtd, column, ctrl);
			ctrl &= ~NAND_CTRL_CHANGE;
			chip->cmd_ctrl(mtd, column >> 8, ctrl);
		}
		if (page_addr != -1) {
			chip->cmd_ctrl(mtd, page_addr, ctrl);
			chip->cmd_ctrl(mtd, page_addr >> 8,
				       NAND_NCE | NAND_ALE);
			/* One more address cycle for devices > 128MiB */
			if (chip->chipsize > (128 << 20))
				chip->cmd_ctrl(mtd, page_addr >> 16,
					       NAND_NCE | NAND_ALE);

			switch (nand_if->curr_command) {
			case NAND_CMD_SEQIN:
				/*
				 * Apply this short delay to meet Tadl
				 */
				if (nand_info) {
					if (nand_info->type == BANK_TYPE_NAND)
						ndelay(nand_info->nand_timing.
								common_nand_timing.busy_wait_timing.Tadl);
					else {
						/*
						 * TODO
						 */
						WARN(1, "TODO: implement Tadl delay\n");
					}
				}
				break;

			case NAND_CMD_RNDIN:
				/*
				 * Apply this short delay to meet Tcwaw
				 */
				if (nand_info) {
					if (nand_info->type == BANK_TYPE_NAND)
						ndelay(nand_info->nand_timing.
								common_nand_timing.busy_wait_timing.Tcwaw);
					else {
						/*
						 * TODO
						 */
						WARN(1, "TODO: implement Tcwaw delay\n");
					}
				}
				break;

			default:
				break;
			}
		}
	}
	chip->cmd_ctrl(mtd, NAND_CMD_NONE, NAND_NCE | NAND_CTRL_CHANGE);

	/*
	 * Program and erase have their own busy handlers status, sequential
	 * in, and deplete1 need no delay.
	 */
	switch (command) {

	case NAND_CMD_CACHEDPROG:
	case NAND_CMD_PAGEPROG:
	case NAND_CMD_ERASE1:
	case NAND_CMD_ERASE2:
	case NAND_CMD_SEQIN:
	case NAND_CMD_RNDIN:
	case NAND_CMD_STATUS:
	case NAND_CMD_DEPLETE1:
		return;

	case NAND_CMD_STATUS_ERROR:
	case NAND_CMD_STATUS_ERROR0:
	case NAND_CMD_STATUS_ERROR1:
	case NAND_CMD_STATUS_ERROR2:
	case NAND_CMD_STATUS_ERROR3:
		/* Read error status commands require only a short delay */
		udelay(chip->chip_delay);
		return;

	case NAND_CMD_RESET:
		if (chip->dev_ready) {
			/*
			 * Apply this short delay always to ensure that we do wait tRST in
			 * any case on any machine.
			 */
			mdelay(MAX_RESET_DELAY_MS);
			break;
		}

		udelay(chip->chip_delay);
		chip->cmd_ctrl(mtd, NAND_CMD_STATUS,
			       NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
		chip->cmd_ctrl(mtd, NAND_CMD_NONE,
			       NAND_NCE | NAND_CTRL_CHANGE);
		while (!(chip->read_byte(mtd) & NAND_STATUS_READY))
				;
		return;

	case NAND_CMD_RNDOUT:
		/* No ready / busy check necessary */
		chip->cmd_ctrl(mtd, NAND_CMD_RNDOUTSTART,
			       NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
		/*
		 * Apply this short delay to meet Twhr2
		 */
		if (nand_info) {
			if (nand_info->type == BANK_TYPE_NAND)
				ndelay(nand_info->nand_timing.
						common_nand_timing.busy_wait_timing.Twhr2);
			else {
				/*
				 * TODO
				 */
				WARN(1, "TODO: implement Twhr2 delay\n");
			}
		}

		chip->cmd_ctrl(mtd, NAND_CMD_NONE,
			       NAND_NCE | NAND_CTRL_CHANGE);
		return;

	case NAND_CMD_READ0:
		chip->cmd_ctrl(mtd, NAND_CMD_READSTART,
			       NAND_NCE | NAND_CLE | NAND_CTRL_CHANGE);
		chip->cmd_ctrl(mtd, NAND_CMD_NONE,
			       NAND_NCE | NAND_CTRL_CHANGE);

		/* This applies to read commands */
	default:
		/*
		 * If we don't have access to the busy pin, we apply the given
		 * command delay.
		 */
		if (!chip->dev_ready) {
			udelay(chip->chip_delay);
			return;
		}
	}

	/*
	 * Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine.
	 */
	ndelay(100);

	nand_wait_ready(mtd);
}

static int jz4780_nand_relocate_hot_to_tcsm(struct jz4780_nand *nand)
{
	/*
	 * TODO:
	 * copy hot points functions to TCSM
	 */
	return 0;
}

static void jz4780_nand_unlocate_hot_from_tcsm(struct jz4780_nand *nand)
{
	/*
	 * TODO: need tcsm_put
	 */
}

static bool jz4780_nand_dma_filter(struct dma_chan *chan, void *filter_param)
{
	struct jz4780_nand *nand = container_of(filter_param,
			struct jz4780_nand, dma_pipe);
	/*
	 * chan_id must 30, also is PHY channel1,
	 * i did some speical modification for channel1
	 * of ingenic dmaenginc codes.
	 *
	 * TODO: make it generic
	 */
	return (int)chan->private == (int)nand->dma_pipe.type &&
			chan->chan_id == 30;
}

static int jz4780_nand_request_dma(struct jz4780_nand* nand)
{
	dma_cap_mask_t mask;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	nand->dma_pipe.type = JZDMA_REQ_AUTO_TXRX;

	nand->dma_pipe.chan = dma_request_channel(mask,
			jz4780_nand_dma_filter, &nand->dma_pipe);
	if (!nand->dma_pipe.chan)
		return -ENXIO;

	init_completion(&nand->dma_pipe.comp);


	/*
	 * TODO remove these ugly
	 */
#ifdef CONFIG_JZ4780_NAND_USE_RAW_DMA
	{
		struct jzdma_master *dma_master;
		struct jzdma_channel *dma_channel;
		unsigned int reg_dmac;
		dma_channel = to_jzdma_chan(nand->dma_pipe.chan);
		dma_master = dma_channel->master;

		/*
		 * basic configure DMA channel
		 */
		reg_dmac = readl(dma_master->iomem + DMAC);
		if (!(reg_dmac & BIT(1))) {
			/*
			 * enable special channel0,1
			 */
			writel(reg_dmac | BIT(1), dma_master->iomem + DMAC);

			dev_info(&nand->pdev->dev, "enable DMA"
					" special channel<0,1>\n");
		}
	}
#endif

	return 0;
}

static int jz4780_nand_ecc_calculate_bch(struct mtd_info *mtd,
		const uint8_t *dat, uint8_t *ecc_code)
{
	struct nand_chip *chip;
	struct jz4780_nand *nand;
	bch_request_t *req;

	chip = mtd->priv;

	if (chip->state == FL_READING)
		return 0;

	nand = mtd_to_jz4780_nand(mtd);
	req  = &nand->bch_req;

	req->raw_data = dat;
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

	req->raw_data = dat;
	req->type     = BCH_REQ_DECODE_CORRECT;
	req->ecc_data = read_ecc;

	bch_request_submit(req);

	wait_for_completion(&nand->bch_req_done);

	if (req->ret_val == BCH_RET_OK)
		return req->cnt_ecc_errors;

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

static int request_busy_poll(nand_flash_if_t *nand_if)
{
	int ret;

	if (!gpio_is_valid(nand_if->busy_gpio))
		return -EINVAL;

	ret = gpio_request(nand_if->busy_gpio,
				label_busy_gpio[nand_if->bank]);
	if (ret)
		return ret;

	ret = gpio_direction_input(nand_if->busy_gpio);

	return ret;
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

	ret = gpio_direction_input(nand_if->busy_gpio);
	if (ret)
		return ret;

	nand_if->busy_irq = gpio_to_irq(nand_if->busy_gpio);

	irq_flags = nand_if->busy_gpio_low_assert ?
			IRQF_TRIGGER_RISING :
				IRQF_TRIGGER_FALLING;

	ret = request_irq(nand_if->busy_irq, jz4780_nand_busy_isr,
				irq_flags, label_busy_gpio[nand_if->bank], nand_if);

	if (ret) {
		gpio_free(nand_if->busy_gpio);
		return ret;
	}

	init_completion(&nand_if->ready);

	if (!nand_if->ready_timout_ms)
		nand_if->ready_timout_ms = MAX_RB_TIMOUT_MS;

	return 0;
}

#ifdef CONFIG_DEBUG_FS

static int jz4780_nand_debugfs_show(struct seq_file *m, void *__unused)
{
	struct jz4780_nand *nand = (struct jz4780_nand *)m->private;
	nand_flash_if_t *nand_if;
	nand_flash_info_t *nand_info = nand->curr_nand_flash_info;
	int i, j;

	seq_printf(m, "Attached banks:\n");
	for (i = 0; i < nand->num_nand_flash_if; i++) {
		nand_if = nand->nand_flash_if_table[i];

		seq_printf(m, "bank%d\n", nand_if->bank);
	}

	if (nand->curr_nand_flash_if != -1) {
		nand_if = nand->nand_flash_if_table[nand->curr_nand_flash_if];
		seq_printf(m, "selected: bank%d\n",
				nand_if->bank);
	} else {
		seq_printf(m, "selected: none\n");
	}

	if (nand_info) {
		seq_printf(m, "\n");
		seq_printf(m, "Attached NAND flash:\n");

		seq_printf(m, "Chip name: %s\n", nand_info->name);
		if (nand->chip.onfi_version)
			seq_printf(m, "ONFI: v%d\n", nand->chip.onfi_version);
		else
			seq_printf(m, "ONFI: unsupported\n");
		seq_printf(m, "Chip devid: 0x%x\n", nand_info->nand_dev_id);
		seq_printf(m, "Chip size: %dMB\n", (int)(nand->chip.chipsize >> 20));
		seq_printf(m, "Erase size: %ubyte\n", nand->mtd.erasesize);
		seq_printf(m, "Write size: %dbyte\n", nand->mtd.writesize);
		seq_printf(m, "OOB size %dbyte\n", nand->mtd.oobsize);

		seq_printf(m, "\n");
		seq_printf(m, "Attached NAND flash ECC:\n");
		seq_printf(m, "ECC type: %s\n", nand->ecc_type == NAND_ECC_TYPE_HW ?
				"HW-BCH" : "SW-BCH");
		seq_printf(m, "ECC size: %dbyte\n", nand->chip.ecc.size);
		seq_printf(m, "ECC bits: %d\n", nand_info->ecc_step.ecc_bits);
		seq_printf(m, "ECC bytes: %d\n", nand->chip.ecc.bytes);
		seq_printf(m, "ECC steps: %d\n", nand->chip.ecc.steps);

		seq_printf(m, "\n");
		seq_printf(m, "ECC layout:\n");
		seq_printf(m, "ecclayout.eccbytes: %d\n", nand->ecclayout.eccbytes);
		seq_printf(m, "ecclayout.eccpos:\n");
		for (i = 0; i < nand->chip.ecc.steps; i++) {
			seq_printf(m, "ecc step: %d\n", i + 1);
			for (j = 0; j < nand->chip.ecc.bytes - 1; j++) {
				seq_printf(m, "%d, ",
						nand->ecclayout.eccpos[i * nand->chip.ecc.bytes + j]);

				if ((j + 1) % 10 == 0)
					seq_printf(m, "\n");
			}

			seq_printf(m, "%d\n",
					nand->ecclayout.eccpos[i * nand->chip.ecc.bytes + j]);
		}

		seq_printf(m, "ecclayout.oobavail: %d\n", nand->ecclayout.oobavail);
		seq_printf(m, "ecclayout.oobfree:\n");
		for (i = 0; i < ARRAY_SIZE(nand->ecclayout.oobfree); i++) {
			struct nand_oobfree *oobfree = &nand->ecclayout.oobfree[i];
			if (oobfree->length) {
				seq_printf(m ,"oobfree[%d]:\n", i);
				seq_printf(m, "length: %u\n", oobfree->length);
				seq_printf(m, "offset: %u\n", oobfree->offset);
			}
		}
	}

	return 0;
}

static int jz4780_nand_debugfs_open(struct inode *inode, struct file *file)
{
	return single_open(file, jz4780_nand_debugfs_show, inode->i_private);
}

static const struct file_operations jz4780_nand_debugfs_ops = {
	.open     = jz4780_nand_debugfs_open,
	.read     = seq_read,
	.llseek   = seq_lseek,
	.release  = single_release,
};

static struct dentry *jz4780_nand_debugfs_init(struct jz4780_nand *nand)
{
	return debugfs_create_file(dev_name(&nand->pdev->dev), S_IFREG | S_IRUGO,
			debugfs_root, nand, &jz4780_nand_debugfs_ops);
}

#endif

static nand_flash_info_t *
jz4780_nand_match_nand_chip_info(struct jz4780_nand *nand)
{
	struct mtd_info *mtd;
	struct nand_chip *chip;

	nand_flash_if_t *nand_if;
	struct jz4780_nand_platform_data *pdata;

	unsigned int nand_dev_id;
	int i;

	pdata = nand->pdata;
	chip = &nand->chip;
	mtd = &nand->mtd;

	nand_if = nand->nand_flash_if_table[0];

	if (!chip->onfi_version) {
		/*
		 * by traditional way
		 */
		chip->select_chip(mtd, 0);
		chip->cmdfunc(mtd, NAND_CMD_RESET, -1, -1);
		chip->cmdfunc(mtd, NAND_CMD_READID, 0x00, -1);
		nand_dev_id = chip->read_byte(mtd);
		nand_dev_id = chip->read_byte(mtd);
		chip->select_chip(mtd, -1);

		/*
		 * first match from board specific timings
		 */
		for (i = 0; i < pdata->num_nand_flash_info; i++) {
			if (nand_dev_id ==
					pdata->nand_flash_info_table[i].nand_dev_id &&
					nand_if->cs.bank_type ==
							pdata->nand_flash_info_table[i].type)
				return &pdata->nand_flash_info_table[i];
		}

		/*
		 * if got nothing form board specific timings
		 * we try to match form driver built-in timings
		 */
		for (i = 0; i < ARRAY_SIZE(builtin_nand_info_table); i++) {
			if (nand_dev_id ==
				  builtin_nand_info_table[i].nand_dev_id &&
				  nand_if->cs.bank_type ==
						  builtin_nand_info_table[i].type)
				return &builtin_nand_info_table[i];
		}
	} else {
		/*
		 * by ONFI way
		 */


		/*
		 * first match from board specific timings
		 */
		for (i = 0; i < pdata->num_nand_flash_info; i++) {
			if (!strncmp(chip->onfi_params.model,
					pdata->nand_flash_info_table[i].name, 20) &&
					nand_if->cs.bank_type ==
							pdata->nand_flash_info_table[i].type)
				return &pdata->nand_flash_info_table[i];
		}

		/*
		 * if got nothing form board specific timings
		 * we try to match form driver built-in timings
		 */
		for (i = 0; i < ARRAY_SIZE(builtin_nand_info_table); i++) {
			if (!strncmp(chip->onfi_params.model,
					builtin_nand_info_table[i].name, 20) &&
					nand_if->cs.bank_type ==
							builtin_nand_info_table[i].type)
				return &builtin_nand_info_table[i];
		}
	}


	if (!chip->onfi_version) {
		  dev_err(&nand->pdev->dev,
				  "Failed to find NAND info for devid: 0x%x\n",
						  nand_dev_id);
	} else {
		  dev_err(&nand->pdev->dev,
				  "Failed to find NAND info for model: %s\n",
						  chip->onfi_params.model);
	}

	return NULL;
}

#ifndef CONFIG_JZ4780_NAND_USE_RAW_DMA

static void jz4780_nand_dma_callback(void *comp)
{
	complete((struct completion *) comp);
}

#endif

static void jz4780_nand_cpu_read_buf(struct mtd_info *mtd,
		uint8_t *buf, int len)
{
	int i;
	struct nand_chip *chip = mtd->priv;

	for (i = 0; i < len; i++)
		buf[i] = readb(chip->IO_ADDR_R);
}

static void jz4780_nand_dma_read_buf(struct mtd_info *mtd,
		void *addr, int len)
{
#ifndef CONFIG_JZ4780_NAND_USE_RAW_DMA

	struct jz4780_nand *nand = mtd_to_jz4780_nand(mtd);
	struct dma_async_tx_descriptor *tx;

	unsigned int n;
	int ret;

	sg_init_one(&nand->dma_pipe.sg, addr, len);
	n = dma_map_sg(nand->dma_pipe.chan->device->dev,
			&nand->dma_pipe.sg, 1, DMA_FROM_DEVICE);
	if (n == 0) {
		dev_err(&nand->pdev->dev,
			"Failed to DMA map a %d byte buffer"
			" when DMA read NAND.\n", len);
		goto out_copy;
	}

	tx = dmaengine_prep_slave_sg(nand->dma_pipe.chan,
			&nand->dma_pipe.sg, n,
			DMA_FROM_DEVICE, DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!tx) {
		dev_err(&nand->pdev->dev, "Failed to prepare"
				" DMA read NAND.\n");
		goto out_copy_unmap;
	}

	tx->callback = jz4780_nand_dma_callback;
	tx->callback_param = &nand->dma_pipe.comp;
	dmaengine_submit(tx);

	/*
	 * setup and start DMA using dma_addr
	 */
	dma_async_issue_pending(nand->dma_pipe.chan);

	ret = wait_for_completion_timeout(&nand->dma_pipe.comp,
			msecs_to_jiffies(MAX_DMA_TRANSFER_TIMOUT_MS));

	if (!ret) {
		WARN(1, "Timeout when DMA read NAND for %dms.\n",
				MAX_DMA_TRANSFER_TIMOUT_MS);
		goto out_copy_unmap;
	}


	dma_unmap_sg(nand->dma_pipe.chan->device->dev,
			&nand->dma_pipe.sg, 1, DMA_FROM_DEVICE);

	return;

out_copy_unmap:
	dma_unmap_sg(nand->dma_pipe.chan->device->dev,
			&nand->dma_pipe.sg, 1, DMA_FROM_DEVICE);
out_copy:
	jz4780_nand_cpu_read_buf(mtd, (uint8_t *)addr, len);

#else

	struct jz4780_nand *nand = mtd_to_jz4780_nand(mtd);
	struct jzdma_channel *dmac;
	void __iomem *dmac_regs;
	unsigned long timeo;
	unsigned int n;

	sg_init_one(&nand->dma_pipe.sg, addr, len);
	n = dma_map_sg(nand->dma_pipe.chan->device->dev,
			&nand->dma_pipe.sg, 1, DMA_FROM_DEVICE);
	if (n == 0) {
		dev_err(&nand->pdev->dev,
			"Failed to DMA map a %d byte buffer"
			" when DMA read NAND.\n", len);
		goto out_copy;
	}

	/*
	 * step1. configure DMA channel
	 */
	dmac = to_jzdma_chan(nand->dma_pipe.chan);
	dmac_regs = dmac->iomem;

	/* no descriptor*/
	writel(BIT(31), dmac_regs + CH_DCS);

	/* src_addr */
	writel(dmac->config->src_addr, dmac_regs + CH_DSA);

	/* dst_addr */
	writel(sg_dma_address(&nand->dma_pipe.sg), dmac_regs + CH_DTA);

	/* channel cmd */
	writel(DCM_DAI | dmac->rx_dcm_def, dmac_regs + CH_DCM);

	/* request type */
	writel(dmac->type, dmac_regs + CH_DRT);

	/* transfer count */
	writel(sg_dma_len(&nand->dma_pipe.sg), dmac_regs + CH_DTC);

	wmb();

	/*
	 * step2. start no descriptor DMA transfer
	 */
	writel(BIT(0), dmac_regs + CH_DCS);

	/*
	 * step3. wait for transfer done
	 */
	timeo = jiffies + msecs_to_jiffies(MAX_DMA_TRANSFER_TIMOUT_MS);
	do {
		if (!readl(dmac_regs + CH_DTC))
			break;

		cond_resched();
	} while (time_before(jiffies, timeo));

	if (readl(dmac_regs + CH_DTC)) {
		WARN(1, "Timeout when DMA read NAND for %dms.\n",
				MAX_DMA_TRANSFER_TIMOUT_MS);
		goto out_copy_unmap;
	}

	/*
	 * step4. err check and stop
	 */
	dmac->dcs_saved = readl(dmac->iomem + CH_DCS);

	if (dmac->dcs_saved & DCS_AR) {
		dev_err(&nand->pdev->dev,
				"DMA addr err: DCS%d = %lx\n",
				dmac->id,dmac->dcs_saved);
	} else if (dmac->dcs_saved & DCS_HLT) {
		dev_err(&nand->pdev->dev,
				"DMA halt: DCS%d = %lx\n",
				dmac->id, dmac->dcs_saved);
	}

	writel(0, dmac->iomem + CH_DCS);

	dma_unmap_sg(nand->dma_pipe.chan->device->dev,
			&nand->dma_pipe.sg, 1, DMA_FROM_DEVICE);

	return;

out_copy_unmap:
	dma_unmap_sg(nand->dma_pipe.chan->device->dev,
			&nand->dma_pipe.sg, 1, DMA_FROM_DEVICE);
out_copy:
	jz4780_nand_cpu_read_buf(mtd, (uint8_t *)addr, len);

#endif
}

static void jz4780_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	if (len <= mtd->oobsize)
		jz4780_nand_cpu_read_buf(mtd, buf, len);
	else
		jz4780_nand_dma_read_buf(mtd, buf, len);
}

static void jz4780_nand_cpu_write_buf(struct mtd_info *mtd,
		const uint8_t *buf, int len)
{
	int i;
	struct nand_chip *chip = mtd->priv;

	for (i = 0; i < len; i++)
		writeb(buf[i], chip->IO_ADDR_W);
}

static void jz4780_nand_dma_write_buf(struct mtd_info *mtd,
		const void *addr, int len)
{
#ifndef CONFIG_JZ4780_NAND_USE_RAW_DMA

	struct jz4780_nand *nand = mtd_to_jz4780_nand(mtd);
	struct dma_async_tx_descriptor *tx;

	unsigned int n;
	int ret;

	sg_init_one(&nand->dma_pipe.sg, addr, len);
	n = dma_map_sg(nand->dma_pipe.chan->device->dev,
			&nand->dma_pipe.sg, 1, DMA_TO_DEVICE);
	if (n == 0) {
		dev_err(&nand->pdev->dev,
			"Failed to DMA map a %d byte buffer"
			" when DMA write NAND.\n", len);
		goto out_copy;
	}

	tx = dmaengine_prep_slave_sg(nand->dma_pipe.chan,
			&nand->dma_pipe.sg, n,
			DMA_TO_DEVICE, DMA_PREP_INTERRUPT | DMA_CTRL_ACK);
	if (!tx) {
		dev_err(&nand->pdev->dev, "Failed to prepare"
				" DMA write NAND.\n");
		goto out_copy_unmap;
	}

	tx->callback = jz4780_nand_dma_callback;
	tx->callback_param = &nand->dma_pipe.comp;
	dmaengine_submit(tx);

	/*
	 * setup and start DMA using dma_addr
	 */
	dma_async_issue_pending(nand->dma_pipe.chan);

	ret = wait_for_completion_timeout(&nand->dma_pipe.comp,
			msecs_to_jiffies(MAX_DMA_TRANSFER_TIMOUT_MS));

	if (!ret) {
		WARN(!ret, "Timeout when DMA write NAND for %dms.\n",
				MAX_DMA_TRANSFER_TIMOUT_MS);
		goto out_copy_unmap;
	}

	dma_unmap_sg(nand->dma_pipe.chan->device->dev,
			&nand->dma_pipe.sg, 1, DMA_TO_DEVICE);

	return;

out_copy_unmap:
	dma_unmap_sg(nand->dma_pipe.chan->device->dev,
			&nand->dma_pipe.sg, 1, DMA_TO_DEVICE);
out_copy:
	jz4780_nand_cpu_write_buf(mtd, (uint8_t *)addr, len);

#else

	struct jz4780_nand *nand = mtd_to_jz4780_nand(mtd);
	struct jzdma_channel *dmac;
	void __iomem *dmac_regs;
	unsigned long timeo;
	unsigned int n;

	sg_init_one(&nand->dma_pipe.sg, addr, len);
	n = dma_map_sg(nand->dma_pipe.chan->device->dev,
			&nand->dma_pipe.sg, 1, DMA_TO_DEVICE);
	if (n == 0) {
		dev_err(&nand->pdev->dev,
			"Failed to DMA map a %d byte buffer"
			" when DMA read NAND.\n", len);
		goto out_copy;
	}

	/*
	 * step1. configure DMA channel
	 */
	dmac = to_jzdma_chan(nand->dma_pipe.chan);
	dmac_regs = dmac->iomem;

	/* no descriptor*/
	writel(BIT(31), dmac_regs + CH_DCS);

	/* src_addr */
	writel(sg_dma_address(&nand->dma_pipe.sg), dmac_regs + CH_DSA);

	/* dst_addr */
	writel(dmac->config->dst_addr, dmac_regs + CH_DTA);

	/* channel cmd */
	writel(DCM_SAI | dmac->tx_dcm_def, dmac_regs + CH_DCM);

	/* request type */
	writel(dmac->type, dmac_regs + CH_DRT);

	/* transfer count */
	writel(sg_dma_len(&nand->dma_pipe.sg), dmac_regs + CH_DTC);

	wmb();

	/*
	 * step2. start DMA transfer
	 */
	writel(BIT(0), dmac_regs + CH_DCS);

	/*
	 * step3. wait for transfer done
	 */
	timeo = jiffies + msecs_to_jiffies(MAX_DMA_TRANSFER_TIMOUT_MS);
	do {
		if (!readl(dmac_regs + CH_DTC))
			break;

		cond_resched();
	} while (time_before(jiffies, timeo));

	if (readl(dmac_regs + CH_DTC)) {
		WARN(1, "Timeout when DMA read NAND for %dms.\n",
				MAX_DMA_TRANSFER_TIMOUT_MS);
		goto out_copy_unmap;
	}

	/*
	 * step4. err check and stop
	 */
	dmac->dcs_saved = readl(dmac->iomem + CH_DCS);

	if (dmac->dcs_saved & DCS_AR) {
		dev_err(&nand->pdev->dev,
				"DMA addr err: DCS%d = %lx\n",
				dmac->id,dmac->dcs_saved);
	} else if (dmac->dcs_saved & DCS_HLT) {
		dev_err(&nand->pdev->dev,
				"DMA halt: DCS%d = %lx\n",
				dmac->id, dmac->dcs_saved);
	}

	writel(0, dmac->iomem + CH_DCS);

	dma_unmap_sg(nand->dma_pipe.chan->device->dev,
			&nand->dma_pipe.sg, 1, DMA_TO_DEVICE);

	return;

out_copy_unmap:
	dma_unmap_sg(nand->dma_pipe.chan->device->dev,
			&nand->dma_pipe.sg, 1, DMA_TO_DEVICE);
out_copy:
	jz4780_nand_cpu_read_buf(mtd, (uint8_t *)addr, len);

#endif
}

static void jz4780_nand_write_buf(struct mtd_info *mtd,
		const uint8_t *buf, int len)
{
	if (len <= mtd->oobsize)
		jz4780_nand_cpu_write_buf(mtd, buf, len);
	else
		jz4780_nand_dma_write_buf(mtd, buf, len);
}

static int jz4780_nand_probe(struct platform_device *pdev)
{
	int ret = 0;
	int bank = 0;
	int i = 0, j = 0, k = 0, m = 0;
	int eccpos_start;

	struct nand_chip *chip;
	struct mtd_info *mtd;

	struct jz4780_nand *nand;
	struct jz4780_nand_platform_data *pdata;

	nand_flash_if_t *nand_if;

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

	nand->pdev = pdev;
	nand->pdata = pdata;
	platform_set_drvdata(pdev, nand);

	nand->num_nand_flash_if = pdata->num_nand_flash_if;
	nand->xfer_type = pdata->xfer_type;
	nand->ecc_type = pdata->ecc_type;

	/*
	 * request GPEMC banks
	 */
	for (i = 0; i < nand->num_nand_flash_if; i++, j = i) {
		nand_if = &pdata->nand_flash_if_table[i];
		nand->nand_flash_if_table[i] = nand_if;
		bank = nand_if->bank;

		ret = gpemc_request_cs(&pdev->dev, &nand_if->cs, bank);
		if (ret) {
			dev_err(&pdev->dev,
				"Failed to request GPEMC bank%d.\n", bank);
			goto err_release_cs;
		}
	}

	/*
	 * request busy GPIO interrupt
	 */
	switch (nand->xfer_type) {
	case NAND_XFER_CPU_IRQ:
	case NAND_XFER_DMA_IRQ:
		for (i = 0; i < nand->num_nand_flash_if; i++, k = i) {
			nand_if = &pdata->nand_flash_if_table[i];
			ret = request_busy_irq(nand_if);
			if (ret) {
				dev_err(&pdev->dev,
					"Failed to request busy gpio irq for bank%d\n", bank);
				goto err_free_busy_irq;
			}
		}

		break;

	case NAND_XFER_CPU_POLL:
	case NAND_XFER_DMA_POLL:
		for (i = 0; i < nand->num_nand_flash_if; i++, k = i) {
			nand_if = &pdata->nand_flash_if_table[i];
			ret = request_busy_poll(nand_if);
			if (ret) {
				dev_err(&pdev->dev,
					"Failed to request busy gpio irq for bank%d\n", bank);
				goto err_free_busy_irq;
			}
		}

		nand->busy_poll = 1;

		break;

	default:
		WARN(1, "Unsupport transfer type.\n");
		BUG();

		break;
	}

	/*
	 * request WP GPIO
	 */
	for (i = 0; i < nand->num_nand_flash_if; i++, m = i) {
		nand_if = &pdata->nand_flash_if_table[i];
		if (nand_if->wp_gpio < 0)
			continue;

		if (!gpio_is_valid(nand_if->wp_gpio)) {
			dev_err(&pdev->dev,
				"Invalid wp GPIO:%d\n", nand_if->wp_gpio);

			ret = -EINVAL;
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
	 * NAND flash devices support list override
	 */
	nand->nand_flash_table = pdata->nand_flash_table ?
		pdata->nand_flash_table : builtin_nand_flash_table;
	nand->num_nand_flash = pdata->nand_flash_table ?
		pdata->num_nand_flash :
			ARRAY_SIZE(builtin_nand_flash_table);

	/*
	 * attach to MTD subsystem
	 */
	chip              = &nand->chip;
	chip->chip_delay  = MAX_RB_DELAY_US;
	chip->cmdfunc     = jz4780_nand_command;
	chip->dev_ready   = jz4780_nand_dev_is_ready;
	chip->select_chip = jz4780_nand_select_chip;
	chip->cmd_ctrl    = jz4780_nand_cmd_ctrl;

	switch (nand->xfer_type) {
	case NAND_XFER_DMA_IRQ:
	case NAND_XFER_DMA_POLL:
		/*
		 * DMA transfer
		 */
		ret = jz4780_nand_request_dma(nand);
		if (ret) {
			dev_err(&pdev->dev, "Failed to request DMA channel.\n");
			goto err_free_wp_gpio;
		}

		chip->read_buf  = jz4780_nand_read_buf;
		chip->write_buf = jz4780_nand_write_buf;

		nand_if = nand->nand_flash_if_table[0];

		nand->dma_pipe.cfg.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		nand->dma_pipe.cfg.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		nand->dma_pipe.cfg.dst_maxburst = 16;
		nand->dma_pipe.cfg.src_maxburst = 16;
		nand->dma_pipe.cfg.src_addr =
				(dma_addr_t)CPHYSADDR(nand_if->cs.io_nand_dat);
		nand->dma_pipe.cfg.dst_addr =
				(dma_addr_t)CPHYSADDR(nand_if->cs.io_nand_dat);

		dmaengine_slave_config(nand->dma_pipe.chan, &nand->dma_pipe.cfg);

		nand->use_dma = 1;

		break;

	case NAND_XFER_CPU_IRQ:
	case NAND_XFER_CPU_POLL:
		/*
		 * CPU transfer
		 */
		chip->read_buf  = jz4780_nand_cpu_read_buf;
		chip->write_buf = jz4780_nand_cpu_write_buf;

		break;

	default:
		WARN(1, "Unsupport transfer type.\n");
		BUG();

		break;
	}

	mtd              = &nand->mtd;
	mtd->priv        = chip;
	mtd->name        = dev_name(&pdev->dev);
	mtd->owner       = THIS_MODULE;

	/*
	 * Detect NAND flash chips
	 */

	/* step1. relax bank timings to scan */
	for (bank = 0; bank < nand->num_nand_flash_if; bank++) {
		nand_if = nand->nand_flash_if_table[bank];

		gpemc_relax_bank_timing(&nand_if->cs);
	}

	if (nand_scan_ident(mtd, nand->num_nand_flash_if,
			nand->nand_flash_table)) {

		ret = -ENXIO;
		dev_err(&pdev->dev, "Failed to detect NAND flash.\n");
		goto err_dma_release_channel;
	}

	/*
	 * post configure bank timing by detected NAND device
	 */

	/* step1. replace NAND command function with large page version */
	if (mtd->writesize > 512)
		chip->cmdfunc = jz4780_nand_command_lp;

	/* step2. match NAND chip information */
	nand->curr_nand_flash_info = jz4780_nand_match_nand_chip_info(nand);
	if (!nand->curr_nand_flash_info) {
		ret = -ENODEV;
		goto err_dma_release_channel;
	}

	/* step3. configure bank timings */
	switch (nand->curr_nand_flash_info->type) {
	case BANK_TYPE_NAND:
		for (bank = 0; bank < nand->num_nand_flash_if; bank++) {
			nand_if = nand->nand_flash_if_table[bank];

			gpemc_fill_timing_from_nand(&nand_if->cs,
					&nand->curr_nand_flash_info->nand_timing.common_nand_timing);

			ret = gpemc_config_bank_timing(&nand_if->cs);
			if (ret) {
				dev_err(&pdev->dev,
					"Failed to configure timings for bank%d\n"
						, nand_if->bank);
				goto err_dma_release_channel;
			}
		}

		break;

	case BANK_TYPE_TOGGLE:
		for (bank = 0; bank < nand->num_nand_flash_if; bank++) {
			nand_if = nand->nand_flash_if_table[bank];

			gpemc_fill_timing_from_toggle(&nand_if->cs,
					&nand->curr_nand_flash_info->nand_timing.toggle_nand_timing);

			ret = gpemc_config_bank_timing(&nand_if->cs);
			if (ret) {
				dev_err(&pdev->dev,
					"Failed to configure timings for bank%d\n"
						, nand_if->bank);
				goto err_dma_release_channel;
			}
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
	case NAND_ECC_TYPE_SW:
		chip->ecc.mode  = NAND_ECC_SOFT_BCH;
		chip->ecc.size  = nand->curr_nand_flash_info->ecc_step.data_size;
		chip->ecc.bytes =
				(13 * nand->curr_nand_flash_info->ecc_step.ecc_bits + 7) / 8;

		break;

	case NAND_ECC_TYPE_HW:
		nand->bch_req.dev       = &nand->pdev->dev;
		nand->bch_req.complete  = jz4780_nand_bch_req_complete;
		nand->bch_req.ecc_level =
				nand->curr_nand_flash_info->ecc_step.ecc_bits;
		nand->bch_req.blksz     =
				nand->curr_nand_flash_info->ecc_step.data_size;

		nand->bch_req.errrept_data = kzalloc(MAX_ERRREPT_DATA_SIZE,
				GFP_KERNEL);
		if (!nand->bch_req.errrept_data) {
			dev_err(&pdev->dev,
				"Failed to allocate ECC errrept_data buffer\n");
			ret = -ENOMEM;
			goto err_dma_release_channel;
		}

		init_completion(&nand->bch_req_done);

		chip->ecc.mode      = NAND_ECC_HW;
		chip->ecc.calculate = jz4780_nand_ecc_calculate_bch;
		chip->ecc.correct   = jz4780_nand_ecc_correct_bch;
		chip->ecc.hwctl     = jz4780_nand_ecc_hwctl;
		chip->ecc.size  = nand->curr_nand_flash_info->ecc_step.data_size;
		chip->ecc.bytes = bch_ecc_bits_to_bytes(
				nand->curr_nand_flash_info->ecc_step.ecc_bits);

		chip->ecc.strength = nand->bch_req.ecc_level;

		break;

	default :
		BUG();

		break;
	}

	/* step2. generate ECC layout */

	/*
	 * eccbytes = eccsteps * eccbytes_prestep;
	 */
	nand->ecclayout.eccbytes =
		mtd->writesize / chip->ecc.size * chip->ecc.bytes;

	/*
	 * eccpos is right aligned
	 * start position = oobsize - eccbytes
	 */
	eccpos_start = mtd->oobsize - nand->ecclayout.eccbytes;
	for (bank = 0; bank < nand->ecclayout.eccbytes; bank++)
		nand->ecclayout.eccpos[bank] = eccpos_start + bank;

	nand->ecclayout.oobfree->offset = chip->badblockpos + 2;
	nand->ecclayout.oobfree->length =
		mtd->oobsize - (nand->ecclayout.eccbytes + chip->badblockpos + 2);

	chip->ecc.layout = &nand->ecclayout;

	/*
	 * second phase NAND scan
	 */
	if (nand_scan_tail(mtd)) {
		ret = -ENXIO;
		goto err_free_errrpt_data;
	}

#ifdef CONFIG_DEBUG_FS

	nand->debugfs_entry = jz4780_nand_debugfs_init(nand);
	if (IS_ERR(nand->debugfs_entry)) {
		dev_err(&pdev->dev, "Failed to register debugfs entry.\n");

		ret = PTR_ERR(nand->debugfs_entry);
		goto err_free_errrpt_data;
	}

#endif

	/*
	 * relocate hot functions to TCSM
	 */
	if (pdata->relocate_hot_functions) {
		ret = jz4780_nand_relocate_hot_to_tcsm(nand);
		if (ret) {
			dev_err(&pdev->dev, "Failed to relocate hot functions.\n");
			goto err_debugfs_remove;
		}
	}

	/*
	 * MTD register
	 */
	ret = mtd_device_parse_register(mtd, NULL, NULL,
			pdata->part_table, pdata->num_part);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add MTD device\n");
		goto err_unlocate_hot;
	}

	dev_info(&pdev->dev,
		"Successfully registered JZ4780 SoC NAND controller driver.\n");

	return 0;

err_unlocate_hot:
	if (pdata->relocate_hot_functions)
		jz4780_nand_unlocate_hot_from_tcsm(nand);

err_debugfs_remove:
	debugfs_remove_recursive(nand->debugfs_entry);

err_free_errrpt_data:
	if (pdata->ecc_type == NAND_ECC_TYPE_HW)
		kfree(nand->bch_req.errrept_data);

err_dma_release_channel:
	if (nand->xfer_type == NAND_XFER_DMA_IRQ ||
			nand->xfer_type == NAND_XFER_DMA_POLL)
		dma_release_channel(nand->dma_pipe.chan);


err_free_wp_gpio:
	for (bank = 0; bank < m; bank++) {
		nand_if = &pdata->nand_flash_if_table[bank];

		gpio_free(nand_if->wp_gpio);
	}

err_free_busy_irq:
	for (bank = 0; bank < k; bank++) {
		nand_if = &pdata->nand_flash_if_table[bank];

		if (pdata->xfer_type == NAND_XFER_CPU_IRQ ||
				pdata->xfer_type ==NAND_XFER_DMA_IRQ)
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

static int jz4780_nand_remove(struct platform_device *pdev)
{
	struct jz4780_nand *nand;
	nand_flash_if_t *nand_if;
	int i;

	nand = platform_get_drvdata(pdev);

	debugfs_remove_recursive(nand->debugfs_entry);

	nand_release(&nand->mtd);

	/* free NAND flash interface resource */
	for (i = 0; i < nand->num_nand_flash_if; i++) {
		nand_if = nand->nand_flash_if_table[i];

		if (nand_if->busy_gpio != -1) {
			if (nand->xfer_type == NAND_XFER_CPU_IRQ ||
				nand->xfer_type == NAND_XFER_DMA_IRQ)
				free_irq(nand_if->busy_irq, nand_if);

			gpio_free(nand_if->busy_gpio);
		}

		if (nand_if->wp_gpio != -1) {
			jz4780_nand_enable_wp(nand_if, 1);
			gpio_free(nand_if->wp_gpio);
		}

		gpemc_release_cs(&nand_if->cs);
	}

	if (nand->bch_req.errrept_data)
		kfree(nand->bch_req.errrept_data);

	if (nand->xfer_type == NAND_XFER_DMA_IRQ ||
			nand->xfer_type == NAND_XFER_DMA_POLL)
		dma_release_channel(nand->dma_pipe.chan);

	/*
	 * TODO
	 * "unlocate..." implements nothing
	 * so you may get in trouble when
	 * do "insmod jz4780_nand.ko"
	 * becuase of failed to allocate TCSM
	 */
	if (nand->pdata->relocate_hot_functions)
		jz4780_nand_unlocate_hot_from_tcsm(nand);

	kfree(nand);

	return 0;
}

static struct platform_driver jz4780_nand_driver = {
	.probe = jz4780_nand_probe,
	.remove = jz4780_nand_remove,
	.driver = {
		.name = DRVNAME,
		.owner = THIS_MODULE,
	},
};

static int __init jz4780_nand_init(void)
{
#ifdef CONFIG_DEBUG_FS

	debugfs_root = debugfs_create_dir(DRVNAME, NULL);
	if (IS_ERR(debugfs_root))
		return PTR_ERR(debugfs_root);

#endif

	return platform_driver_register(&jz4780_nand_driver);
}
module_init(jz4780_nand_init);

static void __exit jz4780_nand_exit(void)
{
	platform_driver_unregister(&jz4780_nand_driver);

#ifdef CONFIG_DEBUG_FS

	debugfs_remove_recursive(debugfs_root);

#endif
}
module_exit(jz4780_nand_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Fighter Sun <wanmyqawdr@126.com>");
MODULE_DESCRIPTION("NAND controller driver for JZ4780 SoC");
MODULE_ALIAS("platform:"DRVNAME);
