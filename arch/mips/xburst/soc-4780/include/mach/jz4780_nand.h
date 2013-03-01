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

#ifndef __MACH_JZ4780_NAND_H__
#define __MACH_JZ4780_NAND_H__

#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <soc/gpemc.h>

typedef enum {
	/* NAND_XFER_<Data path driver>_<R/B# indicator> */
	NAND_XFER_CPU_IRQ = 0,
	NAND_XFER_CPU_POLL,
	NAND_XFER_DMA_IRQ,
	NAND_XFER_DMA_POLL
} nand_xfer_type_t;

typedef struct {
	int bank;
	bank_type_t type;

	int busy_gpio;
	int busy_gpio_low_assert;

	int wp_gpio;                      /* -1 if does not exist */
	int wp_gpio_low_assert;
} nand_interface_t;

typedef struct {
	const char *name;
	int nand_dev_id;

	common_nand_timing_t *common_nand_timing;
	toggle_nand_timing_t *toggle_nand_timing;
} nand_chip_info_t;

struct jz4780_nand_platform_data {
	struct mtd_partition *part_table; /* MTD partitions array */
	int num_part;                     /* number of partitions  */

	nand_interface_t *nand_interface_table;
	int num_nand_interface;

    /* not NULL if override default timings */
	nand_chip_info_t *nand_chip_info_table;

	nand_xfer_type_t xfer_type;  /* transfer type */
};

#endif
