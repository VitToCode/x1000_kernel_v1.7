#ifndef __WAKEUP_AND_UPDATE_DISPLAY_H__
#define __WAKEUP_AND_UPDATE_DISPLAY_H__


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

struct pic_arg {
	int pic_count;
	char *pic_buf;
	char *format;
};

enum watch_ops {
	WATCH_OPEN = 1,
	WATCH_PIC_PATH,
	WATCH_PERIOD,
	WATCH_CLOSE,
};

extern int is_configed_slcd_rtc_alarm_refresh(void);

extern int update_clock(void);

extern int slcd_refresh_prepare(void);
extern int slcd_refresh_finish(void);



#endif /* __WAKEUP_AND_UPDATE_DISPLAY_H__ */
