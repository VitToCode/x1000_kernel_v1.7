#ifndef _CLIB_H_
#define _CLIB_H_

#include <linux/delay.h>
#include <linux/sched.h>

static inline unsigned int nm_sleep(unsigned int seconds)
{
	msleep(seconds * 1000);
	return 0;
}

static inline long long nd_getcurrentsec_ns(void)
{
	return sched_clock();   
}

#endif
