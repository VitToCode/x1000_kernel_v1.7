#ifndef __LOCAL_RTC_ALARM_H__
#define __LOCAL_RTC_ALARM_H__

extern int dump_rtc_regs(void);
extern int is_slcd_refresh_rtc_alarm_wakeup(void);
extern int reconfig_rtc_alarm(void);
extern void check_and_save_old_rtc_alarm(void);

extern int clear_rtc_alarm_flag(void);
extern int rtc_set_alarm_wakeup_period(int period);



#endif /* __LOCAL_RTC_ALARM_H__ */
