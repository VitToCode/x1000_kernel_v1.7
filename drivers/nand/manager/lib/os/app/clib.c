/**
 * clib.c
 **/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>

#include "os/clib.h"

unsigned int nm_sleep(unsigned int seconds)
{
	return sleep(seconds);
}

long long nd_getcurrentsec_ns(void)
{
	long long ns = -1LL;
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ns = (long long)ts.tv_sec * 1000000000L	+ (long long)ts.tv_nsec;
	return ns;
}

unsigned int nd_get_timestamp(void) {
	long long ns = -1LL;
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
	ns = (long long)ts.tv_sec * 1000000000L	+ (long long)ts.tv_nsec;
	return ns / 1000000L;
}
