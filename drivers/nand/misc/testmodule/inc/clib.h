#ifndef _CLIB_H_
#define _CLIB_H_

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <sys/time.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>

static inline unsigned int nm_sleep(unsigned int seconds)
{
	return sleep(seconds);
}

static inline long long nd_getcurrentsec_ns(void)
{
	long long ns = -1LL;
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ns = (long long)ts.tv_sec * 1000000000L	+ (long long)ts.tv_nsec;
	return ns;   
}

#endif
