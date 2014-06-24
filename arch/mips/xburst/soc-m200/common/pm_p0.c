/*
 * linux/arch/mips/jz4785/pm.c
 *
 *  JZ4785 Power Management Routines
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
//#include <asm/mach-xburst/cacheops.h>
//#include <asm/mach-xburst/rjzcache.h>
#include <asm/fpu.h>
#include <linux/syscore_ops.h>
#include <linux/regulator/consumer.h>

#include <asm/cacheops.h>
#include <soc/cache.h>
#include <asm/r4kcache.h>
#include <soc/base.h>
#include <soc/cpm.h>
#include <tcsm.h>
extern long long save_goto(unsigned int);
extern int restore_goto(void);

#define get_cp0_ebase()	__read_32bit_c0_register($15, 1)

#define REG32(x) *(volatile unsigned int *)(x)

#define DDRP_PIR_INIT		(1 << 0)
#define DDRP_PIR_DLLSRST	(1 << 1)
#define DDRP_PIR_DLLLOCK	(1 << 2)
#define DDRP_PIR_ZCAL   	(1 << 3)
#define DDRP_PIR_ITMSRST   	(1 << 4)
#define DDRP_PIR_DRAMRST   	(1 << 5)
#define DDRP_PIR_DRAMINT   	(1 << 6)
#define DDRP_PIR_QSTRN   	(1 << 7)
#define DDRP_PIR_EYETRN   	(1 << 8)
#define DDRP_PIR_DLLBYP   	(1 << 17)
#define DDRP_PGSR_IDONE		(1 << 0)
#define DDRP_PGSR_DLDONE	(1 << 1)
#define DDRP_PGSR_ZCDONE	(1 << 2)
#define DDRP_PGSR_DIDONE	(1 << 3)
#define DDRP_PGSR_DTDONE	(1 << 4)
#define DDRP_PGSR_DTERR 	(1 << 5)
#define DDRP_PGSR_DTIERR 	(1 << 6)
#define DDRP_PGSR_DFTEERR 	(1 << 7)


#define DDRC_BASE	0xb34f0000
#define DDR_PHY_OFFSET	(-0x4e0000 + 0x1000)

#define DDRP_PIR	(DDR_PHY_OFFSET + 0x4) /* PHY Initialization Register */
#define DDRP_PGSR	(DDR_PHY_OFFSET + 0xc) /* PHY General Status Register*/
#define DDRP_DX0GSR     (DDR_PHY_OFFSET + 0x71 * 4)

#define DDRP_DXnDQSTR(n)     (DDR_PHY_OFFSET + (0x10 * n + 0x75) * 4)
#define DDRP_DXnDQTR(n)      (DDR_PHY_OFFSET + (0x10 * n + 0x74) * 4)

#define ddr_writel(value, reg)	REG32(DDRC_BASE + reg) = (value)
#define ddr_readl(reg)		REG32(DDRC_BASE + reg)


#define OFF_TDR         (0x00)
#define OFF_LCR         (0x0C)
#define OFF_LSR         (0x14)

#define LSR_TDRQ        (1 << 5)
#define LSR_TEMT        (1 << 6)

#define U1_IOBASE (UART1_IOBASE + 0xa0000000)
#define TCSM_PCHAR(x)							\
	*((volatile unsigned int*)(U1_IOBASE+OFF_TDR)) = x;		\
	while ((*((volatile unsigned int*)(U1_IOBASE + OFF_LSR)) & (LSR_TDRQ | LSR_TEMT)) != (LSR_TDRQ | LSR_TEMT))

#define TCSM_DELAY(x) \
	i=x;	\
	while(i--)	\
	__asm__ volatile(".set mips32\n\t"\
			"nop\n\t"\
			".set mips32")


