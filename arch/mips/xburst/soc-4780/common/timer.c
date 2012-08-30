/*
 * Setting up the system tick clock.
 *
 * Copyright (C) 2008 Ingenic Semiconductor Inc.
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 */
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/clockchips.h>

#include <soc/irq.h>
#include <soc/cpm.h>
#include <soc/base.h>
#include <soc/extal.h>
#include <soc/ost.h>


#define regr(off) 	inl(OST_IOBASE + (off))
#define regw(val,off)	outl(val, OST_IOBASE + (off))
#define SYS_TIMER_IRQ 	IRQ_OST
#define SYS_TIMER_CLK (JZ_EXTAL/4)
static const unsigned int latch = (SYS_TIMER_CLK + (HZ>>1)) / HZ;

union clycle_type
{
	cycle_t cycle64;
	unsigned long cycle32[2];
};

static cycle_t jz_get_cycles(struct clocksource *cs)
{
	union clycle_type cycle;

	do {
		cycle.cycle32[1] = regr(OST_CNTH);
		cycle.cycle32[0] = regr(OST_CNTL);
	} while(cycle.cycle32[1] != regr(OST_CNTH));
	return cycle.cycle64;
}

static struct clocksource clocksource_jz = {
	.name 		= "jz-clocksource",
	.rating		= 300,
	.read		= jz_get_cycles,
	.mask		= 0x7FFFFFFFFFFFFFFFULL,
	.shift 		= 10,
	.flags		= CLOCK_SOURCE_WATCHDOG,
};

unsigned long long sched_clock(void)
{
	return (jz_get_cycles(0) * clocksource_jz.mult) 
		>> clocksource_jz.shift;
}

static irqreturn_t timer_interrupt(int irq, void *data)
{
	struct clock_event_device *cd = data;
	unsigned long dr;

	regw(TFR_OSTF,  OST_TFCR);  /* clear ost flag */
	dr = regr(OST_DR);
	do {
		dr += latch;
	} while(dr <= regr(OST_CNTL));
	regw(dr, OST_DR);

	cd->event_handler(cd);
	return IRQ_HANDLED;
}

static int jz_set_next_event(unsigned long evt,
		struct clock_event_device *unused)
{
	return 0;
}

static void jz_set_mode(enum clock_event_mode mode,
		struct clock_event_device *evt)
{
	switch (mode) {
		case CLOCK_EVT_MODE_PERIODIC:
			break;
		case CLOCK_EVT_MODE_ONESHOT:
		case CLOCK_EVT_MODE_UNUSED:
		case CLOCK_EVT_MODE_SHUTDOWN:
			break;
		case CLOCK_EVT_MODE_RESUME:
			break;
	}
}

static struct clock_event_device jz_clockevent_device = {
	.name		= "jz-clockenvent",
	.features	= CLOCK_EVT_FEAT_PERIODIC,
	/* .mult, .shift, .max_delta_ns and .min_delta_ns left uninitialized */
	.mult		= 1,
	.rating		= 300,
	.irq		= SYS_TIMER_IRQ,
	.set_mode	= jz_set_mode,
	.set_next_event	= jz_set_next_event,
};

static void __init jz_sysclock_setup(void)
{
	unsigned int cpu = smp_processor_id();

	regw( 0, OST_CNTL);
	regw( 0, OST_CNTH);
	regw(latch, OST_DR);

	regw(TFR_OSTF,  OST_TFCR);  /* clear ost flag */
	regw(TMR_OSTM,  OST_TMCR);  /* unmask match irq */
	regw(TSR_OSTS,  OST_TSCR);  /* clear stop bit */


	regw(OSTCSR_CNT_MD, OST_CSR);
	regw(CSR_DIV4,OST_TCSR);
	regw(OST_EN,OST_TESR);


	/* init clock source */
	clocksource_jz.mult = 
		clocksource_hz2mult(SYS_TIMER_CLK, clocksource_jz.shift);
	clocksource_register(&clocksource_jz);

	/* init clock event */
	if (request_irq(SYS_TIMER_IRQ, timer_interrupt, 
				IRQF_DISABLED | IRQF_PERCPU | IRQF_TIMER,
				"sys tick", &jz_clockevent_device)) {
		BUG();
	}

	jz_clockevent_device.cpumask = cpumask_of(cpu);
	clockevents_register_device(&jz_clockevent_device);
}

#ifdef CONFIG_SMP
extern void percpu_timer_setup(void);
#endif

void __init plat_time_init(void)
{
#ifdef CONFIG_SMP
	percpu_timer_setup();
#endif
	jz_sysclock_setup();
}
