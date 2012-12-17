#ifndef __TIMER_H__
#define __TIMER_H__

#define SECTOR_SIZE 512


typedef struct _TimeByte TimeByte;
struct _TimeByte {
	int R_byte;
	int W_byte;
	long long SR_time;
	long long ER_time;
	long long SW_time;
	long long EW_time;
	int W_speed;
	int R_speed;
	unsigned int rcount;
	unsigned int wcount;
};

#endif
