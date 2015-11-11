#ifndef __JZ_AXP173_ADC_H
#define __JZ_AXP173_ADC_H

#include <linux/device.h>
#ifdef CONFIG_JZ_CURRENT_BATTERY
#include <linux/power/jz-current-battery.h>
#elif defined(CONFIG_JZ_VOL_BATTERY)
#include <linux/power/jz-voltage-battery.h>
#endif

struct axp173_adc_platform_data{
#ifdef CONFIG_JZ_CURRENT_BATTERY
	struct jz_current_battery_info* battery_info;
#elif defined(CONFIG_JZ_VOL_BATTERY)
	struct jz_vol_battery_info* battery_info;
#endif
};

#endif /*jz-axp173-adc.h*/
