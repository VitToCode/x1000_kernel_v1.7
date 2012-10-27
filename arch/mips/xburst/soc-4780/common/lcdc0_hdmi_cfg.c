/*
 * JZFB0 hdmi platform data common setting
 *
 * Copyright (c) 2012 Ingenic Semiconductor Co., Ltd.
 *              http://www.ingenic.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/fb.h>
#include <mach/jzfb.h>
#include <mach/fb_hdmi_modes.h>

#ifdef CONFIG_FB_JZ4780_LCDC0
/* LCD Controller 0 output to HDMI */
static struct fb_videomode jzfb0_hdmi_videomode[] = {
	ADD_HDMI_VIDEO_MODE(HDMI_640X480_P_60HZ_4X3), /* 1 */
	ADD_HDMI_VIDEO_MODE(HDMI_720X480_P_60HZ_16X9), /* 3 */
	ADD_HDMI_VIDEO_MODE(HDMI_1280X720_P_60HZ_16X9), /* 4 */
//	ADD_HDMI_VIDEO_MODE(HDMI_1440X480_P_60HZ_16X9), /* 15 */
	ADD_HDMI_VIDEO_MODE(HDMI_1920X1080_P_60HZ_16X9), /* 16 */
//	ADD_HDMI_VIDEO_MODE(HDMI_720X576_P_50HZ_16X9), /* 18 */
	ADD_HDMI_VIDEO_MODE(HDMI_1280X720_P_50HZ_16X9), /* 19 */
	ADD_HDMI_VIDEO_MODE(HDMI_1920X1080_P_50HZ_16X9), /* 31 */
	ADD_HDMI_VIDEO_MODE(HDMI_1920X1080_P_24HZ_16X9), /* 32 */
	ADD_HDMI_VIDEO_MODE(HDMI_1920X1080_P_25HZ_16X9), /* 33 */
	ADD_HDMI_VIDEO_MODE(HDMI_1920X1080_P_30HZ_16X9), /* 34 */
	ADD_HDMI_VIDEO_MODE(HDMI_1280X720_P_100HZ_16X9), /* 41 */
	ADD_HDMI_VIDEO_MODE(HDMI_1280X720_P_120HZ_16X9), /* 47 */
	ADD_HDMI_VIDEO_MODE(HDMI_720X480_P_120HZ_16X9), /* 49 */
	ADD_HDMI_VIDEO_MODE(HDMI_720X480_P_240HZ_16X9), /* 57 */
	ADD_HDMI_VIDEO_MODE(HDMI_1280X720_P_25HZ_16X9), /* 61 */
	ADD_HDMI_VIDEO_MODE(HDMI_1280X720_P_30HZ_16X9), /* 62 */
	ADD_HDMI_VIDEO_MODE(HDMI_1920X1080_P_120HZ_16X9), /* 63 */
	ADD_HDMI_VIDEO_MODE(HDMI_1920X1080_P_100HZ_16X9), /* 64 */
	/* add other mode later */
};

struct jzfb_platform_data jzfb0_hdmi_pdata = {
	.num_modes = ARRAY_SIZE(jzfb0_hdmi_videomode),
	.modes = jzfb0_hdmi_videomode,

	.lcd_type = LCD_TYPE_GENERIC_24_BIT,
	.bpp = 24,
	.width = 0,
	.height = 0,

	.pixclk_falling_edge = 1,
	.date_enable_active_low = 0,

	.alloc_vidmem = 0,
};
#endif /* CONFIG_FB_JZ4780_LCDC0 */
