/*
 *  Copyright (C) 2009, Lars-Peter Clausen <lars@metafoo.de>
 *
 *  This program is free software; you can redistribute	 it and/or modify it
 *  under  the terms of	 the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the	License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifndef __ASM_MACH_JZ4780_JZ4780_FB_H__
#define __ASM_MACH_JZ4780_JZ4780_FB_H__

#include <linux/fb.h>

enum jz4780_fb_lcd_type {
	LCD_TYPE_GENERIC_16_BIT = 0,
	LCD_TYPE_GENERIC_18_BIT = 0 | (1 << 7),
	LCD_TYPE_GENERIC_24_BIT = 0 | (1 << 6),
	LCD_TYPE_SPECIAL_TFT_1 = 1,
	LCD_TYPE_SPECIAL_TFT_2 = 2,
	LCD_TYPE_SPECIAL_TFT_3 = 3,
	LCD_TYPE_NON_INTERLACED_CCIR656 = 4,
	LCD_TYPE_INTERLACED_CCIR656 = 6,
	LCD_TYPE_8BIT_SERIAL = 0xc,
	LCD_TYPE_LCM = 0xd,
};

#define JZ4780_FB_SPECIAL_TFT_CONFIG(start, stop) (((start) << 16) | (stop))

/*
* width: width of the lcd display in mm
* height: height of the lcd display in mm
* num_modes: size of modes
* modes: list of valid video modes
* bpp: bits per pixel for the lcd
* lcd_type: lcd type
*/

struct jzfb_platform_data {
	unsigned int enable;
	unsigned int width;
	unsigned int height;

	size_t num_modes;
	struct fb_videomode *modes;

	unsigned int bpp;
	enum jz4780_fb_lcd_type lcd_type;

	struct {
		uint32_t spl;
		uint32_t cls;
		uint32_t ps;
		uint32_t rev;
	} special_tft_config;

	struct {
		uint32_t spl;
	} smart_config;

	unsigned is_smart;
	unsigned pixclk_falling_edge;
	unsigned date_enable_active_low;
};

#endif
