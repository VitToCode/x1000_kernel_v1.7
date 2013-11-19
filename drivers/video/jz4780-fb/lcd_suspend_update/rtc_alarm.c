
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

#include "jz4780_fb.h"
#include "slcd_suspend_debug.h"


//static int slcd_refresh_period = 60; /* slcd refresh in every 60 seconds. */
static int slcd_refresh_period = 16; /* slcd refresh in every 60 seconds. */
static unsigned int last_slcd_refresh_alarm_value = 0;

static int old_rtc_alarm_enabled;
static unsigned int old_rtc_alarm_value;


/* copy from #include "drivers/rtc/rtc-jz4775.h" */
#include "rtc-jz4775.h"


/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */

/*
 * dump_rtc_regs
 */
int dump_rtc_regs(void)
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
int clear_rtc_alarm_flag(void)
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
int is_slcd_refresh_rtc_alarm_wakeup(void)
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
int reconfig_rtc_alarm(void)
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



void check_and_save_old_rtc_alarm(void)
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



int rtc_set_alarm_wakeup_period(int period)
{
	slcd_refresh_period = period;
	return 0;
}

#endif /* CONFIG_SLCD_SUSPEND_ALARM_WAKEUP_REFRESH */
