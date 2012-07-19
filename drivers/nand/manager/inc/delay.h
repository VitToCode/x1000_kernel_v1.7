#ifndef __NAND_DELAY_H_
#define __NAND_DELAY_H_

#ifndef LINUX_KERNEL

#include <sleep.h>

#else

#include <linux/delay.h>

static inline void sleep(unsigned int secs) {
	msleep(secs * 1000);
}

#endif

#endif 

