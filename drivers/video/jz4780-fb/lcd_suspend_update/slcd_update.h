#ifndef __SLCD_UPDATE_H__
#define __SLCD_UPDATE_H__


struct display_update_ops {
	int (*prepare)(struct fb_info * fb, void * ignore);
	int (*update_frame_buffer)(struct fb_info * fb, void * addr, unsigned int rtc_second);
	void (*finish)(void);
};


/* callbacks by customer module */
struct update_config_callback_ops {
	int (*set_refresh)(int enable);
	/* set rtc alarm wakeup period(in second). */
	int (*set_period)(int period);
};

/*
 * EXPORT_SYMBOL(display_update_set_ops);
 */
extern struct update_config_callback_ops * display_update_set_ops(const struct display_update_ops *ops);

extern int is_configed_slcd_rtc_alarm_refresh(void);

extern int update_clock(void);

extern int slcd_refresh_prepare(void);
extern int slcd_refresh_finish(void);
//extern void set_slcd_suspend_alarm_resume(int);

#endif /* __SLCD_UPDATE_H__ */
