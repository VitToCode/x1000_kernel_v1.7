/*
 * Interrupt controller.
 *
 * Copyright (c) 2006-2007  Ingenic Semiconductor Inc.
 * Author: <lhhuang@ingenic.cn>
 *
 *  This program is free software; you can redistribute	 it and/or modify it
 *  under  the terms of	 the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the	License, or (at your
 *  option) any later version.
 */
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/bitops.h>

#include <asm/irq_cpu.h>

#include <soc/base.h>
#include <soc/irq.h>
#include <soc/ost.h>

#define PART_OFF	0x20

#define ISR_OFF		(0x00)
#define IMR_OFF		(0x04)
#define IMSR_OFF	(0x08)
#define IMCR_OFF	(0x0c)
#define IPR_OFF		(0x10)

#define regr(off) 	inl(OST_IOBASE + (off))
#define regw(val,off)	outl(val, OST_IOBASE + (off))

static void __iomem *intc_base;
static DECLARE_BITMAP(intc_wakeup, INTC_NR_IRQS);

static void intc_irq_ctrl(struct irq_data *data, int msk, int wkup)
{
	int intc = (int)irq_data_get_irq_chip_data(data);
	void *base = intc_base + PART_OFF * (intc/32);

	if (msk == 1)
		writel(BIT(intc%32), base + IMSR_OFF);
	else if (msk == 0)
		writel(BIT(intc%32), base + IMCR_OFF);

	if (wkup == 1)
		set_bit(intc, intc_wakeup);
	else if (wkup == 0)
		clear_bit(intc, intc_wakeup);
}

static void intc_irq_unmask(struct irq_data *data)
{
	intc_irq_ctrl(data, 0, -1);
}

static void intc_irq_mask(struct irq_data *data)
{
	intc_irq_ctrl(data, 1, -1);
}

static int intc_irq_set_wake(struct irq_data *data, unsigned int on)
{
	intc_irq_ctrl(data, -1, !!on);
	return 0;
}

static struct irq_chip jzintc_chip = {
	.name 		= "jz-intc",
	.irq_mask	= intc_irq_mask,
	.irq_mask_ack 	= intc_irq_mask,
	.irq_unmask 	= intc_irq_unmask,
	.irq_set_wake 	= intc_irq_set_wake,
};

#ifdef CONFIG_SMP
extern void jzsoc_mbox_interrupt(void);
static irqreturn_t ipi_interrupt(int irq, void *d)
{
	jzsoc_mbox_interrupt();
	return IRQ_HANDLED;
}
static int setup_ipi(void)
{
	set_c0_status(STATUSF_IP3);
	if (request_irq(IRQ_SMP_IPI, ipi_interrupt, IRQF_DISABLED,
			"SMP IPI", NULL))
		BUG();
	return 0;
}
#endif

static void ost_irq_unmask(struct irq_data *data)
{
	regw(0xffffffff,  OST_TMCR);
}

static void ost_irq_mask(struct irq_data *data)
{
	regw(0xffffffff,  OST_TMSR);
}

static void ost_irq_mask_ack(struct irq_data *data)
{
	regw(0xffffffff,  OST_TMSR);
	regw(0xffffffff,  OST_TFCR);  /* clear ost flag */
}

static struct irq_chip ost_irq_type = {
	.name 		= "ost",
	.irq_mask	= ost_irq_mask,
	.irq_mask_ack 	= ost_irq_mask_ack,
	.irq_unmask 	= ost_irq_unmask,
};

void __init arch_init_irq(void)
{
	int i;

	clear_c0_status(0xff04); /* clear ERL */
	set_c0_status(0x0400);   /* set IP2 */

	/* Set up MIPS CPU irq */
	mips_cpu_irq_init();

	/* Set up INTC irq */
	intc_base = ioremap(INTC_IOBASE, 0xfff);

	writel(0xffffffff, intc_base + IMSR_OFF);
	writel(0xffffffff, intc_base + PART_OFF + IMSR_OFF);
	for (i = IRQ_INTC_BASE; i < IRQ_INTC_BASE + INTC_NR_IRQS; i++) {
		irq_set_chip_data(i, (void *)(i - IRQ_INTC_BASE));
		irq_set_chip_and_handler(i, &jzintc_chip, handle_level_irq);
	}
	
	for (i = IRQ_OST_BASE; i < IRQ_OST_BASE + OST_NR_IRQS; i++) {
		irq_set_chip_data(i, (void *)(i - IRQ_OST_BASE));
		irq_set_chip_and_handler(i, &ost_irq_type, handle_level_irq);
	}
	
	/* enable cpu interrupt mask */
	set_c0_status(IE_IRQ0 | IE_IRQ1);

#ifdef CONFIG_SMP
	setup_ipi();
#endif	
	return;
}

static void intc_irq_dispatch(void)
{
	unsigned long ipr[2];

	do {
		ipr[0] = readl(intc_base + IPR_OFF);
		ipr[1] = readl(intc_base + PART_OFF + IPR_OFF);
		if (ipr[0]) {
			do_IRQ(ffs(ipr[0]) -1 +IRQ_INTC_BASE);
		}
		if (ipr[1]) {
			do_IRQ(ffs(ipr[1]) +31 +IRQ_INTC_BASE);
		}
	} while(ipr[0] || ipr[1]);

	return;
}

asmlinkage void plat_irq_dispatch(void)
{
	unsigned int cause = read_c0_cause();
	unsigned int pending = cause & read_c0_status() & ST0_IM;

	if (cause & CAUSEF_IP4) {
		do_IRQ(IRQ_OST); 
	} else if(pending & CAUSEF_IP2)
		intc_irq_dispatch();
#ifdef CONFIG_SMP
	else if(pending & CAUSEF_IP3)
		do_IRQ(IRQ_SMP_IPI); 
#endif

	return;
}

