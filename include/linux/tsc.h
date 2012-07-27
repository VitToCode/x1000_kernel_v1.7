#ifndef __JZTSC_H__
#define __JZTSC_H__
#include <linux/gpio.h>

struct jztsc_pin {
	unsigned short			num;
#define LOW_ENABLE			0
#define HIGH_ENABLE			1
	unsigned short 			enable_level;
};

__attribute__((weak)) struct jztsc_platform_data {
	struct jztsc_pin		*gpio;
};

static inline int get_pin_status(struct jztsc_pin *pin)
{
	int val;

	if (pin->num < 0)
		return -1;
	val = gpio_get_value(pin->num);

	if (pin->enable_level == LOW_ENABLE)
		return !val;
	return val;
}

static inline void set_pin_status(struct jztsc_pin *pin, int enable)
{
	if (pin->num < 0)
		return;

	if (pin->enable_level == LOW_ENABLE)
		enable = !enable;
	gpio_set_value(pin->num, enable);
}
#endif
