/*
 *  Copyright (C) 2013 Fighter Sun <wanmyqawdr@126.com>
 *  JZ4780 SoC NAND controller driver
 *
 *
 *  TODO:
 *	DMA drove data path
 *	support toggle NAND
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

#define MAX_NUM_NAND_IF    7
#define MAX_RB_DELAY_US    50

#define MAX_RB_TIMOUT_MS   20

#define MAX_RESET_DELAY_MS 20

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
#define NAND_FLASH_K9GBG08U0A_NANE  "K9GBG08U0A"
#define NAND_FLASH_K9GBG08U0A_ID    0xd7


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
	 * !!!Caution
	 * please do not use busy pin IRQ over "K9GBG08U0A"
	 * the chip is running under very rigorous timings
	 */
	{
		NAND_FLASH_K9GBG08U0A_NANE, NAND_FLASH_K9GBG08U0A_ID,
		0, 4096, 0, LP_OPTIONS
	},
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
		 * Datasheet of K9GBG08U0A, V1.3, P5, S1.2
		 * ECC : 24bit/1KB
		 *
		 * we assign 28bit/1KB here, the overs are usable when
		 * bitflips occur in OOB area
		 */
		COMMON_NAND_CHIP_INFO(
			NAND_FLASH_K9GBG08U0A_NANE, NAND_FLASH_K9GBG08U0A_ID,
			1024, 28,
			12, 5, 12, 5, 20, 5, 12, 5, 12, 10,
			25, 25, 300, 100, 100, 300, 12, 20, 300, 100,
			100, 200 * 1000, 1 * 1000, 200 * 1000,
			5 * 1000 * 1000, BUS_WIDTH_8)
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

	nand_flash_info_t *nand_flash_info_table;
	int num_nand_flash_info;
	nand_flash_info_t *curr_nand_flash_info;

	nand_xfer_type_t xfer_type;

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

	if (nand->xfer_type == NAND_XFER_CPU_IRQ ||
		nand->xfer_type == NAND_XFER_DMA_IRQ) {

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

		/*
		 * Apply this short delay always to ensure that we do wait tCS in
		 * any case on any machine.
		 */
		ndelay(100);
	}

	nand->curr_nand_flash_if = chip;
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

	nand_xfer_type_t old_xfer_type;
	struct jz4780_nand *nand;
	nand_flash_if_t *nand_if;
	nand_flash_info_t *nand_info;

	nand = mtd_to_jz4780_nand(mtd);
	old_xfer_type = nand->xfer_type;

	nand_if = nand->nand_flash_if_table[nand->curr_nand_flash_if];
	nand_if->curr_command = command;

	nand_info = nand->curr_nand_flash_info;

	/*
	 * R/B# polling policy
	 */
	switch (command) {
	case NAND_CMD_READID:
	case NAND_CMD_RESET:
		switch (nand->xfer_type) {
		case NAND_XFER_DMA_IRQ:
			nand->xfer_type = NAND_XFER_DMA_POLL;
			break;

		case NAND_XFER_CPU_IRQ:
			nand->xfer_type = NAND_XFER_CPU_POLL;
			break;

		default:
			break;
		}

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
		/* Adjust columns for 16 bit buswidth */
		if (chip->options & NAND_BUSWIDTH_16)
			column >>= 1;
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

	nand->xfer_type = old_xfer_type;
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
			/* Adjust columns for 16 bit buswidth */
			if (chip->options & NAND_BUSWIDTH_16)
				column >>= 1;
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

static int jz4780_nand_probe(struct platform_device *pdev)
{
	int ret = 0;
	int bank = 0;
	int i = 0, j = 0, k = 0, m = 0;
	int nand_dev_id = 0;
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
	platform_set_drvdata(pdev, nand);

#ifdef CONFIG_DEBUG_FS

	nand->debugfs_entry = jz4780_nand_debugfs_init(nand);
	if (IS_ERR(nand->debugfs_entry)) {
		dev_err(&pdev->dev, "Failed to register debugfs entry.\n");

		ret = PTR_ERR(nand->debugfs_entry);
		goto err_free_nand;
	}

#endif

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

		break;

	default:
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
	chip->chip_delay  = MAX_RB_DELAY_US;
	chip->cmdfunc     = jz4780_nand_command;
	chip->dev_ready   = jz4780_nand_dev_is_ready;
	chip->select_chip = jz4780_nand_select_chip;
	chip->cmd_ctrl    = jz4780_nand_cmd_ctrl;

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
		goto err_free_wp_gpio;
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
	chip->select_chip(mtd, -1);

	/* step2. replace NAND command function with large page version */
	if (mtd->writesize > 512)
		chip->cmdfunc = jz4780_nand_command_lp;

	/* step3. find NAND flash info */
	nand_if = nand->nand_flash_if_table[0];
	for (bank = 0; bank < nand->num_nand_flash_info; bank++)
		if (nand_dev_id ==
				nand->nand_flash_info_table[bank].nand_dev_id &&
				nand_if->cs.bank_type ==
						nand->nand_flash_info_table[bank].type)
			break;

	if (bank == nand->num_nand_flash_info) {
		dev_err(&pdev->dev,
			"Failed to find NAND info for devid:%d\n",
				nand_dev_id);
		goto err_free_wp_gpio;
	}

	/* step4. configure bank timing */
	nand->curr_nand_flash_info = &nand->nand_flash_info_table[bank];
	switch (nand->curr_nand_flash_info->type) {
	case BANK_TYPE_NAND:
		for (bank = 0; bank < nand->num_nand_flash_info; bank++) {
			nand_if = nand->nand_flash_if_table[bank];

			gpemc_fill_timing_from_nand(&nand_if->cs,
					&nand->curr_nand_flash_info->nand_timing.common_nand_timing);

			ret = gpemc_config_bank_timing(&nand_if->cs);
			if (ret) {
				dev_err(&pdev->dev,
					"Failed to configure timings for bank%d\n"
						, nand_if->bank);
				goto err_free_wp_gpio;
			}
		}

		break;

	case BANK_TYPE_TOGGLE:
		for (bank = 0; bank < nand->num_nand_flash_info; bank++) {
			nand_if = nand->nand_flash_if_table[bank];

			gpemc_fill_timing_from_toggle(&nand_if->cs,
					&nand->curr_nand_flash_info->nand_timing.toggle_nand_timing);

			ret = gpemc_config_bank_timing(&nand_if->cs);
			if (ret) {
				dev_err(&pdev->dev,
					"Failed to configure timings for bank%d\n"
						, nand_if->bank);
				goto err_free_wp_gpio;
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

			kfree(nand->bch_req.ecc_data);
			goto err_free_wp_gpio;
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
		goto err_free_wp_gpio;
	}

	/*
	 * MTD register
	 */
	ret = mtd_device_parse_register(mtd, NULL, NULL,
			pdata->part_table, pdata->num_part);
	if (ret) {
		dev_err(&pdev->dev, "Failed to add MTD device\n");
		goto err_free_wp_gpio;
	}

	dev_info(&pdev->dev,
		"Successfully registered JZ4780 SoC NAND controller driver.\n");

	return 0;

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

	debugfs_remove_recursive(nand->debugfs_entry);

err_free_nand:
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

	/* free device object */
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
