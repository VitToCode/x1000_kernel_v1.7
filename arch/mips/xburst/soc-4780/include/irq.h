/*
 *  Copyright (C) 2010 Ingenic Semiconductor Inc.
 *
 *  Author: <zpzhong@ingenic.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_MACH_INGENIC_IRQ_H__
#define __ASM_MACH_INGENIC_IRQ_H__

/* IRQ for MIPS CPU */
#define MIPS_CPU_IRQ_BASE 	0
#define IRQ_SMP_IPI 		3

#define INTC_IRQ_BASE		8

enum {
#define INTC_NR_IRQS	64
	IRQ_INTC_BASE = INTC_IRQ_BASE,
	IRQ_INTC_END = IRQ_INTC_BASE + INTC_NR_IRQS,

#define DMAC_NR_IRQS	32
	IRQ_PMAC_BASE,
	IRQ_PMAC_END = IRQ_PMAC_BASE + DMAC_NR_IRQS,

#define GPIO_NR_IRQS	(32*6)
	IRQ_GPIO_BASE,
	IRQ_GPIO_END = IRQ_GPIO_BASE + GPIO_NR_IRQS,

#define OST_NR_IRQS	3
	IRQ_OST_BASE,
	IRQ_OST_END = IRQ_OST_BASE + OST_NR_IRQS,
};

#define NR_IRQS	300

#endif
