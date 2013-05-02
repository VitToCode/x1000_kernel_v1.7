/*
 *  Copyright (C) 2013 Fighter Sun <wanmyqawdr@126.com>
 *  JZ4780 SoC NAND controller driver
 *
 *
 *  TODO:
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
#include <linux/kthread.h>
#include <linux/freezer.h>

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/nand_bch.h>
#include <linux/bch.h>

#include <linux/gpio.h>

#include <asm/r4kcache.h>
#include <asm/cacheflush.h>
#include <asm/cache.h>
/*
 * oh yes, we are architecture special
 * here we use very low level skills to
 * relocate static linked codes to another
 */
#include <asm/inst.h>

#include <soc/gpemc.h>
#include <soc/bch.h>
#include <mach/jzdma.h>
#include <mach/jz4780_nand.h>

#define DRVNAME "jz4780-nand"

#define MAX_RB_DELAY_US             50
#define MAX_RB_TIMOUT_MS            50
#define MAX_RESET_DELAY_MS          50

#define MAX_DMA_TRANSFER_TIMOUT_MS  1000
#define DMA_BUF_SIZE                PAGE_SIZE * 4

#define MAX_CNT_SWECC_REQ           64
#define MAX_SWECC_REQ_POLL_TIME_MS  10


/*
 * for relocation
 */
#undef mdelay
#undef udelay
#undef ndelay

#define mdelay(object, n) (\
	(__builtin_constant_p(n) && (n)<=MAX_UDELAY_MS) ? (object)->udelay((n)*1000) : \
	({unsigned long __ms=(n); while (__ms--) (object)->udelay(1000);}))

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
		 */
		COMMON_NAND_CHIP_INFO(
			NAND_FLASH_K9GBG08U0A_NANE, NAND_FLASH_K9GBG08U0A_ID,
			1024, 24, 0,
			12, 5, 12, 5, 20, 5, 12, 5, 12, 10,
			25, 25, 300, 100, 100, 300, 12, 20, 300, 100,
			100, 200 * 1000, 1 * 1000, 200 * 1000,
			5 * 1000 * 1000, BUS_WIDTH_8)
	},

	{
		/*
		 * Datasheet of MT29F32G08CBACA(WP), Rev-E, P109, Table-17
		 * ECC : 24bit/1080bytes
		 */
		COMMON_NAND_CHIP_INFO(
			NAND_FLASH_MT29F32G08CBACAWP, NAND_FLASH_MT29F32G08CBACAWP_ID,
			1024, 24, 0,
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

struct jz4780_nand_dma {
	struct dma_chan *chan;
	struct dma_slave_config cfg;

	enum jzdma_type type;
};

struct jz4780_nand_dma_buf {
	void *buf;
	dma_addr_t phy_addr;
	int size;
};

struct jz4780_nand_swecc_req {
	uint8_t *buf;
	uint8_t *ecc_calc;
	uint8_t *ecc_code;

	int correct_state;
};

struct jz4780_nand_swecc_pipe {
	struct jz4780_nand_swecc_req req[MAX_CNT_SWECC_REQ];
	atomic_t curr_index;
	int op_type_calculate;

	struct nand_ecc_ctrl *ecc;
	struct nand_bch_control *nbc;

	int (*calculate)(struct nand_bch_control *nbc,
			struct nand_ecc_ctrl *ecc, const uint8_t *dat,
			uint8_t *ecc_code);
	int (*correct)(struct nand_bch_control *nbc,
			struct nand_ecc_ctrl *ecc,
			uint8_t *dat, uint8_t *read_ecc,
			uint8_t *calc_ecc);

	struct task_struct *task;
	wait_queue_head_t wait;
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

	struct jz4780_nand_dma dma_pipe_nand;
	struct jz4780_nand_dma_buf dma_buf;

	nand_flash_info_t *curr_nand_flash_info;

	nand_xfer_type_t xfer_type;
	int use_dma;
	int busy_poll;

	struct jz4780_nand_swecc_pipe swecc_current;
	struct jz4780_nand_swecc_pipe swecc_pipe;

	struct jz4780_nand_platform_data *pdata;
	struct platform_device *pdev;

#ifdef CONFIG_DEBUG_FS

	struct dentry *debugfs_entry;

#endif

	/*
	 * for relocation
	 */
	void (*gpemc_enable_nand_flash)(gpemc_bank_t *bank, bool enable);
	void (*nand_wait_ready)(struct mtd_info *mtd);
	int (*gpio_get_value_cansleep)(unsigned gpio);
	unsigned long (*wait_for_completion_timeout)(struct completion *x,
			unsigned long timeout);
	unsigned long (*msecs_to_jiffies)(const unsigned int m);
	int (*printk)(const char *fmt, ...);
	void (*ndelay)(unsigned int ns);
	void (*udelay)(unsigned int us);
};

static inline struct jz4780_nand *mtd_to_jz4780_nand(struct mtd_info *mtd)
{
	return container_of(mtd, struct jz4780_nand, mtd);
}

static inline int jz4780_nand_chip_is_ready(struct jz4780_nand *nand,
		nand_flash_if_t *nand_if)
{
	int low_assert;
	int gpio;

	low_assert = nand_if->busy_gpio_low_assert;
	gpio = nand_if->busy_gpio;

	return !(nand->gpio_get_value_cansleep(gpio) ^ low_assert);
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

		ret = nand->wait_for_completion_timeout(&nand_if->ready,
				nand->msecs_to_jiffies(nand_if->ready_timout_ms));

		if (!ret)
			nand->printk("%s: Timeout when"
					" wait NAND chip ready for bank%d,"
					" when issue command: 0x%x\n",
					dev_name(&nand->pdev->dev),
					nand_if->bank,
					nand_if->curr_command);
		ret = 1;
	} else {

		if (nand_if->busy_gpio != -1) {
			ret = jz4780_nand_chip_is_ready(nand, nand_if);
		} else {
			nand->udelay(MAX_RB_DELAY_US);
			ret = 1;
		}
	}

	/*
	 * Apply this short delay always to ensure that we do wait tRR in
	 * any case on any machine.
	 */
	nand->ndelay(100);

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
		nand->ndelay(100);

		/* deselect current NAND flash chip */
		nand_if =
			nand->nand_flash_if_table[nand->curr_nand_flash_if];
		nand->gpemc_enable_nand_flash(&nand_if->cs, 0);
	} else {
		/* select new NAND flash chip */
		nand_if = nand->nand_flash_if_table[chip];
		nand->gpemc_enable_nand_flash(&nand_if->cs, 1);

		this->IO_ADDR_R = nand_if->cs.io_nand_dat;
		this->IO_ADDR_W = nand_if->cs.io_nand_dat;

		/* reconfigure DMA */
		if (nand->use_dma &&
				nand->curr_nand_flash_if != chip) {
			nand->dma_pipe_nand.cfg.src_addr =
					(dma_addr_t)CPHYSADDR(this->IO_ADDR_R);
			nand->dma_pipe_nand.cfg.dst_addr =
					(dma_addr_t)CPHYSADDR(this->IO_ADDR_W);
		}

		nand->curr_nand_flash_if = chip;

		/*
		 * Apply this short delay always to ensure that we do wait tCS in
		 * any case on any machine.
		 */
		nand->ndelay(100);
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
			if (nand_info->type == BANK_TYPE_NAND)
				nand->ndelay(nand_info->nand_timing.
						common_nand_timing.busy_wait_timing.Tadl);
			else {
				/*
				 * TODO
				 * implement Tadl delay
				 */
			}
			break;

		case NAND_CMD_RNDIN:
			/*
			 * Apply this short delay to meet Tcwaw
			 */
			if (nand_info->type == BANK_TYPE_NAND)
				nand->ndelay(nand_info->nand_timing.
						common_nand_timing.busy_wait_timing.Tcwaw);
			else {
				/*
				 * TODO
				 * implement Tcwaw delay
				 */
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
			mdelay(nand, MAX_RESET_DELAY_MS);
			break;
		}

		nand->udelay(chip->chip_delay);
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
			nand->udelay(chip->chip_delay);
			return;
		}
	}
	/*
	 * Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine.
	 */
	nand->ndelay(100);

	nand->nand_wait_ready(mtd);

	nand->busy_poll = old_busy_poll;
}

