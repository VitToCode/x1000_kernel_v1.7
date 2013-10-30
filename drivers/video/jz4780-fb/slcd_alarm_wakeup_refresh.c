
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

#include <soc/cache.h>
#include <soc/base.h>
#include <soc/cpm.h>
#include <soc/irq.h>
#include <tcsm.h>

#define DEBUG_TEST


/*
 * NOTE: in suspend, printk depend on CONFIG_SUSPEND_SUPREME_DEBUG.
 * select CONFIG_SUSPEND_SUPREME_DEBUG or
 * #define CONFIG_SUSPEND_SUPREME_DEBUG in pm_p0.c and gpio.c
 */

#ifdef DEBUG_TEST
#define printk_dbg(sss, aaa...)						\
	do {								\
		printk("SLCD-REFRESH: " sss,  ##aaa);			\
	} while (0)
#define printk_info(sss, aaa...)					\
	do {								\
		printk("SLCD-REFRESH: " sss,  ##aaa);			\
	} while (0)
#else
#define printk_dbg(sss, aaa...)						\
	do {								\
	} while (0)
#define printk_info(sss, aaa...)					\
	do {								\
		printk("SLCD-REFRESH: " sss,  ##aaa);			\
	} while (0)
#endif	/* DEBUG_TEST */



extern int jz4775_pm_enter(suspend_state_t state);



//static int slcd_refresh_period = 60; /* slcd refresh in every 60 seconds. */
static int slcd_refresh_period = 16; /* slcd refresh in every 60 seconds. */
static unsigned int last_slcd_refresh_alarm_value = 0;

static int old_rtc_alarm_enabled;
static unsigned int old_rtc_alarm_value;



/* copy from #include "drivers/rtc/rtc-jz4775.h" */
#include "rtc-jz4775.h"



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
static int is_configed_slcd_rtc_alarm_refresh(void)
{

	return 1;
}

/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */

/*
 * dump_rtc_regs
 */
static int dump_rtc_regs(void)
{
#ifdef DEBUG_TEST
	return 0;
#else
	unsigned int rtc_cr,rtc_sr, rtc_sar;

	rtc_cr = jzrtc_read_reg(RTC_RTCCR);
	rtc_sr = jzrtc_read_reg(RTC_RTCSR);
	rtc_sar = jzrtc_read_reg(RTC_RTCSAR);
	printk_dbg("****** %s() RTC_RTCCR=%#x, RTC_RTCSR=%#x, RTC_RTCSAR=%#x, SAR-SR=%d\n", __FUNCTION__, rtc_cr, rtc_sr, rtc_sar, (rtc_sar-rtc_sr));

	printk_dbg("****** %s() old_rtc_alarm_enabled=%d, old_rtc_alarm_value=%#x, last_slcd_refresh_alarm_value=%#x, slcd_refresh_period=%#x\n",
	       __FUNCTION__, old_rtc_alarm_enabled, old_rtc_alarm_value, last_slcd_refresh_alarm_value, slcd_refresh_period);
	return 0;
#endif
}


/*
 * check is it a rtc alarm wakeup
 */
static int is_rtc_alarm_wakeup(void)
{
	unsigned int rtc_cr,rtc_sr, rtc_sar;

	rtc_cr = jzrtc_read_reg(RTC_RTCCR);
	rtc_sr = jzrtc_read_reg(RTC_RTCSR);
	rtc_sar = jzrtc_read_reg(RTC_RTCSAR);
	//printk_dbg("%s() RTC_RTCCR=%#x, RTC_RTCSR=%#x, RTC_RTCSAR=%#x\n", __FUNCTION__, rtc_cr, rtc_sr, rtc_sar);

	if ( rtc_cr & RTCCR_AF) {
		return 1;
	}

	return 0;
}


/*
 * clear_rtc_alarm_flag
 */
static int clear_rtc_alarm_flag(void)
{
	unsigned int rtc_cr,rtc_sr, rtc_sar;

	rtc_cr = jzrtc_read_reg(RTC_RTCCR);
	rtc_sr = jzrtc_read_reg(RTC_RTCSR);
	rtc_sar = jzrtc_read_reg(RTC_RTCSAR);
	printk_dbg("%s() RTC_RTCCR=%#x, RTC_RTCSR=%#x, RTC_RTCSAR=%#x\n", __FUNCTION__, rtc_cr, rtc_sr, rtc_sar);

	rtc_cr &= ~(RTCCR_AF);	/* cleare alarm flag */


	return 0;
}


/*
 * sar: alarm second value
 */
static int set_rtc_alarm(int alarm_value)
{
	unsigned int rtc_cr,rtc_sr, rtc_sar;

	rtc_cr = jzrtc_read_reg(RTC_RTCCR);
	rtc_sr = jzrtc_read_reg(RTC_RTCSR);
	rtc_sar = jzrtc_read_reg(RTC_RTCSAR);
	printk_info("%s() RTCCR=%#x, SR=%#x, SAR=%#x, old_rtc_alarm_value=%#x, new SAR=%#x SAR-SR=%d\n", __FUNCTION__, rtc_cr, rtc_sr, rtc_sar, old_rtc_alarm_value, alarm_value, (alarm_value-rtc_sr));

	rtc_sar = alarm_value;
	jzrtc_write_reg(RTC_RTCSAR, rtc_sar);

	rtc_cr |= RTCCR_AE | RTCCR_AIE; /* enabel alarm and alarm interrupt */
	rtc_cr &= ~(RTCCR_AF);	/* cleare alarm flag */
	jzrtc_write_reg(RTC_RTCCR, rtc_cr);

	return 0;
}





/*
 * check is it a rtc alarm wakeup
 */
static int is_slcd_refresh_rtc_alarm_wakeup(void)
{
	unsigned int rtc_cr,rtc_sr, rtc_sar;
	int alarm_wakeup;

	rtc_cr = jzrtc_read_reg(RTC_RTCCR);
	rtc_sr = jzrtc_read_reg(RTC_RTCSR);
	rtc_sar = jzrtc_read_reg(RTC_RTCSAR);
	alarm_wakeup = is_rtc_alarm_wakeup();
	printk_dbg("%s() RTC_RTCCR=%#x, RTC_RTCSR=%#x, RTC_RTCSAR=%#x, last_slcd_refresh_alarm_value=%#x, old_rtc_alarm_value=%#x, alarm_wakeup=%d\n",
		   __FUNCTION__, rtc_cr, rtc_sr, rtc_sar, last_slcd_refresh_alarm_value, old_rtc_alarm_value, alarm_wakeup);

	/*
	 * what about old_rtc_alarm_value == next_sar ???
	 */
	if ( alarm_wakeup && (rtc_sar<=rtc_sr) && ( last_slcd_refresh_alarm_value == rtc_sar)
	     && (rtc_sar != old_rtc_alarm_value ) ) {
		return 1;
	}

	return 0;
}


/*
 * period: slcd refresh period, in second.
 */
static int reconfig_rtc_alarm(void)
{
	unsigned int rtc_sr, next_sar;

	rtc_sr = jzrtc_read_reg(RTC_RTCSR);
	//rtc_sar = rtc_sr + slcd_refresh_period;
	next_sar = rtc_sr + slcd_refresh_period;
	last_slcd_refresh_alarm_value  = next_sar;

	/* restore_old_rtc_alarm */
	if ( old_rtc_alarm_enabled ) {
		/* compair the original alarm second value with new slcd refresh alarm wakeup value
		 * old_rtc_alarm_value must not illegal.
		 * what about old_rtc_alarm_value == next_sar ???
		 */
		if ( old_rtc_alarm_value > rtc_sr && old_rtc_alarm_value < next_sar ) {
			next_sar = old_rtc_alarm_value;
		}
	}

	set_rtc_alarm(next_sar);

	return 0;
}



static void check_and_save_old_rtc_alarm(void)
{
	unsigned int rtc_cr,rtc_sr, rtc_sar;

	rtc_cr = jzrtc_read_reg(RTC_RTCCR);
	rtc_sr = jzrtc_read_reg(RTC_RTCSR);
	rtc_sar = jzrtc_read_reg(RTC_RTCSAR);
	printk_dbg("%s() RTC_RTCCR=%#x, RTC_RTCSR=%#x, RTC_RTCSAR=%#x\n", __FUNCTION__, rtc_cr, rtc_sr, rtc_sar);

	if (rtc_cr & RTCCR_AE) {
		old_rtc_alarm_enabled = 1;
	}
	else {
		old_rtc_alarm_enabled = 0;
	}

	old_rtc_alarm_value = rtc_sar;

	/* fix old_rtc_alarm_value */
	if ( old_rtc_alarm_enabled && (old_rtc_alarm_value <= rtc_sr)) {
		printk_dbg("warning %s() disable old alarm. old_rtc_alarm_enabled=%d, old_rtc_alarm_value=%#x < rtc_sr=%#x\n",
		       __FUNCTION__, old_rtc_alarm_enabled, old_rtc_alarm_value, rtc_sr);
		old_rtc_alarm_enabled = 0;
	}
	printk_dbg("************ %s() old_rtc_alarm_enabled=%d, old_rtc_alarm_value=%#x\n", __FUNCTION__, old_rtc_alarm_enabled, old_rtc_alarm_value);

	return;
}

#if 0
static void restore_old_rtc_alarm(void)
{
	unsigned int rtc_cr,rtc_sr, rtc_sar;

	rtc_cr = jzrtc_read_reg(RTC_RTCCR);
	rtc_sr = jzrtc_read_reg(RTC_RTCSR);
	rtc_sar = jzrtc_read_reg(RTC_RTCSAR);

	printk_dbg("%s() RTC_RTCCR=%#x, RTC_RTCSR=%#x, RTC_RTCSAR=%#x, old_rtc_alarm_enabled=%d, old_rtc_alarm_value=%#x\n", __FUNCTION__, rtc_cr, rtc_sr, rtc_sar, old_rtc_alarm_enabled, old_rtc_alarm_value);

	if (old_rtc_alarm_enabled) {
		//if () {
		//}
		set_rtc_alarm(old_rtc_alarm_value);
	}
	else {
		/* do not disable alarm wakeup!!! */
		//rtc_cr &= ~(RTCCR_AF | RTCCR_AIE| RTCCR_AE);	/* cleare alarm flag */
		//jzrtc_write_reg(RTC_RTCCR, rtc_cr);
	}

	return;
}
#endif
#if 0
static int should_reconfig_slcd_rtc_alarm_wakeup(void)
{
	unsigned int rtc_cr,rtc_sr, rtc_sar;
	int reconfig;

	reconfig=1;		/* default should reconfig */

	rtc_cr = jzrtc_read_reg(RTC_RTCCR);
	rtc_sr = jzrtc_read_reg(RTC_RTCSR);
	rtc_sar = jzrtc_read_reg(RTC_RTCSAR);
	printk_dbg("%s() RTC_RTCCR=%#x, RTC_RTCSR=%#x, RTC_RTCSAR=%#x\n", __FUNCTION__, rtc_cr, rtc_sr, rtc_sar);

	if ( old_rtc_alarm_enabled ) {
		/* compair the original alarm second value with new slcd refresh alarm wakeup value
		 * old_rtc_alarm_value must not illegal.
		 */
		unsigned int next_refresh;
		next_refresh = rtc_sr + slcd_refresh_period;
		if ( next_refresh > old_rtc_alarm_value && old_rtc_alarm_value > rtc_sr) {
			reconfig = 0;
		}
	}

	return reconfig;
}
#endif

static int update_slcd(void * ignore)
{
	/* set lcd gpio function */
	printk_dbg("* set lcd gpio function??? *\n");

	/* check lcdc clocks */
	check_enable_lcdc_clocks();



	/* update slcd */
	printk_info("************************* update slcd, mdelay(10000) ******************** \n"); mdelay(3000);
	//update_slcd_frame_buffer();

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

/* called by kernel/arch/mips/xburst/soc-4775/common/pm_p0.c */
int jz4775_pm_enter_with_slcd_rtc_alarm_refresh(suspend_state_t state)
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

#endif	/* CONFIG_SLCD_SUSPEND_ALARM_WAKEUP_REFRESH */