static inline void serial_put_hex(unsigned int x) {
	int i;
	unsigned int d;
	for(i = 7;i >= 0;i--) {
		d = (x  >> (i * 4)) & 0xf;
		if(d < 10) d += '0';
		else d += 'A' - 10;
		TCSM_PCHAR(d);
	}
}
#ifdef DDR_MEM_TEST
unsigned int *test_mem;
static unsigned int test_mem_space[0x100000 / 4];
static inline void test_ddr_data_init(void) {
	int i;
	test_mem = test_mem_space;
	test_mem = (unsigned int *)((unsigned int)test_mem | 0xa0000000);
	dma_cache_wback_inv((unsigned int)test_mem_space,0x100000);
	for(i = 0;i < 0x100000 / 4;i++) {
		test_mem[i] = (unsigned int)&test_mem[i];
	}
}
static inline void check_ddr_data(void) {
	int i;
	for(i = 0;i < 0x100000 / 4;i++) {
		unsigned int dd;
		dd = test_mem[i];
		if(dd != (unsigned int)&test_mem[i]) {
			serial_put_hex(dd);
			TCSM_PCHAR(' ');
			serial_put_hex(i);
			TCSM_PCHAR(' ');
			serial_put_hex((unsigned int)&test_mem[i]);
			TCSM_PCHAR('\r');
			TCSM_PCHAR('\n');
		}
	}
}
#endif
static inline void dump_ddr_param(void) {
	int i;
	for(i = 0;i < 4;i++) {
		TCSM_PCHAR('<');
		serial_put_hex(i);
		TCSM_PCHAR('>');
		TCSM_PCHAR('<');
		serial_put_hex(ddr_readl(DDRP_DXnDQSTR(i)));
		TCSM_PCHAR('>');
		serial_put_hex(ddr_readl(DDRP_DXnDQTR(i)));
		TCSM_PCHAR('\r');
		TCSM_PCHAR('\n');
	}
	TCSM_PCHAR('i');
	serial_put_hex(ddr_readl(DDRP_PGSR));
	TCSM_PCHAR('\r');
	TCSM_PCHAR('\n');

	TCSM_PCHAR('i');
	serial_put_hex(ddr_readl(DDRP_PGSR));
	TCSM_PCHAR('\r');
	TCSM_PCHAR('\n');
}
extern void dump_clk(void);
struct save_reg
{
	unsigned int addr;
	unsigned int value;
};
#define SUSPEND_SAVE_REG_SIZE 10
static struct save_reg m_save_reg[SUSPEND_SAVE_REG_SIZE];
static int m_save_reg_count = 0;
static unsigned int read_save_reg_add(unsigned int addr)
{
	unsigned int value = REG32(CKSEG1ADDR(addr));
	if(m_save_reg_count < SUSPEND_SAVE_REG_SIZE) {
		m_save_reg[m_save_reg_count].addr = addr;
		m_save_reg[m_save_reg_count].value = value;
		m_save_reg_count++;
	}else
		printk("suspend_reg buffer size too small!\n");
	return value;
}
static void restore_all_reg(void)
{
	int i;
	for(i = 0;i < m_save_reg_count;i++) {
		REG32(CKSEG1ADDR(m_save_reg[i].addr)) = m_save_reg[i].value;
	}
	m_save_reg_count = 0;
}

extern unsigned int _regs_stack[];

static inline void config_powerdown_core(unsigned int *resume_pc) {
	unsigned int cpu_no,opcr;
	/* set SLBC and SLPC */
	cpm_outl(1,CPM_SLBC);
	/* Clear previous reset status */
	cpm_outl(0,CPM_RSR);
	/* OPCR.PD and OPCR.L2C_PD */
	cpu_no = get_cp0_ebase() & 1;
	opcr = cpm_inl(CPM_OPCR);
	//p 0 or 1 powerdown
	opcr &= ~(3<<25);
	opcr |= (cpu_no + 1) << 25;
	cpm_outl(opcr,CPM_OPCR);

	printk("opcr = %x\n",cpm_inl(CPM_OPCR));
	printk("lcr = %x\n",cpm_inl(CPM_LCR));

	// set resume pc
	cpm_outl((unsigned int)resume_pc,CPM_SLPC);
	blast_dcache32();
	blast_icache32();
	blast_scache32();
}
static noinline void cpu_sleep(void)
{
	config_powerdown_core((unsigned int *)0xb3422000);
	__asm__ volatile(".set mips32\n\t"
			 "sync\n\t"
			 "lw $0,0(%0)\n\t"
			 "nop\n\t"
			 "nop\n\t"
			 "nop\n\t"
			 ".set mips32 \n\t"
			 :
			 : "r" (0xa0000000) );
	printk("sleep!\n");
	//*(volatile unsigned int *)0xb00000b8 = (0xffffffff) & (~((1 << 17) | (1 << 15) | (1 << 31) | (1 << 2)));
	cache_prefetch(LABLE1,LABLE2);
LABLE1:
	__asm__ volatile(".set mips32\n\t"
			 "wait\n\t"
			 "nop\n\t"
			 "nop\n\t"
			 "nop\n\t"
			 ".set mips32 \n\t");

	/* { */
	/* 	void (*f)(void); */
	/* 	f = (void (*)(void))cpm_inl(CPM_SLPC); */
	/* 	f(); */
	/* } */
LABLE2:
	while(1)
		TCSM_PCHAR('n');

}