static inline void jz4780_nand_command_readoob_lp(struct mtd_info *mtd,
		int column, int page_addr)
{
	register struct nand_chip *chip = mtd->priv;
	unsigned int command = NAND_CMD_READ0;

	struct jz4780_nand *nand = mtd_to_jz4780_nand(mtd);

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
		nand->udelay(chip->chip_delay);
		return;
	}

	/*
	 * Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine.
	 */
	nand->ndelay(100);

	nand->nand_wait_ready(mtd);

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
		command = NAND_CMD_READ0;
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
				if (nand_info->type == BANK_TYPE_NAND)
					nand->ndelay(nand_info->nand_timing.
							common_nand_timing.busy_wait_timing.Tadl);
				else {
					/*
					 * TODO
					 * implement Tadl delay
					 */
				}
				break;

			case NAND_CMD_RNDIN:
				/*
				 * Apply this short delay to meet Tcwaw
				 */
				if (nand_info->type == BANK_TYPE_NAND)
					nand->ndelay(nand_info->nand_timing.
							common_nand_timing.busy_wait_timing.Tcwaw);
				else {
					/*
					 * TODO
					 * implement Tcwaw delay
					 */
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
		nand->udelay(chip->chip_delay);
		return;

	case NAND_CMD_RESET:
		if (chip->dev_ready) {
			/*
			 * Apply this short delay always to ensure that we do wait tRST in
			 * any case on any machine.
			 */
			mdelay(nand, MAX_RESET_DELAY_MS);
			break;
		}

		nand->udelay(chip->chip_delay);
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
		if (nand_info->type == BANK_TYPE_NAND)
			nand->ndelay(nand_info->nand_timing.
					common_nand_timing.busy_wait_timing.Twhr2);
		else {
			/*
			 * TODO
			 * implement Twhr2 delay
			 */
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
			nand->udelay(chip->chip_delay);
			return;
		}
	}

	/*
	 * Apply this short delay always to ensure that we do wait tWB in
	 * any case on any machine.
	 */
	nand->ndelay(100);

	nand->nand_wait_ready(mtd);
}

static void
jz4780_nand_resolve_reloc_codes(void *to, void *from, unsigned long size)
{
	union mips_instruction *insn = (union mips_instruction *)to;
	unsigned long offset = (unsigned long)to - (unsigned long)from;
	unsigned long i;

	for (i = 0; i < size / sizeof(int); i++) {
		switch (insn[i].i_format.opcode) {
		case j_op:
			insn[i].j_format.target +=
					(unsigned int)(offset >> 2) & 0x3ffffff;
			break;

		default:
			break;
		}
	}
}

static void *
jz4780_nand_load_func_to_tcsm(struct jz4780_nand *nand, void *func)
{
	unsigned long int size;
	unsigned long int offset;

	char modname[50];
	char name[50];
	int ret;

	struct cpumask mask;
	int cpu;
	void *tcsm = 0;
	extern unsigned int get_cpu_tcsm(int cpu,int len,char *name);
	extern int nr_cpu_ids;

	if (func) {
		ret = lookup_symbol_attrs((unsigned long)func,
				&size, &offset, modname, name);
		if (ret)
			goto err_return;

		cpumask_setall(&mask);
		for_each_cpu(cpu, &mask) {
			tcsm = (void *)get_cpu_tcsm(cpu, size, name);
			if (!tcsm) {
				ret = -ENOMEM;

				/*
				 * TODO release TCSM we got
				 */
				goto err_return;
			}
			memcpy(tcsm, func, size);
			jz4780_nand_resolve_reloc_codes(tcsm, func, size);
			dma_cache_wback_inv((unsigned long)tcsm, size);
		}
	}

err_return:
	return tcsm;
}

static int jz4780_nand_reloc_hot_to_tcsm(struct jz4780_nand *nand)
{
	struct nand_chip *chip = &nand->chip;
	void *addr;

	/*
	 * reloc following functions
	 *
	 * chip->cmdfunc;
	 * chip->dev_ready;
	 * chip->select_chip;
	 * chip->cmd_ctrl;
	 */

	addr = jz4780_nand_load_func_to_tcsm(nand, chip->cmdfunc);
	if (!addr)
		goto err_return;
	chip->cmdfunc = addr;

	addr = jz4780_nand_load_func_to_tcsm(nand, chip->dev_ready);
	if (!addr)
		goto err_return;
	chip->dev_ready = addr;

	addr = jz4780_nand_load_func_to_tcsm(nand, chip->select_chip);
	if (!addr)
		goto err_return;
	chip->select_chip = addr;

	addr = jz4780_nand_load_func_to_tcsm(nand, chip->cmd_ctrl);
	if (!addr)
		goto err_return;
	chip->cmd_ctrl = addr;

	blast_dcache32();
	blast_icache32();

	return 0;

err_return:
	return -ENOMEM;
}

static void jz4780_nand_unreloc_hot_from_tcsm(struct jz4780_nand *nand)
{
	/*
	 * TODO: need tcsm_put
	 */
}

static bool jz4780_nand_dma_nand_filter(struct dma_chan *chan,
		void *filter_param)
{
	struct jz4780_nand *nand = container_of(filter_param,
			struct jz4780_nand, dma_pipe_nand);

	/*
	 * chan_id must 30, also is PHY channel1,
	 * i did some special modification for channel1
	 * of ingenic dmaenginc codes.
	 */
	return (int)chan->private == (int)nand->dma_pipe_nand.type &&
			chan->chan_id == 30;
}

