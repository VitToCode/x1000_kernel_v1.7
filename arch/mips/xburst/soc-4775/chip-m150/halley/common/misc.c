#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/tsc.h>
#include <linux/jz4780-adc.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/android_pmem.h>
#include <mach/platform.h>
#include <mach/jzsnd.h>
#include <mach/jzmmc.h>
#include <mach/jzssi.h>
#include <mach/jz4780_efuse.h>
#include <gpio.h>
#include <linux/jz_dwc.h>
#include <linux/interrupt.h>
#include <sound/jz-aic.h>
#include "board_base.h"
#include <board.h>

#ifdef CONFIG_JZ_LED_RGB
#include <linux/jz_ledrgb.h>
#endif


#ifdef CONFIG_JZ_LED_RGB
static struct jz_led_RGB_pdata jz_led_RGB_pdata = {
	.gpio_RGB_R = GPIO_JZ_LED_RGB_R,
	.gpio_RGB_G = GPIO_JZ_LED_RGB_G,
	.gpio_RGB_B = GPIO_JZ_LED_RGB_B,
};
struct platform_device jz_led_RGB= {
	.name       = "jz_led_RGB",
	.id     = 0,
	.dev        = {
		.platform_data  = &jz_led_RGB_pdata,
	}
};
#endif
