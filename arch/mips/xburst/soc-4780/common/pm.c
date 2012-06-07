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

#define CONFIG_PM_POWERDOWN_P0 1

#define jz_sys_readl(r)		(*((volatile unsigned long*)(r)))
#define jz_sys_writel(r,v)	(*((volatile unsigned long*)(r)) = (v))

#define	CPM_BASE	0xb0000000

#define CPM_LCR_OFFSET          (0x04)  /* rw, 32, 0x000000f8 */
#define CPM_PSWC0ST_OFFSET      (0x90)  /* rw, 32, 0x00000000 */
#define CPM_PSWC1ST_OFFSET      (0x94)  /* rw, 32, 0x00000000 */
#define CPM_PSWC2ST_OFFSET      (0x98)  /* rw, 32, 0x00000000 */
#define CPM_PSWC3ST_OFFSET      (0x9c)  /* rw, 32, 0x00000000 */
#define CPM_RSR_OFFSET          (0x08)  /* rw, 32, 0x???????? */
#define CPM_OPCR_OFFSET         (0x24)  /* rw, 32, 0x00001570 */
#define CPM_SRBC_OFFSET   	(0xc4)  /* rw, 32, 0x00000000 */
#define CPM_SLBC_OFFSET   	(0xc8)  /* rw, 32, 0x00000000 */
#define CPM_SLPC_OFFSET         (0xcc)  /* rw, 32, 0x???????? */
#define CPM_SLBC_OFFSET   	(0xc8)  /* rw, 32, 0x00000000 */
#define CPM_SLBC_OFFSET   	(0xc8)  /* rw, 32, 0x00000000 */
#define CPM_SPCR0_OFFSET   	(0xb8)  /* rw, 32, 0x00000000 */
#define CPM_SPCR1_OFFSET   	(0xbc)  /* rw, 32, 0x00000000 */

#define CPM_LCR		(CPM_BASE + CPM_LCR_OFFSET)
#define CPM_PSWC0ST	(CPM_BASE + CPM_PSWC0ST_OFFSET)
#define CPM_PSWC1ST	(CPM_BASE + CPM_PSWC1ST_OFFSET)
#define CPM_PSWC2ST	(CPM_BASE + CPM_PSWC2ST_OFFSET)
#define CPM_PSWC3ST	(CPM_BASE + CPM_PSWC3ST_OFFSET)
#define CPM_RSR		(CPM_BASE + CPM_RSR_OFFSET)
#define CPM_OPCR	(CPM_BASE + CPM_OPCR_OFFSET)
#define CPM_SRBC	(CPM_BASE + CPM_SRBC_OFFSET)
#define CPM_SLBC	(CPM_BASE + CPM_SLBC_OFFSET)
#define CPM_SLPC	(CPM_BASE + CPM_SLPC_OFFSET)
#define CPM_SPCR0	(CPM_BASE + CPM_SPCR0_OFFSET)
#define CPM_SPCR1	(CPM_BASE + CPM_SPCR1_OFFSET)

#define LCR_LPM_MASK		(0x3)
#define LCR_LPM_SLEEP		(0x1)

#define CPM_LCR_PD_SCPU		(0x1<<31)
#define CPM_LCR_PD_VPU		(0x1<<30)
#define CPM_LCR_PD_GPU		(0x1<<29)
#define CPM_LCR_PD_GPS		(0x1<<28)
#define CPM_LCR_PD_MASK		(0x7<<28)
#define CPM_LCR_SCPUS 		(0x1<<27)
#define CPM_LCR_VPUS		(0x1<<26)
#define CPM_LCR_GPUS		(0x1<<25) 
#define CPM_LCR_GPSS 		(0x1<<25)
#define CPM_LCR_STATUS_MASK 	(0xf<<25)
#define CPM_LCR_GPU_IDLE 	(0x1<<24)


