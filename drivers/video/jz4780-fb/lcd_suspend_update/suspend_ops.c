#ifdef CONFIG_SLCD_SUSPEND_ALARM_WAKEUP_REFRESH
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
#include <asm/fpu.h>
#include <linux/syscore_ops.h>
#include <linux/regulator/consumer.h>
#include <linux/vmalloc.h>
#include <linux/fb.h>

#include <soc/cache.h>
#include <soc/base.h>
#include <soc/cpm.h>
#include <soc/irq.h>
#include <tcsm.h>

#include "rtc_alarm.h"
#include "slcd_suspend_debug.h"
#include "slcd_update.h"


extern int jz4775_pm_enter(suspend_state_t state);
/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */


#define TRACE_IRQ        1
#define PART_OFF	0x20

#define ISR_OFF		(0x00)
#define IMR_OFF		(0x04)
#define IMSR_OFF	(0x08)
#define IMCR_OFF	(0x0c)
#define IPR_OFF		(0x10)
//extern void __iomem *intc_base;
#define intc_base ((void*)(INTC_IOBASE|0xB0000000))

static unsigned long intc_saved[2];
//static int func_is_open = 1;
//static unsigned long intc_wakeup[2];
//static unsigned long intc_read[2];

/* [   57.807941] 508 intc_saved[0]=0xfffc07ff 0xfffffffe */

static void save_intc_irqs(void)
{
	intc_saved[0] = readl(intc_base + IMR_OFF);
	intc_saved[1] = readl(intc_base + PART_OFF + IMR_OFF);
	printk_dbg("%d intc_[0]=%#x %#x\n", __LINE__, (int)intc_saved[0], (int)intc_saved[1]);
	return;
}

/* restore intc masks */
static void restore_intc_irqs(void)
{

	printk_dbg("%d writel intc_[0]=%#x %#x\n", __LINE__, (int)intc_saved[0], (int)intc_saved[1]);
	writel(0xffffffff & ~intc_saved[0], intc_base + IMCR_OFF);
	writel(0xffffffff & ~intc_saved[1], intc_base + PART_OFF + IMCR_OFF);
	return;
}

/* -------------------------------------------------------------------------------- */
#define REG_CPM_CGR *(unsigned int *)(0xB0000020) /* Clock Gate Register */
#define REG_CPM_LPCDR *(unsigned int *)(0xB0000064) /* LCD Pixel Clock Divider Register */


static void check_enable_lcdc_clocks(void)
{
	unsigned int lpcdr, cgr;
	/* [   65.391567] SLCD-REFRESH: REG_CPM_CGR=0x7fe8fff0, REG_CPM_LPCDR=0x14000024 */
	printk_dbg("%s() REG_CPM_CGR=%#x, REG_CPM_LPCDR=%#x\n", __FUNCTION__, REG_CPM_CGR, REG_CPM_LPCDR);

	cgr = REG_CPM_CGR;
	lpcdr = REG_CPM_LPCDR;

	/* CGR_BIT25: LCD Pixel Clock Gate */
	cgr &= ~(1<<25);	/* enable lcd pixel clock gate */
	REG_CPM_CGR = cgr;

	/* set LCD_CE */
	lpcdr |= (1<<28);
	REG_CPM_LPCDR = lpcdr;

	/* clear LCD STOP */
	lpcdr &= ~(1<<26);
	REG_CPM_LPCDR = lpcdr;

	/* wait lcd busy??? */

	printk_dbg("%s() REG_CPM_CGR=%#x, REG_CPM_LPCDR=%#x\n", __FUNCTION__, REG_CPM_CGR, REG_CPM_LPCDR);
	return;
}

static void disable_lcdc_clocks(void)
{
	unsigned int lpcdr, cgr;
	/* [   65.391567] SLCD-REFRESH: REG_CPM_CGR=0x7fe8fff0, REG_CPM_LPCDR=0x14000024 */
	printk_dbg("%s() REG_CPM_CGR=%#x, REG_CPM_LPCDR=%#x\n", __FUNCTION__, REG_CPM_CGR, REG_CPM_LPCDR);

	cgr = REG_CPM_CGR;
	lpcdr = REG_CPM_LPCDR;

	/* set LCD_CE */
	lpcdr |= (1<<28);
	REG_CPM_LPCDR = lpcdr;

	/* clear LCD STOP */
	lpcdr |= (1<<26);
	REG_CPM_LPCDR = lpcdr;

	/* wait lcd busy??? */

	/* CGR_BIT25: LCD Pixel Clock Gate */
	cgr |= (1<<25);	/* enable lcd pixel clock gate */
	REG_CPM_CGR = cgr;

	printk_dbg("%s() REG_CPM_CGR=%#x, REG_CPM_LPCDR=%#x\n", __FUNCTION__, REG_CPM_CGR, REG_CPM_LPCDR);
	return;
}


