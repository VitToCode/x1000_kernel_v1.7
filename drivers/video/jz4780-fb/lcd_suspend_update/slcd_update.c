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
#include "slcd_update.h"
#include "rtc_alarm.h"


/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */

//extern struct jzfb *jzfb0;
extern struct fb_info *suspend_fb;
extern void *suspend_base;

struct display_update_ops *g_update_ops = NULL;
static int g_alarm_refresh_enabled = 0;


int update_clock(void)
{
	unsigned int rtc_second;
	printk_dbg("%s() ENTER\n", __FUNCTION__);

	rtc_second = 0;

	printk_dbg("%s() suspend_fb=%p, suspend_base=%p, rtc_second=%d\n", __FUNCTION__, suspend_fb, suspend_base, rtc_second);

	/* copy next_buffer_count_buffer to lcd frame buffer */
	if (g_update_ops &&  g_update_ops->update_frame_buffer) {
		void * fbaddr;
		//fbaddr = suspend_base;
		fbaddr = suspend_fb->screen_base;

		g_update_ops->update_frame_buffer(suspend_fb, fbaddr, rtc_second);
	}

	/* update_slcd_frame_buffer */
	//update_slcd_frame_buffer();

	printk("mdelay(3000);\n"); mdelay(3000);

	return 0;
}

/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */

/*
 * set rtc alarm wakeup period(in second).
 */
static int set_period(int period)
{
	printk_dbg("%s(%d)\n", __FUNCTION__, period);
	return rtc_set_alarm_wakeup_period(period);

}


static int set_slcd_rtc_alarm_refresh(int enable)
{
	printk_dbg("%s(%d)\n", __FUNCTION__, enable);

	g_alarm_refresh_enabled = enable;

	return 0;
}

int is_configed_slcd_rtc_alarm_refresh(void)
{
	return g_alarm_refresh_enabled;
}


int slcd_refresh_prepare(void)
{
	printk_dbg("%s() ENTER\n", __FUNCTION__);

	/*  */
	if (g_update_ops &&  g_update_ops->prepare) {
		int err;
		err = g_update_ops->prepare(suspend_fb, NULL);
		if ( err ) {
			printk_info("g_update_ops->prepare() err=%d\n", err);
		}
	}

	return 0;
}


int slcd_refresh_finish(void)
{
	printk_dbg("%s() ENTER\n", __FUNCTION__);


	/*  */
	if (g_update_ops &&  g_update_ops->finish)
		g_update_ops->finish();


	return 0;
}



static struct update_config_callback_ops g_config_callback = {
	.set_refresh = set_slcd_rtc_alarm_refresh,
	.set_period = set_period
};

struct update_config_callback_ops * display_update_set_ops(const struct display_update_ops *ops)
{
	g_update_ops = (struct display_update_ops *)ops;

	return &g_config_callback;
}
EXPORT_SYMBOL(display_update_set_ops);

#endif	/* CONFIG_SLCD_SUSPEND_ALARM_WAKEUP_REFRESH */
