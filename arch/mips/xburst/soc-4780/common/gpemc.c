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
#include <linux/gpio.h>

#include <gpio.h>
#include <soc/gpio.h>
#include <soc/gpemc.h>

#define DRVNAME "jz4780-gpemc"

#define GPEMC_REGS_FILE_BASE 	0x13410000
#define CPEMC_NAND_REGS_FILE_BASE	0x13410050

#define BANK_COUNT 7
#define GPEMC_CS1_IOBASE 0x1b000000
#define GPEMC_CS2_IOBASE 0x1a000000
#define GPEMC_CS3_IOBASE 0x19000000
#define GPEMC_CS4_IOBASE 0x18000000
#define GPEMC_CS5_IOBASE 0x17000000
#define GPEMC_CS6_IOBASE 0x16000000

#define GPEMC_NAND_BANK_DATA_OFFSET	0
#define GPEMC_NAND_BANK_ADDR_OFFSET	0x800000
#define GPEMC_NAND_BANK_CMD_OFFSET	0x400000

#define JZ_GPIO_FUNC_MEM_CS     GPIO_FUNC_0
#define JZ_GPIO_FUNC_MEM_RD_WE  GPIO_FUNC_0
#define JZ_GPIO_FUNC_MEM_DATA   GPIO_FUNC_0
#define JZ_GPIO_FUNC_MEM_WAIT   GPIO_FUNC_0
#define JZ_GPIO_FUNC_MEM_ADDR   GPIO_FUNC_0

#define JZ_GPIO_FUNC_ALE_CLE    GPIO_FUNC_0
#define JZ_GPIO_FUNC_DQS        GPIO_FUNC_0

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

typedef struct {
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
} nand_regs_file_t;

typedef struct {
	volatile u32 pad0[0x14 / sizeof(u32)];

	volatile u32 smcr[6];

	volatile u32 pad1;

	volatile u32 sacr[6];
} regs_file_t;

struct addr_pins_pool {
	u32 addr_pins[6];
	u32 addr_pin_use_count[6];

	spinlock_t lock;
};

/* instance a singleton gpemc */
static struct {
	const u32 gpio_pin_data[8];
	struct addr_pins_pool addr_pins_pool;
	const u32 gpio_pin_rd_we[2];
	const u32 gpio_pin_wait;
	const u32 gpio_pin_dqs;
	const u32 gpio_pin_cs[7];

	atomic_t pin_data_use_count;
	atomic_t pin_wait_use_count;
	atomic_t pin_dqs_use_count;
	atomic_t pin_rd_we_use_count;

	struct clk *clk;
	DECLARE_BITMAP(bank_use_map, 64);

	regs_file_t __iomem * const regs_file;
	nand_regs_file_t __iomem * const nand_regs_file;

	const struct resource regs_file_mem;
	const struct resource nand_regs_file_mem;

