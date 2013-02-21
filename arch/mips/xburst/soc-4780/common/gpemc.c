/*
 *  Copyright (C) 2013 Fighter Sun <wanmyqawdr@126.com>
 *  GPEMC(NEMC) support functions
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
#include <linux/ioport.h>
#include <linux/clk.h>
#include <linux/slab.h>
#include <linux/err.h>

#include <soc/gpemc.h>

#define GPEMC_REGS_FILE_BASE 	0x13410000
#define CPEMC_NAND_REGS_FILE_BASE	0x13410050

#define BANK_COUNT 7
#define GPNEMC_CS1_IOBASE 0x1b000000
#define GPNEMC_CS2_IOBASE 0x1a000000
#define GPNEMC_CS3_IOBASE 0x19000000
#define GPNEMC_CS4_IOBASE 0x18000000
#define GPNEMC_CS5_IOBASE 0x17000000
#define GPNEMC_CS6_IOBASE 0x16000000

#define GPEMC_NAND_BANK_DATA_OFFSET	0
#define GPEMC_NAND_BANK_ADDR_OFFSET	0x800000
#define GPEMC_NAND_BANK_CMD_OFFSET	0x400000

static const u32 nT_to_adjs[] = {
	/* 0 ~ 10 */
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10,

	/* 11 ~ 12 */
	11, 11,

	/* 13 ~ 15*/
	12, 12, 12,

	/* 16 ~ 20 */
	13, 13, 13, 13, 13,

	/* 21 ~ 25 */
	14, 14, 14, 14, 14,

	/* 26 ~ 31 */
	15, 15, 15, 15, 15, 15
};

static inline u32 div_ceiling(u32 x, u32 y)
{
	return (x + y - 1) / y;
}

struct gpemc_nand_regs_file {
	volatile u32 nfcsr;

	volatile u32 pad0[(0x13410100 - CPEMC_NAND_REGS_FILE_BASE)
	                  / sizeof(u32)];

	volatile u32 pncr;
	volatile u32 pndr;
	volatile u32 bitcnt;
	volatile u32 tgwe;
	volatile u32 tgcr[6];
	volatile u32 tgsr;
	volatile u32 tgfl;
	volatile u32 tgfh;
	volatile u32 tgcl;
	volatile u32 tgch;
	volatile u32 tgpd;
	volatile u32 tgsl;
	volatile u32 tgsh;
	volatile u32 tgrr;
	volatile u32 tgdr;
	volatile u32 tghl;
	volatile u32 tghh;
};

struct gpemc_regs_file {
	volatile u32 pad0[0x14 / sizeof(u32)];

	volatile u32 smcr[6];

	volatile u32 pad1;

	volatile u32 sacr[6];
};

/* instance a singleton gpemc */
static struct {
	struct clk *clk;
	DECLARE_BITMAP(bank_use_map, 64);

	struct gpemc_regs_file __iomem * const regs_file;
	struct gpemc_nand_regs_file __iomem * const nand_regs_file;

	const struct resource regs_file_mem;
	const struct resource nand_regs_file_mem;

	const struct resource bank_mem[BANK_COUNT];
	const char *bank_name[BANK_COUNT];
} instance = {
	.regs_file = (struct gpemc_regs_file *)
					CKSEG1ADDR(GPEMC_REGS_FILE_BASE),
	.nand_regs_file = (struct gpemc_nand_regs_file *)
						CKSEG1ADDR(CPEMC_NAND_REGS_FILE_BASE),

	.regs_file_mem = {
		.start = GPEMC_REGS_FILE_BASE,
		.end = GPEMC_REGS_FILE_BASE + sizeof(struct gpemc_regs_file) - 1,
	},

	.nand_regs_file_mem = {
		.start = CPEMC_NAND_REGS_FILE_BASE,
		.end = CPEMC_NAND_REGS_FILE_BASE +
				sizeof(struct gpemc_nand_regs_file) - 1,
	},

#define GPEMC_BANK_SIZE (GPNEMC_CS1_IOBASE - GPNEMC_CS2_IOBASE)
	.bank_mem = {
		{},

		{
			.start = GPNEMC_CS1_IOBASE,
			.end = GPEMC_BANK_SIZE - 1,
		},

		{
			.start = GPNEMC_CS2_IOBASE,
			.end = GPEMC_BANK_SIZE - 1,
		},

		{
			.start = GPNEMC_CS3_IOBASE,
			.end = GPEMC_BANK_SIZE - 1,
		},

		{
			.start = GPNEMC_CS4_IOBASE,
			.end = GPEMC_BANK_SIZE - 1,
		},

		{
			.start = GPNEMC_CS5_IOBASE,
			.end = GPEMC_BANK_SIZE - 1,
		},

		{
			.start = GPNEMC_CS6_IOBASE,
			.end = GPEMC_BANK_SIZE - 1,
		}
	},

	.bank_name = {
		"",
		"gpemc-bank1-mem",
		"gpemc-bank2-mem",
		"gpemc-bank3-mem",
		"gpemc-bank4-mem",
		"gpemc-bank5-mem",
		"gpemc-bank6-mem",
	},
}, *gpemc = &instance; /* end instance a singleton gpemc */

