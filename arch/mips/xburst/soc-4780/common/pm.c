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
#include <asm/rjzcache.h>
#include <linux/syscore_ops.h>
#include <linux/regulator/consumer.h>

#include <soc/cache.h>
#include <soc/base.h>
#include <soc/cpm.h>

void (*__reset_dll)(void);

struct jz47xx_pm {
	int volt;
	unsigned int cpccr;
	struct regulator *vcore;
	struct syscore_ops vcore_pm_ops;
	struct platform_suspend_ops pm_ops;
};

static struct jz47xx_pm jz47xx_pm;


void inline reset_dll(void)
{
#define DELAY 0x1ff
	register int i = DELAY;
	*(volatile unsigned *)  0xB3010008 |= 0x1<<17;
	__jz_flush_cache_all();

	__asm__ volatile(".set mips32\n\t"
			"sync\n\t"
			"sync\n\t"
			"lw $zero,0(%0)\n\t"
			"nop\n\t"
			"nop\n\t"
			"nop\n\t"
			"nop\n\t"
			"nop\n\t"
			"nop\n\t"
			"nop\n\t"
			"nop\n\t"
			"nop\n\t"
			"nop\n\t"
			"nop\n\t"
			"nop\n\t"
			"nop\n\t"
			"nop\n\t"
			"wait\n\t"
			"nop\n\t"
			"nop\n\t"
			".set mips32" : : "r"(0xa0000000));	
	*(volatile unsigned *) 0xb00000d0 = 0x3;
	i = *(volatile unsigned *) 0xb00000d0;
        i = DELAY/10;
	while(i--)
		__asm__ volatile(".set mips32\n\t"
			"nop\n\t"
			".set mips32");
	*(volatile unsigned *) 0xb00000d0 = 0x1;
	i = *(volatile unsigned *) 0xb00000d0;
	i=DELAY;
	while(i--)
	__asm__ volatile(".set mips32\n\t"
			"nop\n\t"
			".set mips32");
	*(volatile unsigned *)  0xB3010008 &= ~(0x1<<17);
	__jz_cache_init();

}

#define ENABLE_LCR_MODULES(m) 					\
	do{							\
		unsigned long tmp = cpm_inl(CPM_LCR);		\
		if(tmp &  (1 << ((m) + 28))) {			\
			cpm_outl(tmp & ~(1<<(28+(m))),CPM_LCR);	\
			while(cpm_inl(CPM_LCR) & (1<<(24+ (m))));	\
			udelay(500);					\
		}							\
	}while(0)

#define DISABLE_LCR_MODULES(m)						\
	do{								\
		register unsigned long tmp = cpm_inl(CPM_LCR);		\
		if(!(tmp & (1 << ((m) + 28)))) {			\
			cpm_outl(tmp | (1<<(28+(m))),CPM_LCR);		\
			while(!(cpm_inl(CPM_LCR) & (1<<(24+ (m))))) ;	\
			printk("iiiiiiiiii:%d\n",m);			\
		}							\
	}while(0)

#define SAVE_SIZE   512
static unsigned int save_tcsm[SAVE_SIZE / 4];
static int jz4780_pm_enter(suspend_state_t state)
{
#ifndef CONFIG_FPGA_TEST 
	unsigned long opcr = cpm_inl(CPM_OPCR);

	DISABLE_LCR_MODULES(0);
	DISABLE_LCR_MODULES(1);
	DISABLE_LCR_MODULES(2);
	DISABLE_LCR_MODULES(3);
	cpm_outl(cpm_inl(CPM_LCR) | LCR_LPM_SLEEP,CPM_LCR);
	__reset_dll = (void (*)(void))0xb3425800;
	memcpy(save_tcsm,__reset_dll,SAVE_SIZE);
	memcpy(__reset_dll, reset_dll,SAVE_SIZE);

	/* disable externel clock Oscillator in sleep mode */
	/* select 32K crystal as RTC clock in sleep mode */
	/* select 32K crystal as RTC clock in sleep mode */
	opcr |= 0xff << 8 | (2 << 26);
	opcr |= 1 << 2;
	opcr &= ~(1 << 4);
	opcr &= ~(1 << 3);

	cpm_outl(opcr,CPM_OPCR);
	/* Clear previous reset status */
	cpm_outl(0,CPM_RSR);
        __reset_dll();
	cpm_outl(cpm_inl(CPM_LCR) & ~(LCR_LPM_MASK),CPM_LCR);
	memcpy(__reset_dll,save_tcsm,SAVE_SIZE);
#endif
	return 0;
}

int vcore_suspend(void)
{
	if(!jz47xx_pm.vcore)
		jz47xx_pm.vcore = regulator_get(NULL,"vcore");

	jz47xx_pm.cpccr = cpm_inl(CPM_CPCCR);
	cpm_outl((jz47xx_pm.cpccr & ~0xff) | 0x73 | (0x1<<22),CPM_CPCCR);

	if(jz47xx_pm.vcore) {
		jz47xx_pm.volt = regulator_get_voltage(jz47xx_pm.vcore);
		regulator_set_voltage(jz47xx_pm.vcore, 1025000, 1025000);
	}

	return 0;
}

void vcore_resume(void)
{
	if(!jz47xx_pm.vcore)
		jz47xx_pm.vcore = regulator_get(NULL,"vcore");

	if(jz47xx_pm.vcore && jz47xx_pm.volt) {
		regulator_set_voltage(jz47xx_pm.vcore, jz47xx_pm.volt, jz47xx_pm.volt);
		jz47xx_pm.volt = 0;
	}

	cpm_outl(jz47xx_pm.cpccr,CPM_CPCCR);
}

/*
 * Initialize power interface
 */

int __init jz4780_pm_init(void)
{
	jz47xx_pm.vcore_pm_ops.suspend = vcore_suspend;
	jz47xx_pm.vcore_pm_ops.resume = vcore_resume;
	register_syscore_ops(&jz47xx_pm.vcore_pm_ops);

	jz47xx_pm.pm_ops.valid = suspend_valid_only_mem,
	jz47xx_pm.pm_ops.enter = jz4780_pm_enter,
	suspend_set_ops(&jz47xx_pm.pm_ops);
	return 0;
}

arch_initcall(jz4780_pm_init);