	const struct resource bank_mem[BANK_COUNT];
	const char *bank_name[BANK_COUNT];
} instance = {
	.regs_file = (regs_file_t *)
					CKSEG1ADDR(GPEMC_REGS_FILE_BASE),
	.nand_regs_file = (nand_regs_file_t *)
						CKSEG1ADDR(CPEMC_NAND_REGS_FILE_BASE),

	.regs_file_mem = {
		.start = GPEMC_REGS_FILE_BASE,
		.end = GPEMC_REGS_FILE_BASE + sizeof(regs_file_t) - 1,
	},

	.nand_regs_file_mem = {
		.start = CPEMC_NAND_REGS_FILE_BASE,
		.end = CPEMC_NAND_REGS_FILE_BASE +
				sizeof(nand_regs_file_t) - 1,
	},

#define GPEMC_BANK_SIZE (GPEMC_CS1_IOBASE - GPEMC_CS2_IOBASE)
	.bank_mem = {
		{},

		{
			.start = GPEMC_CS1_IOBASE,
			.end = GPEMC_CS1_IOBASE + GPEMC_BANK_SIZE - 1,
		},

		{
			.start = GPEMC_CS2_IOBASE,
			.end = GPEMC_CS2_IOBASE + GPEMC_BANK_SIZE - 1,
		},

		{
			.start = GPEMC_CS3_IOBASE,
			.end = GPEMC_CS3_IOBASE + GPEMC_BANK_SIZE - 1,
		},

		{
			.start = GPEMC_CS4_IOBASE,
			.end = GPEMC_CS4_IOBASE + GPEMC_BANK_SIZE - 1,
		},

		{
			.start = GPEMC_CS5_IOBASE,
			.end = GPEMC_CS5_IOBASE + GPEMC_BANK_SIZE - 1,
		},

		{
			.start = GPEMC_CS6_IOBASE,
			.end = GPEMC_CS6_IOBASE + GPEMC_BANK_SIZE - 1,
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

	.gpio_pin_data = {
		GPIO_PA(0),
		GPIO_PA(1),
		GPIO_PA(2),
		GPIO_PA(3),
		GPIO_PA(4),
		GPIO_PA(5),
		GPIO_PA(6),
		GPIO_PA(7),
	},

	.addr_pins_pool.addr_pins = {
		/*
		 * also CLE for NAND
		 */
#define PIN_INDEX_OF_CLE	0
		[PIN_INDEX_OF_CLE] = GPIO_PB(0),

		/*
		 * also ALE for NAND
		 */
#define PIN_INDEX_OF_ALE	1
		[PIN_INDEX_OF_ALE] = GPIO_PB(1),

		GPIO_PB(2),
		GPIO_PB(3),
		GPIO_PB(4),
		GPIO_PB(5),
	},

	.gpio_pin_rd_we = {
		GPIO_PA(16),
		GPIO_PA(17),
	},

	.gpio_pin_wait = GPIO_PA(27),

	.gpio_pin_dqs = GPIO_PA(29),

	.gpio_pin_cs = {
		0,
		GPIO_PA(21),
		GPIO_PA(22),
		GPIO_PA(23),
		GPIO_PA(24),
		GPIO_PA(25),
		GPIO_PA(26),
	},

}, *gpemc = &instance; /* end instance a singleton gpemc */

int __init gpemc_init(void) {
	int i;

	struct resource *res;

	atomic_set(&gpemc->pin_data_use_count, 0);
	memset(gpemc->addr_pins_pool.addr_pin_use_count,
			sizeof(gpemc->addr_pins_pool.addr_pin_use_count), 0);
	atomic_set(&gpemc->pin_wait_use_count, 0);
	atomic_set(&gpemc->pin_rd_we_use_count, 0);
	atomic_set(&gpemc->pin_dqs_use_count, 0);

	spin_lock_init(&gpemc->addr_pins_pool.lock);

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

static int gpemc_request_gpio_generic(const u32 *array,
		int count, enum gpio_function func,
		atomic_t *use_count, const char *name)
{
	int i;
	int ret = 0;
	char pin_name[30];

	if (atomic_read(use_count) == 0) {
		for (i = 0; i < count; i++) {
			sprintf(pin_name, "%s%d", name, i);
			ret = gpio_request(array[i], name);
			if (ret < 0) {
				i--;
				do {
					gpio_free(array[i]);
				} while (i--);

				pr_err("gpemc: failed to request pin %s\n", pin_name);
				goto err_return;
			}

			ret = jz_gpio_set_func(array[i], func);
			if (ret < 0) {
				do {
					gpio_free(array[i]);
				} while (i--);

				pr_err("gpemc: failed to set function for pin %s\n", pin_name);
				goto err_return;
			}
		}
	}

	atomic_inc(use_count);

err_return:
	return ret;
}

static void gpemc_release_gpio_generic(const u32 *array,
		int count, atomic_t *use_count)
{
	int i;

	atomic_dec(use_count);
	if (atomic_read(use_count) == 0) {
		for (i = 0; i < count; i++) {
			gpio_free(array[i]);
		}
	}
}

static int gpemc_request_gpio_data(void)
{
	return gpemc_request_gpio_generic(gpemc->gpio_pin_data,
			ARRAY_SIZE(gpemc->gpio_pin_data), JZ_GPIO_FUNC_MEM_DATA,
			&gpemc->pin_data_use_count, "gpemc-data");
}

static void gpemc_release_gpio_data(void)
{
	gpemc_release_gpio_generic(gpemc->gpio_pin_data,
			ARRAY_SIZE(gpemc->gpio_pin_data),
			&gpemc->pin_data_use_count);
}

static int gpemc_request_gpio_addr_one(int index, const char *name)
{
	int ret = 0;
	char pin_name[30];

	BUG_ON(index > ARRAY_SIZE(gpemc->addr_pins_pool.addr_pins) - 1);

	spin_lock(&gpemc->addr_pins_pool.lock);
	if (gpemc->addr_pins_pool.addr_pin_use_count[index] == 0) {
		spin_unlock(&gpemc->addr_pins_pool.lock);

		sprintf(pin_name, "%s%d", name, index);
		ret = gpio_request(gpemc->addr_pins_pool.addr_pins[index],
				name);
		if (ret) {
			pr_err("gpemc: failed to request pin: %s\n", pin_name);
			goto err_return;
		}

		ret = jz_gpio_set_func(gpemc->addr_pins_pool.addr_pins[index],
				JZ_GPIO_FUNC_MEM_ADDR);
		if (ret) {
			pr_err("gpemc: failed to set function for pin: %s\n", pin_name);
			gpio_free(gpemc->addr_pins_pool.addr_pins[index]);
			goto err_return;
		}

		spin_lock(&gpemc->addr_pins_pool.lock);
	}

	gpemc->addr_pins_pool.addr_pin_use_count[index]++;
	spin_unlock(&gpemc->addr_pins_pool.lock);

err_return:
	return ret;
}

static void gpemc_release_gpio_addr_one(int index)
{
	int use_count;

	BUG_ON(index > ARRAY_SIZE(gpemc->addr_pins_pool.addr_pins) - 1);

	spin_lock(&gpemc->addr_pins_pool.lock);
	use_count = --gpemc->addr_pins_pool.addr_pin_use_count[index];
	spin_unlock(&gpemc->addr_pins_pool.lock);

	if (use_count == 0)
		gpio_free(gpemc->addr_pins_pool.addr_pins[index]);
}

static int gpemc_request_gpio_addr(int cnt_addr_pins)
{
	int i;
	int ret = 0;

	for (i = 0; i < cnt_addr_pins; i++) {
		ret = gpemc_request_gpio_addr_one(i, "gpemc-addr");
		if (ret)
			goto err_return;
	}

err_return:
	return ret;
}

static void gpemc_release_gpio_addr(int cnt_addr_pins)
{
	int i;

	for (i = 0; i < cnt_addr_pins; i++)
		gpemc_release_gpio_addr_one(i);
}


static int gpemc_request_gpio_cle_ale(void)
{
	int ret;

	ret = gpemc_request_gpio_addr_one(PIN_INDEX_OF_CLE, "gpemc-cle");
	if (ret)
		goto err_return;

	ret = gpemc_request_gpio_addr_one(PIN_INDEX_OF_ALE, "gpemc-ale");
	if (ret)
		goto err_return;

err_return:
	return ret;
}

static void gpemc_release_gpio_cle_ale(void)
{
	gpemc_release_gpio_addr_one(PIN_INDEX_OF_CLE);
	gpemc_release_gpio_addr_one(PIN_INDEX_OF_ALE);
}

static int gpemc_request_gpio_rd_we(void)
{
	return gpemc_request_gpio_generic(gpemc->gpio_pin_rd_we,
			ARRAY_SIZE(gpemc->gpio_pin_rd_we), JZ_GPIO_FUNC_MEM_RD_WE,
			&gpemc->pin_rd_we_use_count, "gpemc-rd/we");
}

static void gpemc_release_gpio_rd_we(void)
{
	gpemc_release_gpio_generic(gpemc->gpio_pin_rd_we,
			ARRAY_SIZE(gpemc->gpio_pin_rd_we),
			&gpemc->pin_rd_we_use_count);
}

static int gpemc_request_gpio_cs(int cs)
{
	int ret;

	ret = gpio_request(gpemc->gpio_pin_cs[cs], "gpemc-cs");
	if (ret < 0) {
		pr_err("gpemc: failed to request pin cs%d\n", cs);
		goto err_return;
	}

	ret = jz_gpio_set_func(gpemc->gpio_pin_cs[cs], JZ_GPIO_FUNC_MEM_CS);
	if (ret < 0) {
		gpio_free(gpemc->gpio_pin_cs[cs]);
		pr_err("gpemc: failed to set function for pin cs%d\n", cs);
		goto err_return;
	}

err_return:
	return ret;
}

static void gpemc_release_gpio_cs(int cs)
{
	gpio_free(gpemc->gpio_pin_cs[cs]);
}

/*
 * TODO: request input wait#
 */
#if 0

static int gpemc_request_gpio_wait(void)
{
	return gpemc_request_gpio_generic(&gpemc->gpio_pin_wait,
			1, JZ_GPIO_FUNC_MEM_WAIT,
			&gpemc->pin_wait_use_count, "gpemc-wait");
}

static void gpemc_release_gpio_wait(void)
{
	gpemc_release_gpio_generic(&gpemc->gpio_pin_wait,
			1, &gpemc->pin_wait_use_count);
}

#endif

static int gpemc_request_gpio_dqs(void)
{
	return gpemc_request_gpio_generic(&gpemc->gpio_pin_dqs,
			1, JZ_GPIO_FUNC_DQS,
			&gpemc->pin_rd_we_use_count, "gpemc-dqs");
}

static void gpemc_release_gpio_dqs(void)
{
	gpemc_release_gpio_generic(&gpemc->gpio_pin_dqs,
			1, &gpemc->pin_dqs_use_count);
}

static int gpemc_request_gpio_for_sram(gpemc_bank_t *bank)
{

	/*
	 * need inout  DATA[7 : 0]
	 *      output ADDR[cnt : 0]
	 *      output WE#
	 *      output RD#
	 *      output CS#
	 *
	 * TODO: request input wait#
	 */

	int ret;

	ret = gpemc_request_gpio_data();
	if (ret)
		return ret;

	ret = gpemc_request_gpio_addr(bank->cnt_addr_pins);
	if (ret)
		goto err_release_data;

	ret = gpemc_request_gpio_rd_we();
	if (ret)
		goto err_release_addr;

	ret = gpemc_request_gpio_cs(bank->cs);
	if (ret)
		goto err_release_rd_we;

	return ret;

err_release_rd_we:
	gpemc_release_gpio_rd_we();

err_release_addr:
	gpemc_release_gpio_addr(bank->cnt_addr_pins);

err_release_data:
	gpemc_release_gpio_data();

	return ret;
}

static int gpemc_request_gpio_for_command_nand(gpemc_bank_t *bank)
{
	/*
	 * need inout  DATA[7 : 0]
	 *      output CLE, ALE
	 *      output WE#
	 *      output RD#
	 *      output CS#
	 */

	int ret;

	ret = gpemc_request_gpio_data();
	if (ret)
		return ret;

	ret = gpemc_request_gpio_cle_ale();
	if (ret)
		goto err_release_data;

	ret = gpemc_request_gpio_rd_we();
	if (ret)
		goto err_release_cle_ale;

	ret = gpemc_request_gpio_cs(bank->cs);
	if (ret)
		goto err_release_rd_we;

	return ret;

err_release_rd_we:
	gpemc_release_gpio_rd_we();

err_release_cle_ale:
	gpemc_release_gpio_cle_ale();

err_release_data:
	gpemc_release_gpio_data();

	return ret;
}

static int gpemc_request_gpio_for_toggle_nand(gpemc_bank_t *bank)
{
	/*
	 * need inout  DATA[7 : 0]
	 *      output CLE, ALE
	 *      output WE#
	 *      output RD#
	 *      output CS#
	 *      output DQS
	 */

	int ret;

	ret = gpemc_request_gpio_data();
	if (ret)
		return ret;

	ret = gpemc_request_gpio_cle_ale();
	if (ret)
		goto err_release_data;

	ret = gpemc_request_gpio_rd_we();
	if (ret)
		goto err_release_cle_ale;

	ret = gpemc_request_gpio_cs(bank->cs);
	if (ret)
		goto err_release_rd_we;

	ret = gpemc_request_gpio_dqs();
	if (ret)
		goto err_release_cs;

	return ret;

err_release_cs:
	gpemc_release_gpio_cs(bank->cs);

err_release_rd_we:
	gpemc_release_gpio_rd_we();

err_release_cle_ale:
	gpemc_release_gpio_cle_ale();

err_release_data:
	gpemc_release_gpio_data();

	return ret;
}

static void gpemc_release_gpio_for_sram(gpemc_bank_t *bank)
{
	gpemc_release_gpio_data();
	gpemc_release_gpio_rd_we();
	gpemc_release_gpio_addr(bank->cnt_addr_pins);
	gpemc_release_gpio_cs(bank->cs);
}

static void gpemc_release_gpio_for_command_nand(gpemc_bank_t *bank)
{
	gpemc_release_gpio_data();
	gpemc_release_gpio_rd_we();
	gpemc_release_gpio_cle_ale();
	gpemc_release_gpio_cs(bank->cs);
}

static void gpemc_release_gpio_for_toggle_nand(gpemc_bank_t *bank)
{
	gpemc_release_gpio_data();
	gpemc_release_gpio_rd_we();
	gpemc_release_gpio_cle_ale();
	gpemc_release_gpio_cs(bank->cs);
	gpemc_release_gpio_dqs();
}

static int gpemc_request_gpio(gpemc_bank_t *bank)
{
	int ret;

	switch (bank->bank_type) {
	case BANK_TYPE_SRAM:
		ret = gpemc_request_gpio_for_sram(bank);
		break;

	case BANK_TYPE_NAND:
		ret = gpemc_request_gpio_for_command_nand(bank);
		break;

	case BANK_TYPE_TOGGLE:
		ret = gpemc_request_gpio_for_toggle_nand(bank);
		break;
	}


	return ret;
}

static void gpemc_release_gpio(gpemc_bank_t *bank)
{
	switch (bank->bank_type) {
	case BANK_TYPE_SRAM:
		gpemc_release_gpio_for_sram(bank);
		break;

	case BANK_TYPE_NAND:
		gpemc_release_gpio_for_command_nand(bank);
		break;

	case BANK_TYPE_TOGGLE:
		gpemc_release_gpio_for_toggle_nand(bank);
		break;
	}
}

void gpemc_set_bank_as_common_nand(gpemc_bank_t *bank)
{
	u32 index;

	BUG_ON(bank->bank_type != BANK_TYPE_NAND);

	/* set nand type as common nand */
	index = 16 + bank->cs - 1;
	gpemc->nand_regs_file->nfcsr &= ~BIT(index);

	/* set bank role as nand */
	index = (bank->cs - 1) << 1;
	gpemc->nand_regs_file->nfcsr |= BIT(index);

	bank->bank_type = BANK_TYPE_NAND;
}
EXPORT_SYMBOL(gpemc_set_bank_as_common_nand);

void gpemc_set_bank_as_toggle_nand(gpemc_bank_t *bank)
{
	u32 index;

	BUG_ON(bank->bank_type != BANK_TYPE_TOGGLE);

	/* set nand type as toggle */
	index = 16 + bank->cs - 1;
	gpemc->nand_regs_file->nfcsr |= BIT(index);

	/* set bank role as nand */
	index = (bank->cs - 1) << 1;
	gpemc->nand_regs_file->nfcsr |= BIT(index);

	bank->bank_type = BANK_TYPE_TOGGLE;
}
EXPORT_SYMBOL(gpemc_set_bank_as_toggle_nand);

static void gpemc_set_bank_as_sram(gpemc_bank_t *bank)
{
	u32 index;

	BUG_ON(bank->bank_type != BANK_TYPE_SRAM);

	/* set bank role as sram */
	index = (bank->cs - 1) << 1;
	gpemc->nand_regs_file->nfcsr &= ~BIT(index);
}

static void gpemc_set_bank_role(gpemc_bank_t *bank)
{
	switch (bank->bank_type) {
	case BANK_TYPE_SRAM:
		gpemc_set_bank_as_sram(bank);
		break;

	case BANK_TYPE_NAND:
		gpemc_set_bank_as_common_nand(bank);
		break;

	case BANK_TYPE_TOGGLE:
		gpemc_set_bank_as_toggle_nand(bank);
		break;
	}
}

int gpemc_request_cs(gpemc_bank_t *bank, int cs)
{
	struct resource *res;
	int ret;

	BUG_ON(cs < 1 || cs >= ARRAY_SIZE(gpemc->bank_mem));
	BUG_ON(bank->bank_type < BANK_TYPE_SRAM
			|| bank->bank_type > BANK_TYPE_TOGGLE);

	bank->cs = cs;

	if (bank->bank_type == BANK_TYPE_SRAM)
		BUG_ON(bank->cnt_addr_pins == 0);

	if (test_bit(cs, gpemc->bank_use_map)) {
		pr_err("gpemc: grab cs %d failed, it's busy.\n", cs);
		goto err_busy_bank;
	}

	ret = gpemc_request_gpio(bank);
	if (ret)
		return ret;

	res = request_mem_region(gpemc->bank_mem[cs].start,
			resource_size(&gpemc->bank_mem[cs]), gpemc->bank_name[cs]);
	if (!res) {
		pr_err("gpemc: grab bank%d memory failed, it's busy.\n", cs);
		goto err_release_gpio;
	}

	set_bit(cs, gpemc->bank_use_map);

	bank->io_base = (void __iomem *)CKSEG1ADDR(gpemc->bank_mem[cs].start);
	bank->io_nand_dat = bank->io_base + GPEMC_NAND_BANK_DATA_OFFSET;
	bank->io_nand_addr = bank->io_base + GPEMC_NAND_BANK_ADDR_OFFSET;
	bank->io_nand_cmd = bank->io_base + GPEMC_NAND_BANK_CMD_OFFSET;

	gpemc_set_bank_role(bank);

	return 0;

err_release_gpio:
	gpemc_release_gpio(bank);

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

	gpemc_release_gpio(bank);

	clear_bit(bank->cs, gpemc->bank_use_map);
}
EXPORT_SYMBOL(gpemc_release_cs);

void gpemc_fill_timing_from_nand(gpemc_bank_t *bank,
		common_nand_timing_t *timing)
{
	u32 temp;

	/* bank Taw */
	bank->bank_timing.sram_timing.Taw = timing->Trp;

	/* bank Tas */
	temp = max(timing->Tals, timing->Tcls);
	temp = max(temp, timing->Tcs);
	temp -= bank->bank_timing.sram_timing.Taw;
	bank->bank_timing.sram_timing.Tas = temp;

	/* bank Tah */
	temp = max(timing->Talh, timing->Tclh);
	temp = max(temp, timing->Tch);
	temp = max(temp, timing->Tdh);
	bank->bank_timing.sram_timing.Tah = temp;

	/*
	 * bank Tstrv
	 *
	 * TODO: Twhr2 should be considered.
	 *
	 */
	bank->bank_timing.sram_timing.Tstrv = timing->Twhr;

	/* bank Tbp */
	temp = timing->Twp;
	temp = max(temp, (timing->Twc - timing->Twp));
	bank->bank_timing.sram_timing.Tbp = temp;

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

void gpemc_relax_bank_timing(gpemc_bank_t *bank)
{
	/* all sram timing relax */
	gpemc->regs_file->smcr[bank->cs] = ~(u32)0;

	/* BW=0, BL=4, Normal sram */
	gpemc->regs_file->smcr[bank->cs] &=
			~(0x3 << 6) | ~(0x3 << 1) | ~(0x1 << 0);

	/* TODO: all toggle nand timing relax  */
}
EXPORT_SYMBOL(gpemc_relax_bank_timing);

int gpemc_config_bank_timing(gpemc_bank_t *bank)
{
	u32 smcr, temp;
	u32 clk_T;

	/* T count in nanoseconds */
	clk_T = div_ceiling(1000 * 1000 * 1000, clk_get_rate(gpemc->clk));

	smcr = gpemc->regs_file->smcr[bank->cs];

	switch (bank->bank_type) {
	case BANK_TYPE_SRAM:
		/*
		 * TODO
		 */
		break;

	case BANK_TYPE_NAND:
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

		/* Tstrv */
		temp = div_ceiling(bank->bank_timing.sram_timing.Tstrv, clk_T);
		smcr &= ~(0x3f << 24);
		smcr |= temp << 24;

		/* Tbp */
		temp = div_ceiling(bank->bank_timing.sram_timing.Tbp, clk_T);
		temp = nT_to_adjs[temp];
		smcr &= ~(0xf << 16);
		smcr |= temp << 16;

		/* BW & BL */
		smcr &= ~(0x3 << 6) | ~(0x3 << 1);
		smcr |= bank->bank_timing.sram_timing.BW << 6;

		gpemc->regs_file->smcr[bank->cs] = smcr;

		break;

	case BANK_TYPE_TOGGLE:
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

MODULE_AUTHOR("Fighter Sun <wanmyqawdr@126.com>");
MODULE_DESCRIPTION("SoC-jz4780 GPEMC(NEMC) support functions");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:"DRVNAME);