static noinline void cpu_resume(void)
{
	int retrycount = 0;
RETRY_LABLE:
	ddr_writel(DDRP_PIR_INIT |  DDRP_PIR_DLLSRST | DDRP_PIR_DLLLOCK | DDRP_PIR_ZCAL | DDRP_PIR_ITMSRST , DDRP_PIR);
	while((ddr_readl(DDRP_DX0GSR) & 0x3) != 3)
	while (ddr_readl(DDRP_PGSR) != (DDRP_PGSR_IDONE | DDRP_PGSR_DLDONE
					 | DDRP_PGSR_ZCDONE | DDRP_PGSR_DIDONE | DDRP_PGSR_DTDONE)) {

		if(ddr_readl(DDRP_PGSR) & (DDRP_PGSR_DTERR | DDRP_PGSR_DTIERR)) {

			ddr_writel(1 << 28,DDRP_PIR);
			while((ddr_readl(DDRP_DX0GSR) & 0x3) != 0)
				TCSM_PCHAR('1');
			retrycount++;
			serial_put_hex(retrycount);
			TCSM_PCHAR('\r');
			TCSM_PCHAR('\n');
			goto RETRY_LABLE;
			//	goto retry_lable;
		}

	}
	dump_ddr_param();
#ifdef DDR_MEM_TEST
	check_ddr_data();
#endif
	write_c0_ecc(0x0);
	__jz_cache_init();
	__asm__ volatile(".set mips32\n\t"
			 "jr %0\n\t"
			 "nop\n\t"
			 ".set mips32 \n\t" :: "r" (restore_goto));

}
static void load_func_to_tcsm(unsigned int *tcsm_addr,unsigned int *f_addr,unsigned int size)
{
	unsigned int instr;
	int offset;
	int i;
	for(i = 0;i < size / 4;i++) {
		instr = f_addr[i];
		if((instr >> 26) == 2){
			offset = instr & 0x3ffffff;
			offset = (offset << 2) - ((unsigned int)f_addr & 0xfffffff);
			if(offset > 0) {
				offset = ((unsigned int)tcsm_addr & 0xfffffff) + offset;
				instr = (2 << 26) | (offset >> 2);
			}
		}
		tcsm_addr[i] = instr;
	}
}
static int jz4785_pm_enter(suspend_state_t state)
{

	unsigned int  lcr_tmp;
	unsigned int  opcr_tmp;
	unsigned int gate,spcr0;

	disable_fpu();//FIXME by wli
#ifdef DDR_MEM_TEST
	test_ddr_data_init();
#endif
	load_func_to_tcsm((unsigned int *)0xb3422000,(unsigned int *)cpu_resume,4096);

	lcr_tmp = read_save_reg_add(CPM_IOBASE + CPM_LCR);
	lcr_tmp &= ~3;
	lcr_tmp |= LCR_LPM_SLEEP;
	cpm_outl(lcr_tmp,CPM_LCR);
	/* OPCR.MASK_INT bit30*/
	/* set Oscillator Stabilize Time bit8*/
	/* disable externel clock Oscillator in sleep mode bit4*/
	/* select 32K crystal as RTC clock in sleep mode bit2*/
        opcr_tmp = read_save_reg_add(CPM_IOBASE + CPM_OPCR);
	opcr_tmp &= ~((1 << 7) | (1 << 6) | (1 << 4));
	opcr_tmp |= (0xff << 8) | (1<<30) | (1 << 2) | (1 << 27) | (1 << 23);
        cpm_outl(opcr_tmp,CPM_OPCR);
	/*
	 * set sram pdma_ds & open nfi
	 */
	spcr0 = read_save_reg_add(CPM_IOBASE + CPM_SPCR0);
	spcr0 |= (1 << 31);
	spcr0 &= ~((1 << 27) | (1 << 2) | (1 << 15) | (1 << 31));
	cpm_outl(spcr0,CPM_SPCR0);

	/*
	 * set clk gate nfi nemc enable pdma
	 */
	gate = read_save_reg_add(CPM_IOBASE + CPM_CLKGR);
	gate &= ~(3  | (1 << 21));
	cpm_outl(gate,CPM_CLKGR);
	mb();
	save_goto((unsigned int)cpu_sleep);
	mb();
	restore_all_reg();
	return 0;
}

/*
 * Initialize power interface
 */
struct platform_suspend_ops pm_ops = {
	.valid = suspend_valid_only_mem,
	.enter = jz4785_pm_enter,
};
//extern void ddr_retention_exit(void);
//extern void ddr_retention_entry(void);

int __init jz4785_pm_init(void)
{
        volatile unsigned int lcr,opcr;//,i;
	suspend_set_ops(&pm_ops);
/* changed by wli */
        /* init opcr and lcr for idle */
        lcr = cpm_inl(CPM_LCR);
        lcr &= ~(0x7);		/* LCR.SLEEP.DS=1'b0,LCR.LPM=2'b00*/
        lcr |= 0xff << 8;	/* power stable time */
        cpm_outl(lcr,CPM_LCR);

        opcr = cpm_inl(CPM_OPCR);
        opcr &= ~(2 << 25);	/* OPCR.PD=2'b00 */
        opcr |= 0xff << 8;	/* EXCLK stable time */
        cpm_outl(opcr,CPM_OPCR);
        /* sysfs */
	return 0;
}

arch_initcall(jz4785_pm_init);