static int jz4780_nand_request_dma(struct jz4780_nand* nand)
{
	nand_flash_if_t *nand_if;
	struct jzdma_master *dma_master;
	unsigned int reg_dmac;
	dma_cap_mask_t mask;
	int ret = 0;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);

	/*
	 * request NAND channel
	 */
	nand->dma_pipe_nand.type = JZDMA_REQ_AUTO;
	nand->dma_pipe_nand.chan = dma_request_channel(mask,
			jz4780_nand_dma_nand_filter, &nand->dma_pipe_nand);
	if (!nand->dma_pipe_nand.chan)
		return -ENXIO;

	/*
	 * basic configure DMA channel
	 */
	dma_master = to_jzdma_chan(nand->dma_pipe_nand.chan)->master;
	reg_dmac = readl(dma_master->iomem + DMAC);
	if (!(reg_dmac & BIT(1))) {
		/*
		 * enable special channel0,1
		 */
		writel(reg_dmac | BIT(1), dma_master->iomem + DMAC);

		dev_info(&nand->pdev->dev, "enable DMA"
				" special channel<0, 1>\n");
	}

	nand->dma_buf.buf = dma_alloc_coherent(&nand->pdev->dev,
			DMA_BUF_SIZE, &nand->dma_buf.phy_addr, GFP_KERNEL);
	nand->dma_buf.size = DMA_BUF_SIZE;
	if (!nand->dma_buf.buf) {
		ret = -ENOMEM;
		goto err_release_nand_channel;
	}

	dma_cache_wback_inv(CKSEG0ADDR(nand->dma_buf.buf), nand->dma_buf.size);

	nand_if = nand->nand_flash_if_table[0];
	nand->dma_pipe_nand.cfg.dst_addr =
			(dma_addr_t)CPHYSADDR(nand_if->cs.io_nand_dat);
	nand->dma_pipe_nand.cfg.src_addr =
			(dma_addr_t)CPHYSADDR(nand_if->cs.io_nand_dat);

	return 0;

err_release_nand_channel:
	dma_release_channel(nand->dma_pipe_nand.chan);

	return ret;
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
		seq_printf(m, "ECC bits: %d\n", nand->chip.ecc.strength);
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

static inline void jz4780_nand_cpu_read_buf(struct mtd_info *mtd,
		uint8_t *buf, int len)
{
	int i;
	struct nand_chip *chip = mtd->priv;

	for (i = 0; i < len; i++)
		buf[i] = readb(chip->IO_ADDR_R);
}

static inline void jz4780_nand_noirq_dma_read(struct mtd_info *mtd,
		uint8_t *addr, int len)
{
	struct jz4780_nand *nand = mtd_to_jz4780_nand(mtd);
	struct jzdma_channel *dmac_nand;
	unsigned long timeo;
	int bytes_left;

	int *dst_buf = (int *)addr;
	int *src_buf = (int *)nand->dma_buf.buf;
	int i;

	/*
	 * TODO:
	 * this is ugly but got great speed gain
	 * this stuff implement none-interrupt DMA transfer
	 * and raw NAND read speed gain is about 40%
	 *
	 * fix can not invalidate cache under high memory
	 * this problem is caused by our kernel porting
	 *
	 * now we use following data path to workaround.
	 * NAND ---> KSEG1 Buffer ---> buffer
	 *      DMA          CPU copy back
	 *
	 * step1. configure NAND channel, NAND -> KSEG1 buffer
	 */
	dmac_nand = to_jzdma_chan(nand->dma_pipe_nand.chan);

	writel(nand->dma_buf.phy_addr, dmac_nand->iomem + CH_DTA);
	writel(nand->dma_pipe_nand.cfg.src_addr,
			dmac_nand->iomem + CH_DSA);
	writel(DCM_CH1_NAND_TO_DDR | DCM_DAI |
			(7 << 8), dmac_nand->iomem + CH_DCM);
	writel(len, dmac_nand->iomem + CH_DTC);
	writel(nand->dma_pipe_nand.type, dmac_nand->iomem + CH_DRT);

	/*
	 * step2. start NAND transfer
	 */
	writel(BIT(31) | BIT(0), dmac_nand->iomem + CH_DCS);

	/*
	 * step3. wait for NAND transfer done
	 */
	timeo = jiffies + msecs_to_jiffies(MAX_DMA_TRANSFER_TIMOUT_MS);
	do {
		if (readl(dmac_nand->iomem + CH_DCS) & DCS_TT)
			break;

		cond_resched();
	} while (time_before(jiffies, timeo));

	if (!(readl(dmac_nand->iomem + CH_DCS) & DCS_TT)) {
		writel(0, dmac_nand->iomem + CH_DCS);

		WARN(1, "Timeout when DMA read NAND for %dms.\n",
				MAX_DMA_TRANSFER_TIMOUT_MS);
		/*
		 * emergency CPU transfer
		 */
		bytes_left = readl(dmac_nand->iomem + CH_DTC);

		for (i = 0; i < len - bytes_left; i++)
			addr[i] = ((uint8_t *)nand->dma_buf.buf)[i];

		jz4780_nand_cpu_read_buf(mtd,
				(uint8_t *)(addr + len - bytes_left),
				bytes_left);

		return;
	}

	/*
	 * step4. stop NAND transfer
	 */
	writel(0, dmac_nand->iomem + CH_DCS);

	/*
	 * step5. copy back KSEG1 buffer -> buffer
	 */
	for (i = 0; i < (len >> 2); i += 16) {
		dst_buf[i +  0] = src_buf[i +  0];
		dst_buf[i +  1] = src_buf[i +  1];
		dst_buf[i +  2] = src_buf[i +  2];
		dst_buf[i +  3] = src_buf[i +  3];
		dst_buf[i +  4] = src_buf[i +  4];
		dst_buf[i +  5] = src_buf[i +  5];
		dst_buf[i +  6] = src_buf[i +  6];
		dst_buf[i +  7] = src_buf[i +  7];

		dst_buf[i +  8] = src_buf[i +  8];
		dst_buf[i +  9] = src_buf[i +  9];
		dst_buf[i + 10] = src_buf[i + 10];
		dst_buf[i + 11] = src_buf[i + 11];
		dst_buf[i + 12] = src_buf[i + 12];
		dst_buf[i + 13] = src_buf[i + 13];
		dst_buf[i + 14] = src_buf[i + 14];
		dst_buf[i + 15] = src_buf[i + 15];
	}
}

static void jz4780_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	if (((uint32_t)buf & 0x3) || (len & 0x1f))
		jz4780_nand_cpu_read_buf(mtd, buf, len);
	else
		jz4780_nand_noirq_dma_read(mtd, buf, len);
}

static inline void jz4780_nand_cpu_write_buf(struct mtd_info *mtd,
		const uint8_t *buf, int len)
{
	int i;
	struct nand_chip *chip = mtd->priv;

	for (i = 0; i < len; i++)
		writeb(buf[i], chip->IO_ADDR_W);
}