int __init gpemc_init(void) {
	int i;

	struct resource *res;

	gpemc->clk = clk_get(NULL, "nemc");
	if (IS_ERR(gpemc->clk)) {
		pr_err("failed to get gpemc clock.\n");
		goto err_return;
	}

	res = request_mem_region(gpemc->regs_file_mem.start,
			resource_size(&gpemc->regs_file_mem), "gpemc-regs-mem");
	if (!res) {
		pr_err("gpemc: grab gpemc regs file failed.\n");
		goto err_return;
	}

	res = request_mem_region(gpemc->nand_regs_file_mem.start,
			resource_size(&gpemc->nand_regs_file_mem),
			"gpemc-nand-regs-mem");
	if (!res) {
		pr_err("gpemc: grab gpemc nand regs file failed.\n");
		goto err_release_mem;
	}

	for (i = 0; i < 64; i++)
		clear_bit(i, gpemc->bank_use_map);

	return 0;

err_release_mem:
	release_mem_region(gpemc->regs_file_mem.start,
			resource_size(&gpemc->regs_file_mem));

err_return:
	BUG();
	return -EBUSY;
}

postcore_initcall(gpemc_init);

int gpemc_request_cs(gpemc_bank_t *bank, int cs)
{
	struct resource *res;
	BUG_ON(cs < 1 || cs >= ARRAY_SIZE(gpemc->bank_mem));

	if (test_bit(cs, gpemc->bank_use_map)) {
		pr_err("gpemc: grab cs %d failed, it's busy.\n", cs);
		goto err_busy_bank;
	}

	res = request_mem_region(gpemc->bank_mem[cs].start,
			resource_size(&gpemc->bank_mem[cs]), gpemc->bank_name[cs]);
	if (!res) {
		pr_err("gpemc: grab bank %d memory failed, it's busy.\n", cs);
		goto err_busy_bank;
	}

	set_bit(cs, gpemc->bank_use_map);

	bank->cs = cs;
	bank->io_base = (void __iomem *)CKSEG1ADDR(gpemc->bank_mem[cs].start);

	bank->io_nand_dat = bank->io_base + GPEMC_NAND_BANK_DATA_OFFSET;
	bank->io_nand_addr = bank->io_base + GPEMC_NAND_BANK_ADDR_OFFSET;
	bank->io_nand_cmd = bank->io_base + GPEMC_NAND_BANK_CMD_OFFSET;

	return 0;

err_busy_bank:
	return -EBUSY;
}
EXPORT_SYMBOL(gpemc_request_cs);

void gpemc_release_cs(gpemc_bank_t* bank)
{
	BUG_ON(bank->cs < 1 || bank->cs >= ARRAY_SIZE(gpemc->bank_mem));
	if (!test_bit(bank->cs, gpemc->bank_use_map)) {
		WARN(1, "try to release a free cs.\n");
		return;
	}

	release_mem_region(gpemc->bank_mem[bank->cs].start,
			resource_size(&gpemc->bank_mem[bank->cs]));

	clear_bit(bank->cs, gpemc->bank_use_map);
}
EXPORT_SYMBOL(gpemc_release_cs);

void gpemc_fill_timing_from_nand(gpemc_bank_t *bank,
		common_nand_timing_t *timing)
{
	u32 temp;

	/* bank Taw */
	bank->bank_timing.sram_timing.Taw = timing->dc_timing.Twp;

	/* bank Tas */
	temp = max(timing->dc_timing.Tals, timing->dc_timing.Tcls);
	temp = max(temp, timing->dc_timing.Tcs);
	temp -= bank->bank_timing.sram_timing.Taw;
	bank->bank_timing.sram_timing.Tas = temp;

	/* bank Tah */
	temp = max(timing->dc_timing.Talh, timing->dc_timing.Tclh);
	temp = max(temp, timing->dc_timing.Tch);
	temp = max(temp, timing->dc_timing.Tdh);
	temp = max(temp, timing->dc_timing.Twh);
	temp = max(temp, (timing->dc_timing.Twc - timing->dc_timing.Twp));
	bank->bank_timing.sram_timing.Tah = temp;

	/* bank BW */
	bank->bank_timing.sram_timing.BW = timing->BW;

}
EXPORT_SYMBOL(gpemc_fill_timing_from_nand);

