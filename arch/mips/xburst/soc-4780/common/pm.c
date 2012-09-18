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

#include <soc/cache.h>
#include <soc/base.h>
#include <soc/cpm.h>

int regs[128];

#define save_regs(base)		\
	__asm__ __volatile__ (	\
			".set    noat		\n\t"\
			"la	$26,%0		\n\t"\
			"mfhi	$27		\n\t"\
			"sw	$0,0($26)	\n\t"\
			"sw	$1,4($26)	\n\t"\
			"sw	$27,120($26)	\n\t"\
			"mflo	$27		\n\t"\
			"sw	$2,8($26)	\n\t"\
			"sw	$3,12($26)	\n\t"\
			"sw	$27,124($26)	\n\t"\
			"sw	$4,16($26)	\n\t"\
			"sw	$5,20($26)	\n\t"\
			"sw	$6,24($26)	\n\t"\
			"sw	$7,28($26)	\n\t"\
			"sw	$8,32($26)	\n\t"\
			"sw	$9,36($26)	\n\t"\
			"sw	$10,40($26)	\n\t"\
			"sw	$11,44($26)	\n\t"\
			"sw	$12,48($26)	\n\t"\
			"sw	$13,52($26)	\n\t"\
"sw	$14,56($26)	\n\t"\
"sw	$15,60($26)	\n\t"\
"sw	$16,64($26)	\n\t"\
"sw	$17,68($26)	\n\t"\
"sw	$18,72($26)	\n\t"\
"sw	$19,76($26)	\n\t"\
"sw	$20,80($26)	\n\t"\
"sw	$21,84($26)	\n\t"\
"sw	$22,88($26)	\n\t"\
"sw	$23,92($26)	\n\t"\
"sw	$24,96($26)	\n\t"\
"sw	$25,100($26)	\n\t"\
"sw	$28,104($26)	\n\t"\
"sw	$29,108($26)	\n\t"\
"sw	$30,112($26)	\n\t"\
"sw	$31,116($26)	\n\t"\
"mfc0	$1, $0    	\n\t"\
"mfc0	$2, $1    	\n\t"\
"mfc0	$3, $2    	\n\t"\
"mfc0	$4, $3    	\n\t"\
"mfc0	$5, $4    	\n\t"\
"mfc0	$6, $5    	\n\t"\
"mfc0	$7, $6    	\n\t"\
"mfc0	$8, $8    	\n\t"\
"mfc0	$9, $10   	\n\t"\
"mfc0	$10,$12   	\n\t"\
"mfc0	$11, $12,1	\n\t"\
"mfc0	$12, $13 	\n\t"\
"mfc0	$13, $14    	\n\t"\
"mfc0	$14, $15    	\n\t"\
"mfc0	$15, $15,1    	\n\t"\
"mfc0	$16, $16    	\n\t"\
"mfc0	$17, $16,1    	\n\t"\
"mfc0	$18, $16,2    	\n\t"\
"mfc0	$19, $16,3    	\n\t"\
"mfc0	$20, $16, 7    	\n\t"\
"mfc0	$21, $17    	\n\t"\
"sw	$1,  128($26)    \n\t"\
"sw	$2,  132($26)    \n\t"\
"sw	$3,  136($26)    \n\t"\
"sw	$4,  140($26)    \n\t"\
"sw	$5,  144($26)    \n\t"\
"sw	$6,  148($26)    \n\t"\
"sw	$7,  152($26)    \n\t"\
"sw	$8,  156($26)    \n\t"\
"sw	$9,  160($26)    \n\t"\
"sw	$10, 164($26)    \n\t"\
"sw	$11, 168($26)    \n\t"\
"sw	$12, 172($26)    \n\t"\
"sw	$13, 176($26)    \n\t"\
"sw	$14, 180($26)    \n\t"\
"sw	$15, 184($26)    \n\t"\
"sw	$16, 188($26)    \n\t"\
"sw	$17, 192($26)    \n\t"\
"sw	$18, 196($26)    \n\t"\
"sw	$19, 200($26)    \n\t"\
"sw	$20, 204($26)    \n\t"\
"sw	$21, 208($26)    \n\t"\
"mfc0	$1, $18    	\n\t"\
"mfc0	$2, $19    	\n\t"\
"mfc0	$3, $23    	\n\t"\
"mfc0	$4, $24    	\n\t"\
"mfc0	$5, $26    	\n\t"\
"mfc0	$6, $28		\n\t"\
"mfc0	$7, $28,1	\n\t"\
"mfc0	$8, $30		\n\t"\
"mfc0	$9, $31		\n\t"\
"mfc0	$10,$5,4 	\n\t"\
"sw	$1,  212($26)	\n\t"\
"sw	$2,  216($26)	\n\t"\
"sw	$3,  220($26)	\n\t"\
"sw	$4,  224($26)	\n\t"\
"sw	$5,  228($26)	\n\t"\
"sw	$6,  232($26)	\n\t"\
"sw	$7,  236($26)	\n\t"\
"sw	$8,  240($26)	\n\t"\
"sw	$9,  244($26)	\n\t"\
"sw	$10, 248($26)	\n\t"\
: : "i" (base))

