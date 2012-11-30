#include <linux/delay.h>
#include "i88.h"

extern int suspend_get_pin(int pin);
extern void suspend_set_pin(int pin, int value);

void vibrator_on(int ms) 
{
    suspend_set_pin(GPIO_MOTOR_PIN, 1);
    mdelay(ms);
    suspend_set_pin(GPIO_MOTOR_PIN, 0);
}

enum wakeup_src {
    WAKEUP_WAKEUPKEY = 0,
    WAKEUP_USB = 1,
    WAKEUP_ALARM = 2,
    WAKEUP_UNKNOW = 3,
};

static inline bool is_wakeup_key_wake(int delay_ms)
{
    int filter = delay_ms;
    int retval = 0;
    int old_value = -1;

    do{
        mdelay(1);
        retval = suspend_get_pin(GPIO_ENDCALL);
        if(retval != old_value)
        {
            old_value = retval;
            filter = delay_ms;
            continue;
        }
    } while (filter--);

    return !retval;
}

static inline bool is_usb_wake()
{
    int filter = 10; 
    int retval = 0;
    int old_value = -1; 

    do{ 
        mdelay(1);
        retval = suspend_get_pin(GPIO_USB_DETE);
        if(retval != old_value)
        {   
            old_value = retval;
            filter = 10;
            continue;
        }   
    } while(filter--);

    return retval;
}

#define ALARM_WAVE_TIME 10      /* second */
static inline bool is_alarm_wake(void)
{
        unsigned int rtc_rtcsr = rtc_read_reg(RTC_RTCSR);
        unsigned int rtc_rtcsar = rtc_read_reg(RTC_RTCSAR);

        if (rtc_rtcsr >= rtc_rtcsar) {
                if (rtc_rtcsr - rtc_rtcsar < ALARM_WAVE_TIME)
                        return true;
        } else if (rtc_rtcsar - rtc_rtcsr < ALARM_WAVE_TIME) {
                return true;
        } else {
                return false;
        }

        return false;
}
