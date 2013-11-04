#ifndef __SLCD_SUSPEND_DEBUG_H__
#define __SLCD_SUSPEND_DEBUG_H__


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


#endif /* __SLCD_SUSPEND_DEBUG_H__ */