static inline void jz4780_nand_noirq_dma_write(struct mtd_info *mtd,
		const uint8_t *addr, int len)
{
	struct jz4780_nand *nand = mtd_to_jz4780_nand(mtd);
	struct jzdma_channel *dmac_nand;
	unsigned long timeo;
	int bytes_left;

	int *dst_buf = (int *)nand->dma_buf.buf;
	int *src_buf = (int *)addr;
	int i;

	/*
	 * TODO:
	 * this is ugly but got great speed gain
	 * this stuff implement none-interrupt DMA transfer
	 * and raw NAND read speed gain is about 40%
	 *
	 * fix can not invalidate cache under high memory
	 * this problem is caused by our kernel porting
	 *
	 * now we use following data path to workaround.
	 * buffer ---> KSEG1 Buffer ---> NAND
	 *        CPU               DMA
	 *
     *
	 * step1. buffer -> KSEG1 buffer
	 */
	for (i = 0; i < (len >> 2); i += 16) {
		dst_buf[i +  0] = src_buf[i +  0];
		dst_buf[i +  1] = src_buf[i +  1];
		dst_buf[i +  2] = src_buf[i +  2];
		dst_buf[i +  3] = src_buf[i +  3];
		dst_buf[i +  4] = src_buf[i +  4];
		dst_buf[i +  5] = src_buf[i +  5];
		dst_buf[i +  6] = src_buf[i +  6];
		dst_buf[i +  7] = src_buf[i +  7];

		dst_buf[i +  8] = src_buf[i +  8];
		dst_buf[i +  9] = src_buf[i +  9];
		dst_buf[i + 10] = src_buf[i + 10];
		dst_buf[i + 11] = src_buf[i + 11];
		dst_buf[i + 12] = src_buf[i + 12];
		dst_buf[i + 13] = src_buf[i + 13];
		dst_buf[i + 14] = src_buf[i + 14];
		dst_buf[i + 15] = src_buf[i + 15];
	}

	/*
	 * step2. configure NAND channel, NAND -> KSEG1 buffer
	 */
	dmac_nand = to_jzdma_chan(nand->dma_pipe_nand.chan);

	writel(nand->dma_pipe_nand.cfg.dst_addr, dmac_nand->iomem + CH_DTA);
	writel(nand->dma_buf.phy_addr,
			dmac_nand->iomem + CH_DSA);
	writel(DCM_CH1_DDR_TO_NAND | DCM_SAI |
			(7 << 8), dmac_nand->iomem + CH_DCM);
	writel(len, dmac_nand->iomem + CH_DTC);
	writel(nand->dma_pipe_nand.type, dmac_nand->iomem + CH_DRT);

	/*
	 * step3. start NAND transfer
	 */
	writel(BIT(31) | BIT(0), dmac_nand->iomem + CH_DCS);

	/*
	 * step4. wait for NAND transfer done
	 */
	timeo = jiffies + msecs_to_jiffies(MAX_DMA_TRANSFER_TIMOUT_MS);
	do {
		if (readl(dmac_nand->iomem + CH_DCS) & DCS_TT)
			break;

		cond_resched();
	} while (time_before(jiffies, timeo));

	if (!(readl(dmac_nand->iomem + CH_DCS) & DCS_TT)) {
		writel(0, dmac_nand->iomem + CH_DCS);

		WARN(1, "Timeout when DMA read NAND for %dms.\n",
				MAX_DMA_TRANSFER_TIMOUT_MS);
		/*
		 * emergency CPU transfer
		 */
		bytes_left = readl(dmac_nand->iomem + CH_DTC);

		jz4780_nand_cpu_write_buf(mtd,
				(uint8_t *)(addr + len - bytes_left),
				bytes_left);

		return;
	}

	/*
	 * step5. stop NAND transfer
	 */
	writel(0, dmac_nand->iomem + CH_DCS);
}

static void jz4780_nand_write_buf(struct mtd_info *mtd,
		const uint8_t *buf, int len)
{
	if (((uint32_t)buf & 0x3) || (len & 0x1f))
		jz4780_nand_cpu_write_buf(mtd, buf, len);
	else
		jz4780_nand_noirq_dma_write(mtd, buf, len);
}

static void jz4780_nand_do_swecc(struct jz4780_nand *__unused,
		struct jz4780_nand_swecc_pipe *swecc_pipe)
{
	struct jz4780_nand_swecc_req *swecc_req;

	if (swecc_pipe->op_type_calculate) {
		while (atomic_read(&swecc_pipe->curr_index) != -1) {
			swecc_req = &swecc_pipe->
					req[atomic_read(&swecc_pipe->curr_index)];

			swecc_pipe->calculate(swecc_pipe->nbc, swecc_pipe->ecc,
					swecc_req->buf, swecc_req->ecc_calc);

			atomic_dec(&swecc_pipe->curr_index);
		}
	} else {
		while (atomic_read(&swecc_pipe->curr_index) != -1) {
			swecc_req = &swecc_pipe->
					req[atomic_read(&swecc_pipe->curr_index)];

			swecc_req->correct_state =
					swecc_pipe->correct(swecc_pipe->nbc, swecc_pipe->ecc,
							swecc_req->buf, swecc_req->ecc_code,
							swecc_req->ecc_calc);

			atomic_dec(&swecc_pipe->curr_index);
		}
	}
}

static int jz4780_nand_swecc_thread(void *param)
{
	struct jz4780_nand *nand = param;
	struct jz4780_nand_swecc_pipe *swecc_pipe = &nand->swecc_pipe;
	unsigned long timeo = jiffies;

	set_freezable();

	do {
		do {
			if (atomic_read(&swecc_pipe->curr_index) == -1)
				continue;

			jz4780_nand_do_swecc(nand, swecc_pipe);

			timeo = jiffies +
						msecs_to_jiffies(MAX_SWECC_REQ_POLL_TIME_MS);

		} while (time_before(jiffies, timeo));

		wait_event_freezable(swecc_pipe->wait,
				atomic_read(&swecc_pipe->curr_index) != -1 ||
				kthread_should_stop());

	} while (!kthread_should_stop());

	return 0;
}

static int jz4780_nand_sw_ecc_calculate(struct nand_bch_control *nbc,
		struct nand_ecc_ctrl *ecc,
		const uint8_t *buf, uint8_t *code)
{
	unsigned int i;

	memset(code, 0, ecc->bytes);
	encode_bch(nbc->bch, buf, ecc->size, code);

	/* apply mask so that an erased page is a valid codeword */
	for (i = 0; i < ecc->bytes; i++)
		code[i] ^= nbc->eccmask[i];

	return 0;
}

static int jz4780_nand_sw_ecc_correct(struct nand_bch_control *nbc,
		struct nand_ecc_ctrl *ecc, uint8_t *buf,
		uint8_t *read_ecc, uint8_t *calc_ecc)
{
	unsigned int *errloc = nbc->errloc;
	int i, count;

	count = decode_bch(nbc->bch, NULL, ecc->size, read_ecc, calc_ecc,
			   NULL, errloc);
	if (count > 0) {
		for (i = 0; i < count; i++) {
			if (errloc[i] < (ecc->size << 3))
				/* error is located in data, correct it */
				buf[errloc[i] >> 3] ^= (1 << (errloc[i] & 7));
			/* else error in ecc, no action needed */
		}
	} else if (count < 0) {
		printk(KERN_ERR "ecc unrecoverable error\n");
		count = -1;
	}
	return count;
}

