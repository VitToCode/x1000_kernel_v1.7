/*
 * linux/arch/mips/jz4780/pm.c
 *
 *  JZ4780 Power Management Routines
 *  Copyright (C) 2006 - 2012 Ingenic Semiconductor Inc.
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

#include <linux/init.h>
#include <linux/pm.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/sysctl.h>
#include <linux/delay.h>
#include <asm/cacheops.h>

#include <soc/cache.h>
#include <soc/base.h>
#include <soc/cpm.h>

#define K0BASE			KSEG0
#define CFG_DCACHE_SIZE		32768
#define CFG_ICACHE_SIZE		32768
#define CFG_CACHELINE_SIZE	32

static inline void __jz_flush_cache_all(void)
{
	register unsigned long addr;

	/* Clear CP0 TagLo */
	asm volatile ("mtc0 $0, $28\n\t"::);

	for (addr = K0BASE; addr < (K0BASE + CFG_DCACHE_SIZE); addr += CFG_CACHELINE_SIZE) {
		asm volatile (".set mips32\n\t"
				" cache %0, 0(%1)\n\t"
				".set mips32\n\t"
				:
				: "I" (Index_Writeback_Inv_D), "r"(addr));
	}

	for (addr = K0BASE; addr < (K0BASE + CFG_ICACHE_SIZE); addr += CFG_CACHELINE_SIZE) {
		asm volatile (".set mips32\n\t"
				" cache %0, 0(%1)\n\t"
				".set mips32\n\t"
				:
				: "I" (Index_Store_Tag_I), "r"(addr));
	}

	/* invalidate BTB */
	asm volatile (
			".set mips32\n\t"
			" mfc0 $k0, $16, 7\n\t"
			" nop\n\t"
			" ori $k0, 2\n\t"
			" mtc0 $k0, $16, 7\n\t"
			" nop\n\t"
			".set mips32\n\t"
		     );
}

static inline void __jz_cache_init(void)
{
	register unsigned long addr;
	asm volatile ("mtc0 $0, $28\n\t"::);
	for (addr = K0BASE; addr < (K0BASE + CFG_DCACHE_SIZE); addr += CFG_CACHELINE_SIZE) {
		asm volatile (".set mips32\n\t"
				" cache %0, 0(%1)\n\t"
				".set mips32\n\t"
				:
				: "I" (Index_Store_Tag_D), "r"(addr));
	}

	for (addr = K0BASE; addr < (K0BASE + CFG_ICACHE_SIZE); addr += CFG_CACHELINE_SIZE) {
		asm volatile (".set mips32\n\t"
				" cache %0, 0(%1)\n\t"
				".set mips32\n\t"
				:
				: "I" (Index_Store_Tag_I), "r"(addr));
	}
	/* invalidate BTB */
	asm volatile (
			".set mips32\n\t"
			" mfc0 $k0, $16, 7\n\t"
			" nop\n\t"
			" ori $k0, 2\n\t"
			" mtc0 $k0, $16, 7\n\t"
			" nop\n\t"
			".set mips32\n\t"
		     );
}

static int jz4780_pm_enter(suspend_state_t state)
{
#ifndef CONFIG_FPGA_TEST 
	unsigned long lcr = cpm_inl(CPM_LCR);
	unsigned long opcr = cpm_inl(CPM_OPCR);
	unsigned long cpccr = cpm_inl(CPM_CPCCR);
	
	cpm_outl((lcr & ~LCR_LPM_MASK) | LCR_LPM_SLEEP | CPM_LCR_PD_MASK,CPM_LCR);
	while((cpm_inl(CPM_LCR) & CPM_LCR_STATUS_MASK) != CPM_LCR_STATUS_MASK);

	/* set Oscillator Stabilize Time*/
	/* disable externel clock Oscillator in sleep mode */
	/* select 32K crystal as RTC clock in sleep mode */
	cpm_outl(1<<30 | 1<<26 | 0xff<<8 | OPCR_ERCS,CPM_OPCR);
	/* Clear previous reset status */
	cpm_outl(0,CPM_RSR);

	cpm_outl(0,CPM_PSWC0ST);
	cpm_outl(8,CPM_PSWC1ST);
	cpm_outl(11,CPM_PSWC2ST);
	cpm_outl(0,CPM_PSWC3ST);

	cpm_outl((cpccr & ~0xff) | 0x31,CPM_CPCCR);
	__jz_flush_cache_all();

	cache_prefetch(do_sleep,sleep_end);
do_sleep:
	__asm__ volatile(".set mips32\n\t"
			"sync\n\t"
			"wait\n\t"
			"nop\n\t"
			"nop\n\t"
			"nop\n\t"
			".set mips32");
sleep_end:
	/* Restore OPCR */
	cpm_outl(opcr,CPM_OPCR);
	cpm_outl(cpccr,CPM_CPCCR);

	/* Restore LCR */
#define ENABLE_LCR_MODULES(m) 					\
	do{							\
		unsigned long tmp = cpm_inl(CPM_LCR);		\
		cpm_outl(tmp & ~(1<<(28+(m))),CPM_LCR);		\
		while(!(cpm_inl(CPM_LCR) & (1<<(24+ (m)))));	\
		udelay(500);					\
	}while(0)

#if 0
	ENABLE_LCR_MODULES(0);
	ENABLE_LCR_MODULES(1);
	ENABLE_LCR_MODULES(2);
#endif
	cpm_outl(cpm_inl(CPM_LCR) & ~0xff,CPM_LCR);
#undef ENABLE_LCR_MODULES
#endif
	return 0;
}

static struct platform_suspend_ops jz4780_pm_ops = {
	.valid		= suspend_valid_only_mem,
	.enter		= jz4780_pm_enter,
};

/*
 * Initialize power interface
 */
int __init jz4780_pm_init(void)
{
	suspend_set_ops(&jz4780_pm_ops);
	return 0;
}

arch_initcall(jz4780_pm_init);

