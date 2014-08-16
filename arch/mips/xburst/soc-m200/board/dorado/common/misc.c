
#include <linux/platform_device.h>
#include <linux/power/jz_battery.h>
#include <linux/power/li_ion_charger.h>
#include <linux/jz_adc.h>
#include <mach/jz_efuse.h>
#include "board_base.h"

#ifdef CONFIG_JZ_EFUSE_V12
static struct jz_efuse_platform_data jz_efuse_pdata = {
	/* supply 2.5V to VDDQ */
	.gpio_vddq_en_n = GPIO_EFUSE_VDDQ,
};
#endif

#ifdef CONFIG_CHARGER_LI_ION
/* li-ion charger */
static struct li_ion_charger_platform_data jz_li_ion_charger_pdata = {
	.gpio_charge = GPIO_LI_ION_CHARGE,
	.gpio_ac = GPIO_LI_ION_AC,
	.gpio_active_low = GPIO_ACTIVE_LOW,
};

static struct platform_device jz_li_ion_charger_device = {
	.name = "li-ion-charger",
	.dev = {
		.platform_data = &jz_li_ion_charger_pdata,
	},
};
#endif


#ifdef CONFIG_JZ_BATTERY
static struct jz_battery_info  dorado_battery_info = {
	.max_vol        = 4050,
	.min_vol        = 3600,
	.usb_max_vol    = 4100,
	.usb_min_vol    = 3760,
	.ac_max_vol     = 4100,
	.ac_min_vol     = 3760,
	.battery_max_cpt = 3000,
	.ac_chg_current = 800,
	.usb_chg_current = 400,
};
struct jz_adc_platform_data adc_platform_data = {
	battery_info = dorado_battery_info;
}
#endif
