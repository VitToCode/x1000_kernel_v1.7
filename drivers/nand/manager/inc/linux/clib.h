#ifndef _CLIB_H_
#define _CLIB_H_

#include <linux/delay.h>

static inline unsigned int nm_sleep(unsigned int seconds)
{
	msleep(seconds * 1000);
	return 0;
}

#endif