static int jz4780_nand_create_swecc_pipe(struct jz4780_nand *nand)
{
	struct mtd_info *mtd = &nand->mtd;
	struct nand_chip *chip = &nand->chip;
	struct jz4780_nand_swecc_pipe *swecc_pipe;
	int ret = 0;

	extern int nr_cpu_ids;
	if (nr_cpu_ids < 2)
		return 0;

	/*
	 * another thread pipe
	 */
	swecc_pipe = &nand->swecc_pipe;

	atomic_set(&swecc_pipe->curr_index, -1);
	init_waitqueue_head(&swecc_pipe->wait);

	swecc_pipe->ecc = &chip->ecc;
	swecc_pipe->nbc = nand_bch_init(mtd, chip->ecc.size,
			chip->ecc.bytes, &chip->ecc.layout);
	swecc_pipe->calculate = jz4780_nand_sw_ecc_calculate;
	swecc_pipe->correct = jz4780_nand_sw_ecc_correct;

	swecc_pipe->task = kthread_run(jz4780_nand_swecc_thread, nand,
			"nand-swecc");
	if (IS_ERR(swecc_pipe->task)) {
		ret = PTR_ERR(swecc_pipe->task);
		dev_err(&nand->pdev->dev, "Failed to start swecc thread: %d\n", ret);
		goto err_return;
	}

	/*
	 * current thread pipe
	 */
	swecc_pipe = &nand->swecc_current;

	swecc_pipe->ecc = &chip->ecc;
	swecc_pipe->nbc = nand_bch_init(mtd, chip->ecc.size,
			chip->ecc.bytes, &chip->ecc.layout);
	swecc_pipe->calculate = jz4780_nand_sw_ecc_calculate;
	swecc_pipe->correct = jz4780_nand_sw_ecc_correct;

	atomic_set(&swecc_pipe->curr_index, -1);
	init_waitqueue_head(&swecc_pipe->wait);

err_return:
	return ret;
}

static void jz4780_nand_destory_swecc_pipe(struct jz4780_nand *nand)
{
	kthread_stop(nand->swecc_pipe.task);
	nand_bch_free(nand->swecc_pipe.nbc);
	nand_bch_free(nand->swecc_current.nbc);
}

static int jz4780_nand_read_page_swecc(struct mtd_info *mtd,
		struct nand_chip *chip, uint8_t *buf, int oob_required, int page)
{
	int i, j, k, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	int half_eccsetps = eccsteps >> 1;
	uint8_t *p = buf;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	uint8_t *ecc_code = chip->buffers->ecccode;
	uint32_t *eccpos = chip->ecc.layout->eccpos;
	unsigned int max_bitflips = 0;

	struct jz4780_nand *nand = mtd_to_jz4780_nand(mtd);
	struct jz4780_nand_swecc_pipe *swecc_pipe = &nand->swecc_pipe;
	struct jz4780_nand_swecc_pipe *swecc_current = &nand->swecc_current;
	extern int nr_cpu_ids;

	chip->ecc.read_page_raw(mtd, chip, buf, 1, page);

	if (nr_cpu_ids < 2) {
		/*
		 * single thread
		 */
		for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize)
			chip->ecc.calculate(mtd, p, &ecc_calc[i]);

		/*
		 * prepare read ECC codes
		 */
		j = chip->ecc.total >> 5;
		for (i = 0, k = 0; i < (j << 3); i += 8, k = i << 2) {
			((uint32_t *)ecc_code)[i + 0] =
					((uint32_t *)chip->oob_poi)[eccpos[k + 0] >> 2];

			((uint32_t *)ecc_code)[i + 1] =
					((uint32_t *)chip->oob_poi)[eccpos[k + 4] >> 2];

			((uint32_t *)ecc_code)[i + 2] =
					((uint32_t *)chip->oob_poi)[eccpos[k + 8] >> 2];

			((uint32_t *)ecc_code)[i + 3] =
					((uint32_t *)chip->oob_poi)[eccpos[k + 12] >> 2];

			((uint32_t *)ecc_code)[i + 4] =
					((uint32_t *)chip->oob_poi)[eccpos[k + 16] >> 2];

			((uint32_t *)ecc_code)[i + 5] =
					((uint32_t *)chip->oob_poi)[eccpos[k + 20] >> 2];

			((uint32_t *)ecc_code)[i + 6] =
					((uint32_t *)chip->oob_poi)[eccpos[k + 24] >> 2];

			((uint32_t *)ecc_code)[i + 7] =
					((uint32_t *)chip->oob_poi)[eccpos[k + 28] >> 2];
		}

		k = i << 2;
		j = chip->ecc.total & 0x1f;
		for (i = k; i < k + j; i++)
			ecc_code[i] = chip->oob_poi[eccpos[i]];

		eccsteps = chip->ecc.steps;
		p = buf;