void gpemc_fill_timing_from_toggle(gpemc_bank_t *bank,
		toggle_nand_timing_t *timing)
{
	/*
	 * TODO
	 */
}
EXPORT_SYMBOL(gpemc_fill_timing_from_toggle);

void gpemc_fill_timing_from_sram(gpemc_bank_t *bank,
		sram_timing_t *timing)
{
	/*
	 * TODO
	 */
}
EXPORT_SYMBOL(gpemc_fill_timing_from_sram);

int gpemc_config_bank_timing(gpemc_bank_t *bank)
{
	u32 smcr, temp;
	u32 clk_T;

	/* T count in nanoseconds */
	clk_T = div_ceiling(1000 * 1000 * 1000, clk_get_rate(gpemc->clk));

	smcr = gpemc->regs_file->smcr[bank->cs];

	switch (bank->bank_type) {
	case bank_type_sram:
		/*
		 * TODO
		 */
		break;

	case bank_type_nand:
		/* Tah */
		temp = div_ceiling(bank->bank_timing.sram_timing.Tah, clk_T);
		temp = min(temp, (u32)15);
		smcr &= ~(0xf << 12);
		smcr |= temp << 12;

		/* Taw */
		temp = div_ceiling(bank->bank_timing.sram_timing.Taw, clk_T);
		temp = min(temp, (u32)31);
		temp = nT_to_adjs[temp];

		smcr &= ~(0xf << 20);
		smcr |= temp << 20;

		/* Tas */
		temp = div_ceiling(bank->bank_timing.sram_timing.Tas, clk_T);
		temp = min(temp, (u32)15);
		smcr &= ~(0xf << 8);
		smcr |= temp << 8;

		/* BW & BL*/
		smcr &= ~(0x3 << 6) | ~(0x3 << 1);
		smcr |= bank->bank_timing.sram_timing.BW << 6;

		gpemc->regs_file->smcr[bank->cs] = smcr;

		break;

	case bank_type_toggle:
		/*
		 * TODO
		 */
		break;

	default:
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(gpemc_config_bank_timing);

int gpemc_config_toggle_bank_timing(gpemc_bank_t *bank)
{
	/*
	 * TODO
	 */
	return 0;
}
EXPORT_SYMBOL(gpemc_config_toggle_bank_timing);

void gpemc_set_bank_as_common_nand(gpemc_bank_t *bank)
{
	u32 index;

	/* set nand type as common nand */
	index = 16 + bank->cs - 1;
	gpemc->nand_regs_file->nfcsr &= ~BIT(index);

	/* set bank role as nand */
	index = (bank->cs - 1) << 1;
	gpemc->nand_regs_file->nfcsr |= BIT(index);

	bank->bank_type = bank_type_nand;
}
EXPORT_SYMBOL(gpemc_set_bank_as_common_nand);

void gpemc_set_bank_as_toggle_nand(gpemc_bank_t *bank)
{
	u32 index;

	/* set nand type as toggle */
	index = 16 + bank->cs - 1;
	gpemc->nand_regs_file->nfcsr |= BIT(index);

	/* set bank role as nand */
	index = (bank->cs - 1) << 1;
	gpemc->nand_regs_file->nfcsr |= BIT(index);

	bank->bank_type = bank_type_toggle;
}
EXPORT_SYMBOL(gpemc_set_bank_as_toggle_nand);

void gpemc_set_bank_as_sram(gpemc_bank_t *bank)
{
	u32 index;

	/* set bank role as sram */
	index = (bank->cs - 1) << 1;
	gpemc->nand_regs_file->nfcsr &= ~BIT(index);

	bank->bank_type = bank_type_sram;
}
EXPORT_SYMBOL(gpemc_set_bank_as_sram);

void gpemc_enable_nand_flash(gpemc_bank_t *bank, bool enable)
{
	u32 index;

	index = ((bank->cs - 1) << 1) + 1;
	if (enable)
		gpemc->nand_regs_file->nfcsr |= BIT(index);
	else
		gpemc->nand_regs_file->nfcsr &= ~BIT(index);
}
EXPORT_SYMBOL(gpemc_enable_nand_flash);

bank_type_t gpemc_get_bank_type(gpemc_bank_t *bank)
{
	return bank->bank_type;
}
EXPORT_SYMBOL(gpemc_get_bank_type);
