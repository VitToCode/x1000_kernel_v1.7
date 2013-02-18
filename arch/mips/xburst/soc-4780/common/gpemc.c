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

#define BANK_COUNT 7
#define GPEMC_REGS_FILE_BASE 	0x13410000
#define CPEMC_NAND_REGS_FILE_BASE	0x13410050

#define GPNEMC_CS1_IOBASE 0x1b000000
#define GPNEMC_CS2_IOBASE 0x1a000000
#define GPNEMC_CS3_IOBASE 0x19000000
#define GPNEMC_CS4_IOBASE 0x18000000
#define GPNEMC_CS5_IOBASE 0x17000000
#define GPNEMC_CS6_IOBASE 0x16000000

struct gpemc_nand_regs_file {
	volatile u32 nfcsr;

	volatile u32 pad0[(0x13410100 - CPEMC_NAND_REGS_FILE_BASE)
	                  / sizeof(u32)];

	volatile u32 pncr;
	volatile u32 pndr;
	volatile u32 bitcnt;
	volatile u32 tgwe;
	volatile u32 tgcr1;
	volatile u32 tgcr2;
	volatile u32 tgcr3;
	volatile u32 tgcr4;
	volatile u32 tgcr5;
	volatile u32 tgcr6;
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

	volatile u32 SMCR1;
	volatile u32 SMCR2;
	volatile u32 SMCR3;
	volatile u32 SMCR4;
	volatile u32 SMCR5;
	volatile u32 SMCR6;

	volatile u32 pad1;

	volatile u32 SACR1;
	volatile u32 SACR2;
	volatile u32 SACR3;
	volatile u32 SACR4;
	volatile u32 SACR5;
	volatile u32 SACR6;
};

/* instance a singleton gpemc */
static struct {
	struct clk *clk;
	DECLARE_BITMAP(bank_use_map, 64);

	struct gpemc_regs_file __iomem * const regs_file;
	struct gpemc_nand_regs_file __iomem * const nand_regs_file;

	const struct resource regs_file_mem;
	const struct resource nand_regs_file_mem;

	const struct resource banks_mem[BANK_COUNT];
	const char *banks_name[BANK_COUNT];
} gpemc = {
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
	.banks_mem = {
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

	.banks_name = {
		"",
		"gpemc-bank1-mem",
		"gpemc-bank2-mem",
		"gpemc-bank3-mem",
		"gpemc-bank4-mem",
		"gpemc-bank5-mem",
		"gpemc-bank6-mem",
	},
}; /* end instance a singleton gpemc */

int __init gpemc_init(void) {
	int i;

	struct resource *res;

	gpemc.clk = clk_get(NULL, "nemc");
	if (IS_ERR(gpemc.clk)) {
		pr_err("failed to get gpemc clock.\n");
		goto err_return;
	}

	res = request_mem_region(gpemc.regs_file_mem.start,
			resource_size(&gpemc.regs_file_mem), "gpemc-regs-mem");
	if (!res) {
		pr_err("gpemc: grab gpemc regs file failed.\n");
		goto err_return;
	}

	res = request_mem_region(gpemc.nand_regs_file_mem.start,
			resource_size(&gpemc.nand_regs_file_mem),
			"gpemc-nand-regs-mem");
	if (!res) {
		pr_err("gpemc: grab gpemc nand regs file failed.\n");
		goto err_release_mem;
	}

	for (i = 0; i < 64; i++)
		clear_bit(i, gpemc.bank_use_map);

	return 0;

err_release_mem:
	release_mem_region(gpemc.regs_file_mem.start,
			resource_size(&gpemc.regs_file_mem));

err_return:
	BUG();
	return -EBUSY;
}

postcore_initcall(gpemc_init);


struct gpemc_bank *gpemc_request_cs(int cs)
{
	struct gpemc_bank *gpbank;
	struct resource *res;
	BUG_ON(cs < 1 || cs >= ARRAY_SIZE(gpemc.banks_mem));

	if (test_bit(cs, gpemc.bank_use_map)) {
		pr_err("gpemc: grab cs %d failed, it's busy.\n", cs);
		goto err_busy_bank;
	}

	res = request_mem_region(gpemc.banks_mem[cs].start,
			resource_size(&gpemc.banks_mem[cs]), gpemc.banks_name[cs]);
	if (!res) {
		pr_err("gpemc: grab bank %d memory failed, it's busy.\n", cs);
		goto err_busy_bank;
	}

	gpbank = kzalloc(sizeof(*gpbank), GFP_KERNEL);
	if (!gpbank) {
		pr_err("gpemc: alloc memory failed.\n");
		goto err_release_mem;
	}

	set_bit(cs, gpemc.bank_use_map);

	gpbank->cs = cs;
	gpbank->io_base = (void __iomem *)CKSEG1ADDR(gpemc.banks_mem[cs].start);
	gpbank->io_nand_dat = gpbank->io_base + GPEMC_NAND_BANK_DATA_OFFSET;
	gpbank->io_nand_addr = gpbank->io_base + GPEMC_NAND_BANK_ADDR_OFFSET;
	gpbank->io_nand_cmd = gpbank->io_base + GPEMC_NAND_BANK_CMD_OFFSET;

	return gpbank;

err_release_mem:
	release_mem_region(gpemc.banks_mem[cs].start,
			resource_size(&gpemc.banks_mem[cs]));

err_busy_bank:
	return ERR_PTR(-EBUSY);
}
EXPORT_SYMBOL(gpemc_request_cs);

void gpemc_release_cs(struct gpemc_bank* bank)
{
	BUG_ON(bank->cs < 1 || bank->cs >= ARRAY_SIZE(gpemc.banks_mem));
	if (!test_bit(bank->cs, gpemc.bank_use_map)) {
		WARN(1, "try to release a free cs.\n");
		return;
	}

	release_mem_region(gpemc.banks_mem[bank->cs].start,
			resource_size(&gpemc.banks_mem[bank->cs]));

	clear_bit(bank->cs, gpemc.bank_use_map);
}
EXPORT_SYMBOL(gpemc_release_cs);

int gpemc_config_bank_timing(struct gpemc_bank_timing *timing)
{
	return 0;
}
EXPORT_SYMBOL(gpemc_config_bank_timing);
