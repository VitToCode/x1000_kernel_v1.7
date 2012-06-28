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
	LCD_TYPE_NON_INTERLACED_TV = 4 | (1 << 26),
	LCD_TYPE_INTERLACED_TV = 6 | (1 << 26) | (1 << 30),
	LCD_TYPE_8BIT_SERIAL = 0xc,
	LCD_TYPE_LCM = 0xd | (1 << 31),
};

/*******************************************************************************/
/* smart lcd interface_type */
enum smart_lcd_type {
	SMART_LCD_TYPE_PARALLEL,
	SMART_LCD_TYPE_SERIAL,
};

/* smart lcd command width */
enum smart_lcd_cwidth {
	SMART_LCD_CWIDTH_16_BIT_ONCE,
	SMART_LCD_CWIDTH_9_BIT_ONCE = SMART_LCD_CWIDTH_16_BIT_ONCE,
	SMART_LCD_CWIDTH_8_BIT_ONCE,
	SMART_LCD_CWIDTH_18_BIT_ONCE,
	SMART_LCD_CWIDTH_24_BIT_ONCE,
};

/* smart lcd data width */
enum smart_lcd_dwidth {
	SMART_LCD_DWIDTH_18_BIT_ONCE_PARALLEL_SERIAL,
	SMART_LCD_DWIDTH_16_BIT_ONCE_PARALLEL_SERIAL,
	SMART_LCD_DWIDTH_8_BIT_THIRD_TIME_PARALLEL,
	SMART_LCD_DWIDTH_8_BIT_TWICE_TIME_PARALLEL,
	SMART_LCD_DWIDTH_8_BIT_ONCE_PARALLEL_SERIAL,
	SMART_LCD_DWIDTH_24_BIT_ONCE_PARALLEL,
	SMART_LCD_DWIDTH_9_BIT_TWICE_TIME_PARALLEL = 7,
};
/*******************************************************************************/

#define JZ4780_FB_SPECIAL_TFT_CONFIG(start, stop) (((start) << 16) | (stop))

/*
 * @num_modes: size of modes
 * @modes: list of valid video modes
 * @lcd_type: lcd type
 * @lcdc0_to_tft_ttl: LCDC0 output to TFT TTL interface LCD
 * @bpp: bits per pixel for the lcd
 * @width: width of the lcd display in mm
 * @height: height of the lcd display in mm
 * @pinmd: 16bpp lcd data pin mapping. 0: LCD_D[15:0], 1: LCD_D[17:10] LCD_D[8:1]
 * @transfer_type: smart lcd transfer type, 0: parrallel, 1: serial
 * @cmd_width: smart lcd command width
 * @data_width:smart lcd data Width
 * @clkply: smart lcd clock polarity: 0- Active edge is Falling,  1- Active edge is Rasing
 * @rsply: smart lcd RS polarity. 0: Command_RS=0, Data_RS=1; 1: Command_RS=1, Data_RS=0
 * @csply: smart lcd CS Polarity: 0- Active level is low,  1- Active level is high
 * @spl: special_tft SPL signal register setting
 * @cls: special_tft CLS signal register setting
 * @ps: special_tft PS signal register setting
 * @rev: special_tft REV signal register setting
 * @pixclk_falling_edge: pixel clock at falling edge
 * @date_enable_active_low: data enable active low
 */

struct jzfb_platform_data {
	size_t num_modes;
	struct fb_videomode *modes;

	enum jz4780_fb_lcd_type lcd_type;
	int lcdc0_to_tft_ttl;
	unsigned int bpp;
	unsigned int width;
	unsigned int height;
	int pinmd;

	unsigned pixclk_falling_edge;
	unsigned date_enable_active_low;

	struct {
		enum smart_lcd_type smart_type;
		enum smart_lcd_cwidth cmd_width;
		enum smart_lcd_cwidth data_width;

		int clkply_active_rising;
		int rsply_cmd_high;
		int csply_active_high;
	} smart_config;

	struct {
		uint32_t spl;
		uint32_t cls;
		uint32_t ps;
		uint32_t rev;
	} special_tft_config;
};

#endif