		for (i = 0 ; eccsteps; eccsteps--, i += eccbytes, p += eccsize) {
			int stat;

			stat = chip->ecc.correct(mtd, p, &ecc_code[i], &ecc_calc[i]);
			if (stat < 0) {
				mtd->ecc_stats.failed++;
			} else {
				mtd->ecc_stats.corrected += stat;
				max_bitflips = max_t(unsigned int, max_bitflips, stat);
			}
		}
		return max_bitflips;

	} else {
		/*
		 * dual threads
		 */

		/*
		 * prepare current thread for ECC calculate
		 */
		swecc_current->op_type_calculate = 1;
		for (i = 0, j = 0; j < half_eccsetps;
				i += eccbytes, j++, p += eccsize) {
			swecc_current->req[j].buf = p;
			swecc_current->req[j].ecc_calc = &ecc_calc[i];
			swecc_current->req[j].ecc_code = &ecc_code[i];
		}
		atomic_set(&swecc_current->curr_index, half_eccsetps - 1);

		/*
		 * prepare another thread for ECC calculate
		 */
		swecc_pipe->op_type_calculate = 1;
		for (j = 0; j < half_eccsetps;
				i += eccbytes, j++, p += eccsize) {
			swecc_pipe->req[j].buf = p;
			swecc_pipe->req[j].ecc_calc = &ecc_calc[i];
			swecc_pipe->req[j].ecc_code = &ecc_code[i];
		}

		/*
		 * start another thread for ECC calculate
		 */
		atomic_set(&swecc_pipe->curr_index, half_eccsetps - 1);
		wake_up(&nand->swecc_pipe.wait);

		/*
		 * start current thread for ECC calculate
		 */
		jz4780_nand_do_swecc(nand, swecc_current);

		/*
		 * wait for all done
		 */
		while (atomic_read(&swecc_pipe->curr_index) != -1)
			continue;

		/*
		 * prepare read ECC codes for ECC correct
		 */
		j = chip->ecc.total >> 5;
		for (i = 0, k = 0; i < (j << 3); i += 8, k = i << 2) {
			((uint32_t *)ecc_code)[i + 0] =
					((uint32_t *)chip->oob_poi)[eccpos[k + 0] >> 2];

			((uint32_t *)ecc_code)[i + 1] =
					((uint32_t *)chip->oob_poi)[eccpos[k + 4] >> 2];

			((uint32_t *)ecc_code)[i + 2] =
					((uint32_t *)chip->oob_poi)[eccpos[k + 8] >> 2];

			((uint32_t *)ecc_code)[i + 3] =
					((uint32_t *)chip->oob_poi)[eccpos[k + 12] >> 2];

			((uint32_t *)ecc_code)[i + 4] =
					((uint32_t *)chip->oob_poi)[eccpos[k + 16] >> 2];

			((uint32_t *)ecc_code)[i + 5] =
					((uint32_t *)chip->oob_poi)[eccpos[k + 20] >> 2];

			((uint32_t *)ecc_code)[i + 6] =
					((uint32_t *)chip->oob_poi)[eccpos[k + 24] >> 2];

			((uint32_t *)ecc_code)[i + 7] =
					((uint32_t *)chip->oob_poi)[eccpos[k + 28] >> 2];
		}

		k = i << 2;
		j = chip->ecc.total & 0x1f;
		for (i = k; i < k + j; i++)
			ecc_code[i] = chip->oob_poi[eccpos[i]];

		/*
		 * prepare current thread for ECC correct
		 */
		swecc_current->op_type_calculate = 0;
		atomic_set(&swecc_current->curr_index, half_eccsetps - 1);

		/*
		 * prepare another thread for ECC correct
		 */
		swecc_pipe->op_type_calculate = 0;

		/*
		 * start another thread for ECC correct
		 */
		atomic_set(&swecc_pipe->curr_index, half_eccsetps - 1);
		wake_up(&nand->swecc_pipe.wait);

		/*
		 * start current thread for ECC correct
		 */
		jz4780_nand_do_swecc(nand, swecc_current);

		/*
		 * wait for all done
		 */
		while (atomic_read(&swecc_pipe->curr_index) != -1)
			continue;

		/*
		 * errors check
		 */
		for (i = 0; i < half_eccsetps; i++) {
			int stat;

			stat = swecc_current->req[i].correct_state;
			if (stat < 0) {
				mtd->ecc_stats.failed++;
			} else {
				mtd->ecc_stats.corrected += stat;
				max_bitflips = max_t(unsigned int, max_bitflips, stat);
			}

			stat = swecc_pipe->req[i].correct_state;
			if (stat < 0) {
				mtd->ecc_stats.failed++;
			} else {
				mtd->ecc_stats.corrected += stat;
				max_bitflips = max_t(unsigned int, max_bitflips, stat);
			}
		}

		return max_bitflips;
	}
}

static int jz4780_nand_read_subpage_swecc(struct mtd_info *mtd,
		struct nand_chip *chip, uint32_t data_offs,
		uint32_t readlen, uint8_t *bufpoi)
{
	int start_step, end_step, num_steps;
	uint32_t *eccpos = chip->ecc.layout->eccpos;
	uint8_t *p;
	int data_col_addr, i, j, gaps = 0;
	int datafrag_len, eccfrag_len, aligned_len, aligned_pos;
	int busw = (chip->options & NAND_BUSWIDTH_16) ? 2 : 1;
	int index = 0;
	unsigned int max_bitflips = 0;

	struct jz4780_nand *nand = mtd_to_jz4780_nand(mtd);
	struct jz4780_nand_swecc_pipe *swecc_pipe = &nand->swecc_pipe;
	struct jz4780_nand_swecc_pipe *swecc_current = &nand->swecc_current;
	extern int nr_cpu_ids;

	/* Column address within the page aligned to ECC size (256bytes) */
	start_step = data_offs / chip->ecc.size;
	end_step = (data_offs + readlen - 1) / chip->ecc.size;
	num_steps = end_step - start_step + 1;

	/* Data size aligned to ECC ecc.size */
	datafrag_len = num_steps * chip->ecc.size;
	eccfrag_len = num_steps * chip->ecc.bytes;

	data_col_addr = start_step * chip->ecc.size;
	/* If we read not a page aligned data */
	if (data_col_addr != 0)
		chip->cmdfunc(mtd, NAND_CMD_RNDOUT, data_col_addr, -1);

	p = bufpoi + data_col_addr;
	chip->read_buf(mtd, p, datafrag_len);

	/* Calculate ECC */
	if (nr_cpu_ids < 2) {
		/*
		 * single thread
		 */
		for (i = 0; i < eccfrag_len ; i += chip->ecc.bytes, p += chip->ecc.size)
			chip->ecc.calculate(mtd, p, &chip->buffers->ecccalc[i]);
	} else {
		/*
		 * dual threads
		 */
		int partial_eccsteps = num_steps >> 1;

		/*
		 * prepare current thread for ECC calculate
		 */
		swecc_current->op_type_calculate = 1;
		for (i = 0, j = 0; j < partial_eccsteps;
				i += chip->ecc.bytes, j++, p += chip->ecc.size) {
			swecc_current->req[j].buf = p;
			swecc_current->req[j].ecc_calc = &chip->buffers->ecccalc[i];
			swecc_current->req[j].ecc_code = &chip->buffers->ecccode[i];
		}
		atomic_set(&swecc_current->curr_index, partial_eccsteps - 1);

		/*
		 * prepare another thread for ECC calculate
		 */
		partial_eccsteps = num_steps - partial_eccsteps;
		swecc_pipe->op_type_calculate = 1;
		for (j = 0; j < partial_eccsteps;
				i += chip->ecc.bytes, j++, p += chip->ecc.size) {
			swecc_pipe->req[j].buf = p;
			swecc_pipe->req[j].ecc_calc = &chip->buffers->ecccalc[i];
			swecc_pipe->req[j].ecc_code = &chip->buffers->ecccode[i];
		}

		/*
		 * start another thread for ECC calculate
		 */
		atomic_set(&swecc_pipe->curr_index, partial_eccsteps - 1);
		wake_up(&nand->swecc_pipe.wait);

		/*
		 * start current thread for ECC calculate
		 */
		jz4780_nand_do_swecc(nand, swecc_current);

		/*
		 * wait for all done
		 */
		while (atomic_read(&swecc_pipe->curr_index) != -1)
			continue;
	}

	/*
	 * The performance is faster if we position offsets according to
	 * ecc.pos. Let's make sure that there are no gaps in ECC positions.
	 */
	for (i = 0; i < eccfrag_len - 1; i++) {
		if (eccpos[i + start_step * chip->ecc.bytes] + 1 !=
			eccpos[i + start_step * chip->ecc.bytes + 1]) {
			gaps = 1;
			break;
		}
	}
	if (gaps) {
		chip->cmdfunc(mtd, NAND_CMD_RNDOUT, mtd->writesize, -1);
		chip->read_buf(mtd, chip->oob_poi, mtd->oobsize);
	} else {
		/*
		 * Send the command to read the particular ECC bytes take care
		 * about buswidth alignment in read_buf.
		 */
		index = start_step * chip->ecc.bytes;

		aligned_pos = eccpos[index] & ~(busw - 1);
		aligned_len = eccfrag_len;
		if (eccpos[index] & (busw - 1))
			aligned_len++;
		if (eccpos[index + (num_steps * chip->ecc.bytes)] & (busw - 1))
			aligned_len++;

		chip->cmdfunc(mtd, NAND_CMD_RNDOUT,
					mtd->writesize + aligned_pos, -1);
		chip->read_buf(mtd, &chip->oob_poi[aligned_pos], aligned_len);
	}

	for (i = 0; i < eccfrag_len; i++)
		chip->buffers->ecccode[i] = chip->oob_poi[eccpos[i + index]];

	if (nr_cpu_ids < 2) {
		/*
		 * single thread correct
		 */
		p = bufpoi + data_col_addr;

		for (i = 0; i < eccfrag_len ; i += chip->ecc.bytes, p += chip->ecc.size) {
			int stat;

			stat = chip->ecc.correct(mtd, p,
				&chip->buffers->ecccode[i], &chip->buffers->ecccalc[i]);
			if (stat < 0) {
				mtd->ecc_stats.failed++;
			} else {
				mtd->ecc_stats.corrected += stat;
				max_bitflips = max_t(unsigned int, max_bitflips, stat);
			}
		}
	} else {
		/*
		 * dual threads correct
		 */
		int partial_eccsteps = num_steps >> 1;

		/*
		 * prepare current thread for ECC correct
		 */
		swecc_current->op_type_calculate = 0;
		atomic_set(&swecc_current->curr_index, partial_eccsteps - 1);

		/*
		 * prepare another thread for ECC correct
		 */
		partial_eccsteps = num_steps - partial_eccsteps;
		swecc_pipe->op_type_calculate = 0;

		/*
		 * start another thread for ECC correct
		 */
		atomic_set(&swecc_pipe->curr_index, partial_eccsteps - 1);
		wake_up(&nand->swecc_pipe.wait);

		/*
		 * start current thread for ECC correct
		 */
		jz4780_nand_do_swecc(nand, swecc_current);

		/*
		 * wait for all done
		 */
		while (atomic_read(&swecc_pipe->curr_index) != -1)
			continue;

		/*
		 * errors check
		 */
		for (i = 0; i < (num_steps >> 1); i++) {
			int stat;

			stat = swecc_current->req[i].correct_state;
			if (stat < 0) {
				mtd->ecc_stats.failed++;
			} else {
				mtd->ecc_stats.corrected += stat;
				max_bitflips = max_t(unsigned int, max_bitflips, stat);
			}
		}

		for (i = 0; i < (num_steps - (num_steps >> 1)); i++) {
			int stat;

			stat = swecc_pipe->req[i].correct_state;
			if (stat < 0) {
				mtd->ecc_stats.failed++;
			} else {
				mtd->ecc_stats.corrected += stat;
				max_bitflips = max_t(unsigned int, max_bitflips, stat);
			}
		}
	}

	return max_bitflips;
}

