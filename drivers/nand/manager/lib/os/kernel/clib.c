#include <linux/delay.h>
#include <linux/sched.h>

#include "os/clib.h"

unsigned int nm_sleep(unsigned int seconds)
{
	msleep(seconds * 1000);
	return 0;
}

long long nd_getcurrentsec_ns(void)
{
	return sched_clock();
}

unsigned int nd_get_timestamp(void)
{
	return jiffies_to_msecs(jiffies);
}