/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */


static int update_slcd(void * ignore)
{
	/* set lcd gpio function */
	printk_dbg("* set lcd gpio function??? *\n");

	/* check lcdc clocks */
	check_enable_lcdc_clocks();



	/* update slcd */
	printk_info("************************* update slcd, mdelay(10000) ******************** \n"); mdelay(3000);
	update_clock();

	/* check lcdc clocks */
	disable_lcdc_clocks();


	return 0;
}

static int jz4775_pm_enter_with_slcd_rtc_alarm_refresh_impl(suspend_state_t state)
{
	int return_val;


	/* [   57.807941] 508 intc_saved[0]=0xfffc07ff 0xfffffffe */
	save_intc_irqs();

	check_and_save_old_rtc_alarm();

suspend_again:

#ifdef DEBUG_TEST
	{
		static int suspend_test_count = 0;
		unsigned long intc_read[2];
		suspend_test_count ++; printk_dbg("suspend_test_count=%d\n", suspend_test_count);
		intc_read[0] = readl(intc_base + IMR_OFF);
		intc_read[1] = readl(intc_base + PART_OFF + IMR_OFF);
		printk_dbg("%d read intc [0]=%#x %#x\n", __LINE__, (int)intc_read[0], (int)intc_read[1]);
	}
#endif


#if 0
	if ( should_reconfig_slcd_rtc_alarm_wakeup() ) {
		reconfig_rtc_alarm();
	}
	else {
		/* restore old rtc alarm value */
		restore_old_rtc_alarm();
	}
#else
	reconfig_rtc_alarm();
#endif

	/* dump_rtc_regs for DEBUG_TEST */
	dump_rtc_regs();

	printk_info("jz4775_pm_enter(state)\n");
	return_val = jz4775_pm_enter(state);

	/* alarm wakeup, key wakeup??? */
	if ( is_slcd_refresh_rtc_alarm_wakeup()) {
		/* clear alarm flag? */
		clear_rtc_alarm_flag();

		update_slcd(NULL);

		/* continue suspend */

		/* restore intc masks */
		restore_intc_irqs();

		printk_dbg("goto suspend_again,\n");
		//printk_dbg("goto suspend_again, msleep(10000)\n"); msleep(10000); // failed, do not use schedule.
		//printk_dbg("goto suspend_again, mdelay(3000)\n"); mdelay(3000);
		goto suspend_again;
	}

	printk_dbg("jz4775_pm_enter_internal() return_val=%d\n", return_val);
	return return_val;
}

static int jz4775_pm_enter_with_slcd_rtc_alarm_refresh(suspend_state_t state)
{
	int return_val;
	if (is_configed_slcd_rtc_alarm_refresh()) {
		return_val = jz4775_pm_enter_with_slcd_rtc_alarm_refresh_impl(state);
	}
	else {
		return_val = jz4775_pm_enter(state);
	}
	return return_val;
}


static int jz4775_suspend_begin(suspend_state_t state)
{
	printk_dbg("%s ENTER\n", __FUNCTION__);
	if (is_configed_slcd_rtc_alarm_refresh())
		slcd_refresh_prepare();
	return 0;
}

static int jz4775_suspend_prepare(void)
{
	printk_dbg("%s ENTER\n", __FUNCTION__);
	return 0;
}

static void jz4775_suspend_finish(void)
{
	printk_dbg("%s ENTER\n", __FUNCTION__);

	return ;
}

static void jz4775_suspend_end(void)
{
	printk_dbg("%s ENTER\n", __FUNCTION__);
	if (is_configed_slcd_rtc_alarm_refresh())
		slcd_refresh_finish();
	return ;
}
#if 0
void set_slcd_suspend_alarm_resume(int data){
	func_is_open = data;
	printk("++++++func_is_open++++++is %d  \n",func_is_open);
}
#endif
//EXPORT_SYMBOL(set_slcd_suspend_alarm_resume);

/* called by kernel/arch/mips/xburst/soc-4775/common/pm_p0.c
 * kernel/power/suspend.c:suspend_enter()
 */
struct platform_suspend_ops pm_ops = {
	.begin = jz4775_suspend_begin,
	.prepare = jz4775_suspend_prepare,
	.valid = suspend_valid_only_mem,
	.enter = jz4775_pm_enter_with_slcd_rtc_alarm_refresh,
	.finish = jz4775_suspend_finish,
	.end = jz4775_suspend_end,
};

#endif	/* CONFIG_SLCD_SUSPEND_ALARM_WAKEUP_REFRESH */
