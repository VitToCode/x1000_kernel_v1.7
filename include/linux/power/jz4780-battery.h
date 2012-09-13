/* include/linux/power/jz4780-battery.h
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *	http://www.ingenic.com
 *	Sun Jiwei<jwsun@ingenic.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#ifndef __JZ4780_BATTERY_H
#define __JZ4780_BATTERY_H

struct jz_battery_info {
	int max_vol;
	int min_vol;
	int usb_max_vol;
	int usb_min_vol;
	int ac_max_vol;
	int ac_min_vol;

	int battery_max_cpt;
	int ac_chg_current;
	int usb_chg_current;
};

struct jz_battery_platform_data {
	struct jz_battery_info info;
};
#endif
