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
#define INTC_NR_IRQS	48
	IRQ_INTC_BASE = INTC_IRQ_BASE,
	IRQ_INTC_END = IRQ_INTC_BASE + INTC_NR_IRQS,

#define DMAC_NR_IRQS	16
	IRQ_DMAC_BASE,
	IRQ_DMAC_END = IRQ_DMAC_BASE + DMAC_NR_IRQS,

#define BDMA_NR_IRQS	6
	IRQ_BDMA_BASE,
	IRQ_BDMA_END = IRQ_BDMA_BASE + BDMA_NR_IRQS,

#define GPIO_NR_IRQS	(32*6)
	IRQ_GPIO_BASE,
	IRQ_GPIO_END = IRQ_GPIO_BASE + GPIO_NR_IRQS,
};

#define NR_IRQS	384

#endif