#define load_regs(base)\
	__asm__ __volatile__ (\
			".set    noat		\n\t"\
			"la	$26,%0		\n\t"\
			"lw	$1,  128($26)	\n\t"\
			"lw	$2,  132($26)	\n\t"\
			"lw	$3,  136($26)	\n\t"\
			"lw	$4,  140($26)	\n\t"\
			"lw	$5,  144($26)	\n\t"\
			"lw	$6,  148($26)	\n\t"\
			"lw	$7,  152($26)	\n\t"\
			"lw	$8,  156($26)	\n\t"\
			"lw	$9,  160($26)	\n\t"\
			"lw	$10, 164($26)	\n\t"\
			"lw	$11, 168($26)	\n\t"\
			"lw	$12, 172($26)	\n\t"\
			"lw	$13, 176($26)	\n\t"\
			"lw	$14, 180($26)	\n\t"\
			"lw	$15, 184($26)	\n\t"\
			"lw	$16, 188($26)	\n\t"\
			"lw	$17, 192($26)	\n\t"\
			"lw	$18, 196($26)	\n\t"\
"lw	$19, 200($26)	\n\t"\
"lw	$20, 204($26)	\n\t"\
"lw	$21, 208($26)	\n\t"\
"mtc0	$1, $0		\n\t"\
"mtc0	$2, $1		\n\t"\
"mtc0	$3, $2		\n\t"\
"mtc0	$4, $3		\n\t"\
"mtc0	$5, $4		\n\t"\
"mtc0	$6, $5		\n\t"\
"mtc0	$7, $6		\n\t"\
"mtc0	$8, $8		\n\t"\
"mtc0	$9, $10		\n\t"\
"mtc0	$10,$12		\n\t"\
"mtc0	$11, $12,1	\n\t"\
"mtc0	$12, $13	\n\t"\
"mtc0	$13, $14	\n\t"\
"mtc0	$14, $15	\n\t"\
"mtc0	$15, $15,1	\n\t"\
"mtc0	$16, $16	\n\t"\
"mtc0	$17, $16,1	\n\t"\
"mtc0	$18, $16,2	\n\t"\
"mtc0	$19, $16,3	\n\t"\
"mtc0	$20, $16,7	\n\t"\
"mtc0	$21, $17	\n\t"\
"lw	$1,  212($26)	\n\t"\
"lw	$2,  216($26)	\n\t"\
"lw	$3,  220($26)	\n\t"\
"lw	$4,  224($26)	\n\t"\
"lw	$5,  228($26)	\n\t"\
"lw	$6,  232($26)	\n\t"\
"lw	$7,  236($26)	\n\t"\
"lw	$8,  240($26)	\n\t"\
"lw	$9,  244($26)	\n\t"\
"lw	$10, 248($26)	\n\t"\
"mtc0	$1, $18		\n\t"\
"mtc0	$2, $19		\n\t"\
"mtc0	$3, $23		\n\t"\
"mtc0	$4, $24		\n\t"\
"mtc0	$5, $26		\n\t"\
"mtc0	$6, $28		\n\t"\
"mtc0	$7, $28,1	\n\t"\
"mtc0	$8, $30		\n\t"\
"mtc0	$9, $31		\n\t"\
"mtc0	$10,$5,4	\n\t"\
"lw	$27,	120($26)\n\t"\
"lw	$0,	0($26)	\n\t"\
"lw	$1,  	4($26)	\n\t"\
"mthi	$27		\n\t"\
"lw	$27,	124($26)\n\t"\
"lw	$2,	8($26)	\n\t"\
"lw	$3,  	12($26)	\n\t"\
"mtlo	$27		\n\t"\
"lw	$4,  	16($26)	\n\t"\
"lw	$5,  	20($26)	\n\t"\
"lw	$6,  	24($26)	\n\t"\
"lw	$7,  	28($26)	\n\t"\
"lw	$8,  	32($26)	\n\t"\
"lw	$9,  	36($26)	\n\t"\
"lw	$10, 	40($26)	\n\t"\
"lw	$11, 	44($26)	\n\t"\
"lw	$12, 	48($26)	\n\t"\
"lw	$13, 	52($26)	\n\t"\
"lw	$14, 	56($26)	\n\t"\
"lw	$15, 	60($26)	\n\t"\
"lw	$16, 	64($26)	\n\t"\
"lw	$17, 	68($26)	\n\t"\
"lw	$18, 	72($26)	\n\t"\
"lw	$19, 	76($26)	\n\t"\
"lw	$20, 	80($26)	\n\t"\
"lw	$21, 	84($26)	\n\t"\
"lw	$22, 	88($26)	\n\t"\
"lw	$23, 	92($26)	\n\t"\
"lw	$24, 	96($26)	\n\t"\
"lw	$25, 	100($26)\n\t"\
"lw	$28, 	104($26)\n\t"\
"lw	$29, 	108($26)\n\t"\
"lw	$30, 	112($26)\n\t"\
"lw	$31, 	116($26)\n\t"\
: : "i" (base))

#define K0BASE			KSEG0
#define CFG_DCACHE_SIZE		32768
#define CFG_ICACHE_SIZE		32768
#define CFG_CACHELINE_SIZE	32

static inline void __jz_flush_cache_all(void)
{
	register unsigned long addr;
	/* Clear CP0 TagLo */
	asm volatile ("mtc0 $0, $28\n\t");
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
	asm volatile ("sync\n\t"
		      "lw $0,0(%0)"
		      ::"r" (0xa0000000));

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
	asm volatile (  ".set mips32\n\t"
			" mfc0 $k0, $16, 7\n\t"
			" nop\n\t"
			" ori $k0, 2\n\t"
			" mtc0 $k0, $16, 7\n\t"
			" nop\n\t"
			"nop\n\t"
			".set mips32\n\t"
		    );
}

void (*__reset_dll)(void);
//void reset_dll(void)
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

