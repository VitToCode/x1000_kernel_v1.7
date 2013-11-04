#ifndef __SLCD_ALARM_WAKEUP_REFRESH_H__
#define __SLCD_ALARM_WAKEUP_REFRESH_H__


struct clock_bitmap_buffer {
	int valid;
	int hour;
	int minute;
	char * buffer;
};


struct clock_buffers {
	int buffer_count;
	int next_buffer_count;
	int buffer_size;

	struct clock_bitmap_buffer * bitmap_buffers;
};


extern int is_configed_slcd_rtc_alarm_refresh(void);

extern int update_clock(void);

extern int slcd_refresh_prepare(void);
extern int slcd_refresh_finish(void);



#endif /* __SLCD_ALARM_WAKEUP_REFRESH_H__ */