static int jz4780_nand_write_page_swecc(struct mtd_info *mtd,
		struct nand_chip *chip, const uint8_t *buf, int oob_required)
{
	int i, j, k, eccsize = chip->ecc.size;
	int eccbytes = chip->ecc.bytes;
	int eccsteps = chip->ecc.steps;
	int half_eccsetps = eccsteps >> 1;
	uint8_t *ecc_calc = chip->buffers->ecccalc;
	const uint8_t *p = buf;
	uint32_t *eccpos = chip->ecc.layout->eccpos;

	struct jz4780_nand *nand = mtd_to_jz4780_nand(mtd);
	struct jz4780_nand_swecc_pipe *swecc_pipe = &nand->swecc_pipe;
	struct jz4780_nand_swecc_pipe *swecc_current = &nand->swecc_current;
	extern int nr_cpu_ids;

	/* Software ECC calculation */
	if (nr_cpu_ids < 2) {
		/*
		 * single thread
		 */
		for (i = 0; eccsteps; eccsteps--, i += eccbytes, p += eccsize)
			chip->ecc.calculate(mtd, p, &ecc_calc[i]);

		/*
		 * copy back ECC codes
		 */
		j = chip->ecc.total >> 5;
		for (i = 0, k = 0; i < (j << 3); i += 8, k = i << 2) {
			((uint32_t *)chip->oob_poi)[eccpos[k +  0] >> 2] =
					((uint32_t *)ecc_calc)[i + 0];

			((uint32_t *)chip->oob_poi)[eccpos[k +  4] >> 2] =
					((uint32_t *)ecc_calc)[i + 1];

			((uint32_t *)chip->oob_poi)[eccpos[k +  8] >> 2] =
					((uint32_t *)ecc_calc)[i + 2];

			((uint32_t *)chip->oob_poi)[eccpos[k + 12] >> 2] =
					((uint32_t *)ecc_calc)[i + 3];

			((uint32_t *)chip->oob_poi)[eccpos[k + 16] >> 2] =
					((uint32_t *)ecc_calc)[i + 4];

			((uint32_t *)chip->oob_poi)[eccpos[k + 20] >> 2] =
					((uint32_t *)ecc_calc)[i + 5];

			((uint32_t *)chip->oob_poi)[eccpos[k + 24] >> 2] =
					((uint32_t *)ecc_calc)[i + 6];

			((uint32_t *)chip->oob_poi)[eccpos[k + 28] >> 2] =
					((uint32_t *)ecc_calc)[i + 7];
		}

		k = i << 2;
		j = chip->ecc.total & 0x1f;
		for (i = k; i < k + j; i++)
			chip->oob_poi[eccpos[i]] = ecc_calc[i];

		return chip->ecc.write_page_raw(mtd, chip, buf, 1);
	} else {
		/*
		 * dual thread
		 */

		/*
		 * prepare current thread for ECC calculate
		 */
		swecc_current->op_type_calculate = 1;
		for (i = 0, j = 0; j < half_eccsetps;
				i += eccbytes, j++, p += eccsize) {
			swecc_current->req[j].buf = (uint8_t *)p;
			swecc_current->req[j].ecc_calc = &ecc_calc[i];
		}
		atomic_set(&swecc_current->curr_index, half_eccsetps - 1);

		/*
		 * prepare another thread for ECC calculate
		 */
		swecc_pipe->op_type_calculate = 1;
		for (j = 0; j < half_eccsetps;
				i += eccbytes, j++, p += eccsize) {
			swecc_pipe->req[j].buf = (uint8_t *)p;
			swecc_pipe->req[j].ecc_calc = &ecc_calc[i];
		}

		/*
		 * start another thread for ECC calculate
		 */
		atomic_set(&swecc_pipe->curr_index, half_eccsetps - 1);
		wake_up(&nand->swecc_pipe.wait);

		/*
		 * start current thread for ECC calculate
		 */
		jz4780_nand_do_swecc(nand, swecc_current);

		/*
		 * wait for all done
		 */
		while (atomic_read(&swecc_pipe->curr_index) != -1)
			continue;

		/*
		 * copy back ECC codes
		 */
		j = chip->ecc.total >> 5;
		for (i = 0, k = 0; i < (j << 3); i += 8, k = i << 2) {
			((uint32_t *)chip->oob_poi)[eccpos[k +  0] >> 2] =
					((uint32_t *)ecc_calc)[i + 0];

			((uint32_t *)chip->oob_poi)[eccpos[k +  4] >> 2] =
					((uint32_t *)ecc_calc)[i + 1];

			((uint32_t *)chip->oob_poi)[eccpos[k +  8] >> 2] =
					((uint32_t *)ecc_calc)[i + 2];

			((uint32_t *)chip->oob_poi)[eccpos[k + 12] >> 2] =
					((uint32_t *)ecc_calc)[i + 3];

			((uint32_t *)chip->oob_poi)[eccpos[k + 16] >> 2] =
					((uint32_t *)ecc_calc)[i + 4];

			((uint32_t *)chip->oob_poi)[eccpos[k + 20] >> 2] =
					((uint32_t *)ecc_calc)[i + 5];

			((uint32_t *)chip->oob_poi)[eccpos[k + 24] >> 2] =
					((uint32_t *)ecc_calc)[i + 6];

			((uint32_t *)chip->oob_poi)[eccpos[k + 28] >> 2] =
					((uint32_t *)ecc_calc)[i + 7];
		}

		k = i << 2;
		j = chip->ecc.total & 0x1f;
		for (i = k; i < k + j; i++)
			chip->oob_poi[eccpos[i]] = ecc_calc[i];

		return chip->ecc.write_page_raw(mtd, chip, buf, 1);
	}
}