#define OPCR_ERCS		(0x1<<2)
#define OPCR_PD			(0x1<<3)


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
#define CFG_DCACHE_SIZE		16384
#define CFG_ICACHE_SIZE		16384
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
	int i;
	unsigned long lcr = jz_sys_readl(CPM_LCR);
	unsigned long opcr = jz_sys_readl(CPM_OPCR);
	
	if(!(lcr & CPM_LCR_GPU_IDLE)) {
		printk("jz4780_pm_enter failed - GPU is busy.\n");
		goto jz4780_pm_enter_exit;
	}

	jz_sys_writel(CPM_LCR, (lcr & ~LCR_LPM_MASK) | LCR_LPM_SLEEP | CPM_LCR_PD_MASK);
	udelay(30);

	if((jz_sys_readl(CPM_LCR) & CPM_LCR_STATUS_MASK) != CPM_LCR_STATUS_MASK) {
		printk("jz4780_pm_enter failed - some module CAN NOT shutdown.\n");
		goto jz4780_pm_enter_exit;
	}

	/* set Oscillator Stabilize Time*/
	/* disable externel clock Oscillator in sleep mode */
	/* select 32K crystal as RTC clock in sleep mode */
#if defined(CONFIG_PM_POWERDOWN_P0)
	jz_sys_writel(CPM_OPCR, 1<<30 | 2<<26 | 0xff<<8 | OPCR_PD | OPCR_ERCS);
#else
	jz_sys_writel(CPM_OPCR, 1<<30 | 1<<26 | 0xff<<8 | OPCR_ERCS);
#endif

	/* shutdown memory power control */
	jz_sys_writel(CPM_SPCR0, 0xffffffff);
	/* Clear previous reset status */
	jz_sys_writel(CPM_RSR, 0);

	jz_sys_writel(CPM_PSWC0ST, 0);
	jz_sys_writel(CPM_PSWC1ST, 8);
	jz_sys_writel(CPM_PSWC2ST, 11);
	jz_sys_writel(CPM_PSWC3ST, 0);

	/* set SRBC to stop bus transfer */
	jz_sys_writel(CPM_SRBC, 0);
#if defined(CONFIG_PM_POWERDOWN_P0)
	/* Set resume return address */
	jz_sys_writel(CPM_SLBC, 1);
	jz_sys_writel(CPM_SLPC, (unsigned long)&&sleep_done);

	save_regs(regs);
#endif	
	__jz_flush_cache_all();
	__asm__ volatile(".set mips32\n\t"
			"sync\n\t"
			"wait\n\t"
			"nop\n\t"
			"nop\n\t"
			"nop\n\t"
			".set mips32");

#if defined(CONFIG_PM_POWERDOWN_P0)
sleep_done:
	__jz_cache_init();
	load_regs(regs);
#endif
	/* clear SRBC */
	jz_sys_writel(CPM_SRBC, 0);
	/* enable memory power control */
	for(i=0;i<32;i++) {
		jz_sys_writel(CPM_SPCR0, jz_sys_readl(CPM_SPCR0) & ~(0x1<<i));
		udelay(5);
	}
	/* Restore OPCR */
	jz_sys_writel(CPM_OPCR, opcr);
jz4780_pm_enter_exit:
	/* Restore LCR */
	jz_sys_writel(CPM_LCR, lcr);
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


#if 0
void jz_pm_hibernate(void)
{
	/* Set minimum wakeup_n pin low-level assertion time for wakeup: 100ms */
	rtc_write_reg(RTC_HWFCR, HWFCR_WAIT_TIME(100));

	/* Set reset pin low-level assertion time after wakeup: must  > 60ms */
	rtc_write_reg(RTC_HRCR, HRCR_WAIT_TIME(60));

	/* Scratch pad register to be reserved */
	rtc_write_reg(RTC_HSPR, HSPR_RTCV);

	/* clear wakeup status register */
	rtc_write_reg(RTC_HWRSR, 0x0);

	/* Put CPU to hibernate mode */
	rtc_write_reg(RTC_HCR, HCR_PD);

	mdelay(200):

		while (1) printk("We should NOT come here.\n");
}
#endif

