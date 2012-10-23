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

/*
 * Note:
  "flag" is used as hdmi mode search index.
 */

/* 1 */
#define HDMI_640X480_P_60HZ_4X3						\
	{"640x480-p-60hz-4:3",						\
			60, 640, 480, KHZ2PICOS(25200), 48, 16,		\
			33, 10, 96, 2,					\
			~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_NONINTERLACED, 1}

/* 2 */
#define HDMI_720X480_P_60HZ_4X3						\
	{"720x480-p-60hz-4:3",						\
			60, 720, 480, KHZ2PICOS(27020), 60, 16,		\
			30, 9, 62, 6,					\
			~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_NONINTERLACED, 2}

/* 3 */
#define HDMI_720X480_P_60HZ_16X9					\
	{"720x480-p-60hz-16:9",						\
			60, 720, 480, KHZ2PICOS(27020), 60, 16,		\
			30, 9, 62, 6,					\
			~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_NONINTERLACED, 3}

/* 4 */
#define HDMI_1280X720_P_60HZ_16X9					\
	{"1280x720-p-60hz-16:9",					\
			60, 1280, 720, KHZ2PICOS(74250), 220, 110,	\
			20, 5, 40, 5,					\
			FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_NONINTERLACED, 4}

/* 5 */
#define HDMI_1920X1080_I_60HZ_16X9					\
	{"1920x1080-i-60hz-16:9",					\
			60, 1920, 540, KHZ2PICOS(74250), 148, 88,	\
			15, 2, 44, 5,					\
			FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_INTERLACED, 5}

/* 6 */
#define HDMI_720_1440X480_I_60HZ_4X3					\
	{"720-1440x480-i-60hz-4:3",					\
			60, 1440, 240, KHZ2PICOS(27027), 114, 38,	\
			15, 4, 124, 3,					\
			~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_INTERLACED, 6}

/* 7 */
#define HDMI_720_1440X480_I_60HZ_16X9					\
	{"720-1440x480-i-60hz-16:9",					\
			60, 1440, 240, KHZ2PICOS(27027), 114, 38,	\
			15, 4, 124, 3,					\
			~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_INTERLACED, 7}

/* 8 */
#define HDMI_720_1440X240_P_60HZ_4X3					\
	{"720-1440x240-p-60hz-4:3",					\
			60, 1440, 240, KHZ2PICOS(27027), 114, 38,	\
			15, 5, 124, 3,					\
			~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_NONINTERLACED, 8}

/* 9 */
#define HDMI_720_1440X240_P_60HZ_16X9					\
	{"720-1440x240-p-60hz-16:9",					\
			60, 1440, 240, KHZ2PICOS(27027), 114, 38,	\
			15, 5, 124, 3,					\
			~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_NONINTERLACED, 9}

/* 10 */
#define HDMI_2880X480_I_60HZ_4X3					\
	{"2880x480-i-60hz-4:3",						\
			60, 2880, 240, KHZ2PICOS(54054), 228, 76,	\
			15, 4, 248, 3,					\
			~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_INTERLACED, 10}

/* 11 */
#define HDMI_2880X480_I_60HZ_16X9					\
	{"2880x480-i-60hz-16:9",					\
			60, 2880, 240, KHZ2PICOS(54054), 228, 76,	\
			15, 4, 248, 3,					\
			~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_INTERLACED, 11}

/* 12 */
#define HDMI_2880X240_P_60HZ_4X3					\
	{"2880x240-p-60hz-4:3",						\
			60, 2880, 240, KHZ2PICOS(54054), 228, 76,	\
			15, 4, 248, 3,					\
			~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_NONINTERLACED, 12}

/* 13 */
#define HDMI_2880X240_P_60HZ_16X9					\
	{"2880x240-p-60hz-16:9",					\
			60, 2880, 240, KHZ2PICOS(54054), 228, 76,	\
			15, 4, 248, 3,					\
			~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_NONINTERLACED, 13}

/* 14 */
#define HDMI_1440X480_P_60HZ_4X3					\
	{"1440x480-p-60hz-4:3",						\
			60, 1440, 480, KHZ2PICOS(54054), 120, 32,	\
			30, 9, 124, 6,					\
			~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_NONINTERLACED, 14}

/* 15 */
#define HDMI_1440X480_P_60HZ_16X9					\
	{"1440x480-p-60hz-16:9",					\
			60, 1440, 480, KHZ2PICOS(54054), 120, 32,	\
			30, 9, 124, 6,					\
			~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_NONINTERLACED, 15}

/* 16 */
#define HDMI_1920X1080_P_60HZ_16X9					\
	{"1920x1080-p-60hz-16:9",					\
			60, 1920, 1080, KHZ2PICOS(148500), 148, 88,	\
			36, 4, 44, 5,					\
			FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_NONINTERLACED, 16}

/* 17 */
#define HDMI_720X576_P_50HZ_4X3						\
	{"720x576-p-50hz-4:3",						\
			50, 720, 576, KHZ2PICOS(27000), 68, 12,		\
			39, 5, 64, 5,					\
			~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_NONINTERLACED, 17}

/* 18 */
#define HDMI_720X576_P_50HZ_16X9					\
	{"720x576-p-50hz-16:9",						\
			50, 720, 576, KHZ2PICOS(27000), 68, 12,		\
			39, 5, 64, 5,					\
			~FB_SYNC_HOR_HIGH_ACT & ~FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_NONINTERLACED, 18}

/* 19 */
#define HDMI_1280X720_P_50HZ_16X9					\
	{"1280x720-p-50hz-16:9",					\
			50, 1280, 720, KHZ2PICOS(74250), 220, 440,	\
			20, 5, 40, 5,					\
			FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_NONINTERLACED, 19}

/* 20 */
#define HDMI_1920X1080_I_50HZ_16X9					\
	{"1920x1080-i-50hz-16:9",					\
			50, 1920, 540, KHZ2PICOS(74250), 148, 528,	\
			15, 2, 44, 5,					\
			FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,	\
			FB_VMODE_INTERLACED, 20}