static int jz4780_nand_probe(struct platform_device *pdev)
{
	int ret = 0;
	int bank = 0;
	int i = 0, j = 0, k = 0, m = 0;
	int eccpos_start;
	int eccpos_gap;

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

	chip->options |= NAND_SKIP_BBTSCAN;

	/*
	 * for relocation
	 */
	nand->gpemc_enable_nand_flash = gpemc_enable_nand_flash;
	nand->nand_wait_ready = nand_wait_ready;
	nand->gpio_get_value_cansleep = gpio_get_value_cansleep;
	nand->wait_for_completion_timeout = wait_for_completion_timeout;
	nand->msecs_to_jiffies = msecs_to_jiffies;
	nand->printk = printk;
	nand->udelay = __udelay;
	nand->ndelay = __ndelay;

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
		WARN(1, "Unsupported NAND type.\n");
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
		chip->ecc.read_page = jz4780_nand_read_page_swecc;
		chip->ecc.read_subpage = jz4780_nand_read_subpage_swecc;
		chip->ecc.write_page = jz4780_nand_write_page_swecc;
		chip->ecc.size  = nand->curr_nand_flash_info->ecc_step.data_size / 4;
		chip->ecc.bytes = fls(8 * chip->ecc.size) *
				nand->curr_nand_flash_info->ecc_step.ecc_bits / 4 / 8;
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
		WARN(1, "Unsupported ECC type.\n");
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
	 * ECC codes are right and word aligned
	 * start position = (oobsize - eccbytes) @ word aligned address
	 */
	eccpos_start = mtd->oobsize - nand->ecclayout.eccbytes;
	eccpos_gap = eccpos_start & 0x3;
	eccpos_start >>= 2;
	eccpos_start <<= 2;
	for (bank = 0; bank < nand->ecclayout.eccbytes; bank++)
		nand->ecclayout.eccpos[bank] = eccpos_start + bank;

	if (mtd->oobsize < (nand->ecclayout.eccbytes + eccpos_gap +
			chip->badblockpos + 2)) {
		WARN(1, "ECC codes are out of OOB area.\n");
		BUG();
	}

	nand->ecclayout.oobfree->offset = chip->badblockpos + 2;
	nand->ecclayout.oobfree->length =
		mtd->oobsize - (nand->ecclayout.eccbytes + eccpos_gap +
				chip->badblockpos + 2);

	chip->ecc.layout = &nand->ecclayout;

	/* step3. create dual thread ECC pipe */
	ret = jz4780_nand_create_swecc_pipe(nand);
	if (ret) {
		dev_err(&nand->pdev->dev, "Failed to create swecc pipe.\n");
		goto err_free_ecc;
	}

	/*
	 * second phase NAND scan
	 */
	if (nand_scan_tail(mtd)) {
		ret = -ENXIO;
		goto err_free_ecc;
	}

#ifdef CONFIG_DEBUG_FS

	nand->debugfs_entry = jz4780_nand_debugfs_init(nand);
	if (IS_ERR(nand->debugfs_entry)) {
		dev_err(&pdev->dev, "Failed to register debugfs entry.\n");

		ret = PTR_ERR(nand->debugfs_entry);
		goto err_free_ecc;
	}

#endif

	/*
	 * relocate hot functions to TCSM
	 */
	if (pdata->try_to_reloc_hot) {
		ret = jz4780_nand_reloc_hot_to_tcsm(nand);
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
		goto err_unreloc_hot;
	}

	dev_info(&pdev->dev,
		"Successfully registered JZ4780 SoC NAND controller driver.\n");

	return 0;

err_unreloc_hot:
	if (pdata->try_to_reloc_hot)
		jz4780_nand_unreloc_hot_from_tcsm(nand);

err_debugfs_remove:
#ifdef CONFIG_DEBUG_FS

	debugfs_remove_recursive(nand->debugfs_entry);

#endif

err_free_ecc:
	if (pdata->ecc_type == NAND_ECC_TYPE_SW &&
			nand->swecc_pipe.task)
		jz4780_nand_destory_swecc_pipe(nand);

	if (pdata->ecc_type == NAND_ECC_TYPE_HW)
		kfree(nand->bch_req.errrept_data);

err_dma_release_channel:
	if (nand->xfer_type == NAND_XFER_DMA_IRQ ||
			nand->xfer_type == NAND_XFER_DMA_POLL)
		dma_release_channel(nand->dma_pipe_nand.chan);

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

#ifdef CONFIG_DEBUG_FS

	debugfs_remove_recursive(nand->debugfs_entry);

#endif

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

	if (nand->pdata->ecc_type == NAND_ECC_TYPE_SW &&
			nand->swecc_pipe.task)
		jz4780_nand_destory_swecc_pipe(nand);

	if (nand->pdata->ecc_type == NAND_ECC_TYPE_HW &&
			nand->bch_req.errrept_data)
		kfree(nand->bch_req.errrept_data);

	if (nand->xfer_type == NAND_XFER_DMA_IRQ ||
			nand->xfer_type == NAND_XFER_DMA_POLL)
		dma_release_channel(nand->dma_pipe_nand.chan);

	/*
	 * TODO
	 * "unreloc..." implements nothing
	 * so you may get in trouble when
	 * do "insmod jz4780_nand.ko"
	 * becuase of failed to allocate TCSM
	 */
	if (nand->pdata->try_to_reloc_hot)
		jz4780_nand_unreloc_hot_from_tcsm(nand);

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
