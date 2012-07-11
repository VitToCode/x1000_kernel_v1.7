/*
 * mach/jzhdmi_video_mode.h
 *
 * Copyright (c) 2012 Engenic Semiconductor Co., Ltd.
 *              http://www.engenic.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


#define ADD_HDMI_VIDEO_MODE(mode) mode

/*
 * struct fb_videomode - Defined in kernel/include/linux/fb.h
 * name
 * refresh, xres, yres, pixclock, left_margin, right_margin
 * upper_margin, lower_margin, hsync_len, vsync_len
 * sync
 * vmode, flag
 */
#define HDMI_640x480_P_60HZ_4x3						\
	{"640x480-p-60hz-4:3",						\
			60, 640, 480, 2520, 48, 16, 33, 10, 96, 2,	\
			~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_NONINTERLACED, 0}

#define HDMI_720x480_P_60HZ_4x3						\
	{"720x480-p-60hz-4:3",						\
			60, 720, 480, 2702, 60, 16, 30, 9, 62, 6,	\
			~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_NONINTERLACED, 0}

#define HDMI_720x480_P_60HZ_16x9					\
	{"720x480-p-60hz-16:9",						\
			60, 720, 480, 2702, 60, 16, 30, 9, 62, 6,	\
			~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_NONINTERLACED, 0}
