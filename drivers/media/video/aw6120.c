#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-mediabus.h>
#include <mach/ovisp-v4l2.h>
//#include "AW6120.h"


#define AW6120_CHIP_ID_H	(0x58)
#define AW6120_CHIP_ID_L	(0x43)

#define MAX_WIDTH		1280
#define MAX_HEIGHT		720

#define AW6120_REG_END		0xffff
#define AW6120_REG_DELAY	0xffff

//#define AW6120_YUV

struct AW6120_format_struct;
struct AW6120_info {
	struct v4l2_subdev sd;
	struct AW6120_format_struct *fmt;
	struct AW6120_win_setting *win;
};

struct regval_list {
	unsigned short reg_num;
	unsigned char value;
};
static struct regval_list AW6120_stream_on[] = {
#if 1 // color bar
	{0xfffd,0x80},
	{0xfffe,0x80},
	{0x0090,0x2b},/*2b colorbar 28 mipi rx*/
#endif
	{0xfffd,0x80},
	{0xfffe,0x26},
	{0x2023,0x03},
	{AW6120_REG_END, 0x00},	/* END MARKER */
};

static struct regval_list AW6120_stream_off[] = {
	{0xfffd,0x80},
	{0xfffe,0x26},
	{0x2023,0x00},
	{AW6120_REG_END, 0x00},	/* END MARKER */
};
#if 0
static struct regval_list AW6120_init_720p_yuyv_regs[] = {
	{0xfffd,0x80},
	{0xfffe,0x80},
	{0x001c,0xff},
	{0x001d,0xff},
	{0x001e,0xff},
	{0x001f,0xff},
	{0x0018,0x00},
	{0x0019,0x00},
	{0x001a,0x00},
	{0x001b,0x00},
	{0x00bc,0x11},
	{0x00bd,0x00},
	{0x00be,0x00},
	{0x00bf,0x00},
	{0x0020,0x01},
	{0x0021,0x0e},
	{0x0022,0x00},
	{0x0023,0x00},
	{0x0024,0x04},
	{0x0025,0x0e},
	{0x0026,0x01},
	{0x0027,0x0e},
	{0x0030,0x61},
	{0x0031,0x20},
	{0x0032,0x70},
	{0x0033,0x12},

	{0xfffd,0x80},
	{0xfffe,0x26},
	{0x0002,0x00},

	{0xfffe,0x25},
	{0x0002,0x40},

	{0xfffe,0x80},
	{0x001e,0xfb},
	{0x0050,0x06},
	{0x0054,0x06},
	{0x0058,0x04},
	{0x001e,0xff},
	{0x0058,0x00},
	{0x0058,0x02},
	{AW6120_REG_DELAY, 0x20},	/* DELAY MARKER */

	{0xfffd,0x80},
	{0xfffe,0x80},
	{0x00bc,0x91},
	{0x001b,0x00},
	{0x0090,0x28},//2b colorbar

	{0xfffe,0x26},
	{0x0000,0x20},
	{0x0002,0x00},
	{0x0009,0x04},
	{0x4000,0xf9},
	{0x6001,0x17},
	{0x6005,0x04},
	{0x6006,0x0a},
	{0x6007,0x8c},
	{0x6008,0x09},
	{0x6009,0xfc},
	{0x8000,0x1f},
	{0x8001,0x00},
	{0x8002,0x05},
	{0x8003,0xd0},
	{0x8004,0x02},
	{0x8005,0x03},
	{0x8006,0x05},
	{0x8007,0x99},
	{0x8010,0x04},
	{0x2019,0x05},
	{0x201a,0x00},
	{0x201b,0x02},
	{0x201c,0xd0},
	{0x201d,0x00},
	{0x201e,0x00},
	{0x201f,0x00},
	{0x2020,0x00},
	{0x2015,0x80},
	{0x2017,0x1e},
	{0x2018,0x1e},
	{0x2023,0x03},
	{0x8012,0x00},//1280
	{0x8013,0x05},
	{0x8014,0xd0},//720
	{0x8015,0x02},
	{0x8016,0x00},
	{0x8017,0x00},
	{0x8018,0x00},
	{0x8019,0x00},

	{0xfffe,0x21},
	{0x0001,0x01},
	{0x0004,0x10},
	{0x0708,0x00},
	{0x0072,0x00},
	{0x0074,0x00},
	{0x0006,0x05},
	{0x0007,0x00},
	{0x0008,0x02},
	{0x0009,0xd0},
	{0x000a,0x05},
	{0x000b,0x00},
	{0x000c,0x02},
	{0x000d,0xd0},
	{0x001e,0x05},
	{0x001f,0x00},
	{0x0020,0x02},
	{0x0021,0xd0},
	{0x005e,0xff},
	{0x005f,0x04},
	{0x0060,0xcf},
	{0x0061,0x02},
	{0x0064,0x00},
	{0x0065,0x05},
	{0x0066,0xd0},
	{0x0067,0x02},
	{0x0076,0x05},
	{0x0077,0x00},
	{0x0078,0x02},
	{0x0079,0xd0},
	{0x0700,0x00},
	{0x0701,0x00},
	{0x0702,0x00},
	{0x0703,0x00},
	{0x0704,0x05},
	{0x0705,0x00},
	{0x0706,0x02},
	{0x0707,0xd0},
	{AW6120_REG_END, 0x00},	/* END MARKER */
};
static struct regval_list AW6120_init_720p_yuyv_576m_regs[] = {
	{0xfffd,0x80},
	{0xfffe,0x80},
	{0x001c,0xff},
	{0x001d,0xff},
	{0x001e,0xff},
	{0x001f,0xff},
	{0x0018,0x00},
	{0x0019,0x00},
	{0x001a,0x00},
	{0x001b,0x00},
	{0x00bc,0x11},
	{0x00bd,0x00},
	{0x00be,0x00},
	{0x00bf,0x00},
	{0x0020,0x01},
	{0x0021,0x0e},
	{0x0022,0x00},
	{0x0023,0x00},
	{0x0024,0x03},
	{0x0025,0x0e},
	{0x0026,0x01},
	{0x0027,0x0e},
	{0x0030,0x51},
	{0x0031,0x20},
	{0x0032,0x70},
	{0x0033,0x11},

	{0xfffd,0x80},
	{0xfffe,0x26},
	{0x0002,0x00},

	{0xfffe,0x25},
	{0x0002,0x40},

	{0xfffe,0x80},
	{0x001e,0xfb},
	{0x0050,0x06},
	{0x0054,0x06},
	{0x0058,0x04},
	{0x001e,0xff},
	{0x0058,0x00},
	{0x0058,0x02},
	{AW6120_REG_DELAY, 0x20},	/* DELAY MARKER */

	{0xfffd,0x80},
	{0xfffe,0x80},
	{0x00bc,0x91},
	{0x001b,0x00},
	{0x0090,0x2b},//2b colorbar

	{0xfffe,0x26},
	{0x0000,0x20},
	{0x0002,0x00},
	{0x0009,0x04},
	{0x4000,0xf9},
	{0x6001,0x17},
	{0x6005,0x04},
	{0x6006,0x0a},
	{0x6007,0x8c},
	{0x6008,0x09},
	{0x6009,0xfc},
	{0x8000,0x1f},
	{0x8001,0x00},
	{0x8002,0x05},
	{0x8003,0xd0},
	{0x8004,0x02},
	{0x8005,0x03},
	{0x8006,0x05},
	{0x8007,0x99},
	{0x8010,0x04},
	{0x2019,0x05},
	{0x201a,0x00},
	{0x201b,0x02},
	{0x201c,0xd0},
	{0x201d,0x00},
	{0x201e,0x00},
	{0x201f,0x00},
	{0x2020,0x00},
	{0x2015,0x80},
	{0x2017,0x1e},
	{0x2018,0x1e},
	{0x2023,0x00},
	{0x8012,0x00},//1280
	{0x8013,0x05},
	{0x8014,0xd0},//720
	{0x8015,0x02},
	{0x8016,0x00},
	{0x8017,0x00},
	{0x8018,0x00},
	{0x8019,0x00},

	{0xfffe,0x21},
	{0x0001,0x01},
	{0x0004,0x10},
	{0x0708,0x00},
	{0x0072,0x00},
	{0x0074,0x00},
	{0x0006,0x05},
	{0x0007,0x00},
	{0x0008,0x02},
	{0x0009,0xd0},
	{0x000a,0x05},
	{0x000b,0x00},
	{0x000c,0x02},
	{0x000d,0xd0},
	{0x001e,0x05},
	{0x001f,0x00},
	{0x0020,0x02},
	{0x0021,0xd0},
	{0x005e,0xff},
	{0x005f,0x04},
	{0x0060,0xcf},
	{0x0061,0x02},
	{0x0064,0x00},
	{0x0065,0x05},
	{0x0066,0xd0},
	{0x0067,0x02},
	{0x0076,0x05},
	{0x0077,0x00},
	{0x0078,0x02},
	{0x0079,0xd0},
	{0x0700,0x00},
	{0x0701,0x00},
	{0x0702,0x00},
	{0x0703,0x00},
	{0x0704,0x05},
	{0x0705,0x00},
	{0x0706,0x02},
	{0x0707,0xd0},
	{AW6120_REG_END, 0x00},	/* END MARKER */
};
#else
static struct regval_list AW6120_init_720p_yuyv_regs[] = {
	{0xfffd,0x80},
	{0xfffe,0x80},
	{0x001c,0xff},
	{0x001d,0xff},
	{0x001e,0xff},
	{0x001f,0xff},
	{0x0018,0x00},
	{0x0019,0x00},
	{0x001a,0x00},
	{0x001b,0x00},
	{0x00bc,0x11},
	{0x00bd,0x00},
	{0x00be,0x00},
	{0x00bf,0x00},
	{0x0020,0x01},
	{0x0021,0x0e},
	{0x0022,0x00},
	{0x0023,0x00},
	{0x0024,0x03},
	{0x0025,0x0e},
	{0x0026,0x01},
	{0x0027,0x0e},
	{0x0030,0x51},
	{0x0031,0x20},
	{0x0032,0x70},
	{0x0033,0x11},

	{0xfffd,0x80},
	{0xfffe,0x26},
	{0x0002,0x00},

	{0xfffe,0x25},
	{0x0002,0x40},

	{0xfffe,0x80},
	{0x001e,0xfb},
	{0x0050,0x06},
	{0x0054,0x06},
	{0x0058,0x04},
	{0x001e,0xff},
	{0x0058,0x00},
	{0x0058,0x02},
//	{AW6120_REG_DELAY, 0x20},	/* DELAY MARKER */

	{0xfffd,0x80},
	{0xfffe,0x80},
	{0x00bc,0x91},
	{0x001b,0x00},
	{0x0090,0x28},//2b colorbar 28 mipi rx

	{0xfffe,0x26},
	{0x0000,0x20},
	{0x0002,0x00},
	{0x0009,0x04},
	{0x4000,0xf9},
	{0x6001,0x17},
	{0x6005,0x04},
	{0x6006,0x0a},
	{0x6007,0x8c},
	{0x6008,0x09},
	{0x6009,0xfc},
	{0x8000,0x3f}, //modify
	{0x8001,0x00},
	{0x8002,0x05},
	{0x8003,0xd0},
	{0x8004,0x02},
	{0x8005,0x03},
	{0x8006,0x05},
	{0x8007,0x99},
	{0x8010,0x04},
	{0x2019,0x05},
	{0x201a,0x00},
	{0x201b,0x02},
	{0x201c,0xd0},
	{0x201d,0x00},
	{0x201e,0x00},
	{0x201f,0x00},
	{0x2020,0x00},
	{0x2015,0x80},
	{0x2017,0x1e},
	{0x2018,0x1e},
	{0x2023,0x00},
	{0x8012,0x00},//1280
	{0x8013,0x05},
	{0x8014,0xd0},//720
	{0x8015,0x02},
	{0x8016,0x00},
	{0x8017,0x00},
	{0x8018,0x00},
	{0x8019,0x00},

	{0xfffe,0x21},
	{0x0001,0x01},
	{0x0004,0x10},
	{0x0708,0x00},
	{0x0072,0x00},
	{0x0074,0x00},
	{0x0006,0x05},
	{0x0007,0x00},
	{0x0008,0x02},
	{0x0009,0xd0},
	{0x000a,0x05},
	{0x000b,0x00},
	{0x000c,0x02},
	{0x000d,0xd0},
	{0x001e,0x05},
	{0x001f,0x00},
	{0x0020,0x02},
	{0x0021,0xd0},
	{0x005e,0xff},
	{0x005f,0x04},
	{0x0060,0xcf},
	{0x0061,0x02},
	{0x0064,0x00},
	{0x0065,0x05},
	{0x0066,0xd0},
	{0x0067,0x02},
	{0x0076,0x05},
	{0x0077,0x00},
	{0x0078,0x02},
	{0x0079,0xd0},
	{0x0700,0x00},
	{0x0701,0x00},
	{0x0702,0x00},
	{0x0703,0x00},
	{0x0704,0x05},
	{0x0705,0x00},
	{0x0706,0x02},
	{0x0707,0xd0},
	{AW6120_REG_END, 0x00},	/* END MARKER */
};

#endif
static struct AW6120_format_struct {
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
} AW6120_formats[] = {
	{
		/*YUYV FORMAT, 16 bit per pixel*/
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_1X16,
		.colorspace	= V4L2_COLORSPACE_SRGB,
	},
	/*add other format supported*/
};
#define N_AW6120_FMTS ARRAY_SIZE(AW6120_formats)

static struct AW6120_win_setting {
	int	width;
	int	height;
	enum v4l2_mbus_pixelcode mbus_code;
	enum v4l2_colorspace colorspace;
	struct regval_list *regs; /* Regs to tweak */
} AW6120_win_sizes[] = {
	/* 1280*800 */
	{
		.width		= 1280,
		.height		= 720,
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_1X16,
		.colorspace	= V4L2_COLORSPACE_SRGB,
		.regs 		= AW6120_init_720p_yuyv_regs,
	}
};
#define N_WIN_SIZES (ARRAY_SIZE(AW6120_win_sizes))

int AW6120_read(struct v4l2_subdev *sd, unsigned short reg,
		unsigned char *value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[2] = {reg >> 8, reg & 0xff};
	struct i2c_msg msg[2] = {
		[0] = {
			.addr	= client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= buf,
		},
		[1] = {
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.len	= 1,
			.buf	= value,
		}
	};
	int ret;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret > 0)
		ret = 0;

	return ret;
}


static int AW6120_write(struct v4l2_subdev *sd, unsigned short reg,
		unsigned char value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[3] = {reg >> 8, reg & 0xff, value};
	struct i2c_msg msg = {
		.addr	= client->addr,
		.flags	= 0,
		.len	= 3,
		.buf	= buf,
	};
	int ret;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (ret > 0)
		ret = 0;

	return ret;
}


static int AW6120_read_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	unsigned char val;
	while (vals->reg_num != AW6120_REG_END || vals->value != 0) {
		if (vals->reg_num == AW6120_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
		} else {
			ret = AW6120_read(sd, vals->reg_num, &val);
			if (ret < 0)
				return ret;
		}
		printk("vals->reg_num:%x, vals->value:%x\n",vals->reg_num, val);
		//mdelay(200);
		vals++;
	}
	//printk("vals->reg_num:%x, vals->value:%x\n", vals->reg_num, vals->value);
	AW6120_write(sd, vals->reg_num, vals->value);
	return 0;
}
static int AW6120_write_array(struct v4l2_subdev *sd, struct regval_list *vals)
{
	int ret;
	while (vals->reg_num != AW6120_REG_END || vals->value != 0) {
		if (vals->reg_num == AW6120_REG_DELAY) {
			if (vals->value >= (1000 / HZ))
				msleep(vals->value);
			else
				mdelay(vals->value);
			//printk("vals->reg_num:%x, vals->value:%x\n",vals->reg_num, vals->value);
		} else {
			ret = AW6120_write(sd, vals->reg_num, vals->value);
			if (ret < 0)
				return ret;
		}
		//printk("vals->reg_num:%x, vals->value:%x\n",vals->reg_num, vals->value);
		//mdelay(200);
		vals++;
	}
	//printk("vals->reg_num:%x, vals->value:%x\n", vals->reg_num, vals->value);
	//AW6120_write(sd, vals->reg_num, vals->value);
	return 0;
}

/* R/G and B/G of typical camera module is defined here. */
static unsigned int rg_ratio_typical = 0x58;
static unsigned int bg_ratio_typical = 0x5a;

static int AW6120_update_awb_gain(struct v4l2_subdev *sd,
				unsigned int R_gain, unsigned int G_gain, unsigned int B_gain)
{
	int ret = 0;
	unsigned char h,l;
	AW6120_write(sd, 0xfffd, 0x80);
	AW6120_write(sd, 0xfffe, 0x21);
	/* red gain */
	ret = AW6120_read(sd, 0x0098, &h);
	if (ret < 0)
		return ret;
	ret = AW6120_read(sd, 0x0099, &l);
	if (ret < 0)
		return ret;
	R_gain = h & 0xf;
	R_gain = (R_gain << 8) | l;
	/* green gain */
	ret = AW6120_read(sd, 0x009a, &h);
	if (ret < 0)
		return ret;
	ret = AW6120_read(sd, 0x009b, &l);
	if (ret < 0)
		return ret;
	G_gain = h & 0xf;
	G_gain = (R_gain << 8) | l;
	/* blue gain */
	ret = AW6120_read(sd, 0x009c, &h);
	if (ret < 0)
		return ret;
	ret = AW6120_read(sd, 0x009d, &l);
	if (ret < 0)
		return ret;
	B_gain = h & 0xf;
	B_gain = (R_gain << 8) | l;
	return 0;
}

static int AW6120_reset(struct v4l2_subdev *sd, u32 val)
{
	int ret = 0;
	AW6120_write(sd, 0xfffd, 0x80);
	AW6120_write(sd, 0xfffe, 0x80);
	AW6120_write(sd, 0x0018, 0xff);
	return ret;
}

static int AW6120_detect(struct v4l2_subdev *sd);
static int AW6120_init(struct v4l2_subdev *sd, u32 val)
{
	struct AW6120_info *info = container_of(sd, struct AW6120_info, sd);
	int ret = 0;

	info->fmt = NULL;
	info->win = NULL;

	if (ret < 0)
		return ret;

	return 0;
}
static int AW6120_get_sensor_vts(struct v4l2_subdev *sd, unsigned short *value)
{
	unsigned char h,l;
	int ret = 0;
#if 1
	AW6120_write(sd, 0xfffd, 0x80);
	AW6120_write(sd, 0xfffe, 0x26);
	ret = AW6120_read(sd, 0x201b, &h);
	if (ret < 0)
		return ret;
	ret = AW6120_read(sd, 0x201c, &l);
	if (ret < 0)
		return ret;
	*value = h;
	*value = (*value << 8) | (l);
#endif
	//*value = 0x2d0;
	return ret;
}

static int AW6120_get_sensor_lans(struct v4l2_subdev *sd, unsigned char *value)
{
	*value = 2;
	return 0;
}
static int AW6120_detect(struct v4l2_subdev *sd)
{
	unsigned char v;
	int ret;

	//while(1){
	/*{
	  printk("0x10010410:%x\n", *((volatile unsigned int *)0xb0010410));
	  printk("0x10010420:%x\n", *((volatile unsigned int *)0xb0010420));
	  printk("0x10010430:%x\n", *((volatile unsigned int *)0xb0010430));
	  printk("0x10010440:%x\n", *((volatile unsigned int *)0xb0010440));
	  }*/
	AW6120_write(sd, 0xfffd, 0x80);
	AW6120_write(sd, 0xfffe, 0x80);
	ret = AW6120_read(sd, 0x0003, &v);
	if (ret < 0)
		return ret;
	//printk("-----%s: %d v = %08X\n", __func__, __LINE__, v);
	if (v != AW6120_CHIP_ID_H)
		return -ENODEV;
	ret = AW6120_read(sd, 0x0002, &v);
	if (ret < 0)
		return ret;
	//printk("-----%s: %d v = %08X\n", __func__, __LINE__, v);
	if (v != AW6120_CHIP_ID_L)
		return -ENODEV;
	return 0;
}

static int AW6120_enum_mbus_fmt(struct v4l2_subdev *sd, unsigned index,
					enum v4l2_mbus_pixelcode *code)
{
	if (index >= N_AW6120_FMTS)
		return -EINVAL;

	*code = AW6120_formats[index].mbus_code;
	return 0;
}

static int AW6120_try_fmt_internal(struct v4l2_subdev *sd,
		struct v4l2_mbus_framefmt *fmt,
		struct AW6120_win_setting **ret_wsize)
{
	struct AW6120_win_setting *wsize;

	if(fmt->width > MAX_WIDTH || fmt->height > MAX_HEIGHT)
		return -EINVAL;
	for (wsize = AW6120_win_sizes; wsize < AW6120_win_sizes + N_WIN_SIZES;
	     wsize++)
		if (fmt->width >= wsize->width && fmt->height >= wsize->height)
			break;
	if (wsize >= AW6120_win_sizes + N_WIN_SIZES)
		wsize--;   /* Take the smallest one */
	if (ret_wsize != NULL)
		*ret_wsize = wsize;
	fmt->width = wsize->width;
	fmt->height = wsize->height;
	fmt->code = wsize->mbus_code;
	fmt->field = V4L2_FIELD_NONE;
	fmt->colorspace = wsize->colorspace;
	return 0;
}

static int AW6120_try_mbus_fmt(struct v4l2_subdev *sd,
			    struct v4l2_mbus_framefmt *fmt)
{
	return AW6120_try_fmt_internal(sd, fmt, NULL);
}

unsigned long long aw6120_time = 0;

static int AW6120_s_mbus_fmt(struct v4l2_subdev *sd,
			  struct v4l2_mbus_framefmt *fmt)
{
	struct AW6120_info *info = container_of(sd, struct AW6120_info, sd);
	struct v4l2_fmt_data *data = v4l2_get_fmt_data(fmt);
	struct AW6120_win_setting *wsize;
	int ret;

	ret = AW6120_try_fmt_internal(sd, fmt, &wsize);
	if (ret)
		return ret;
	if ((info->win != wsize) && wsize->regs) {
		ret = AW6120_write_array(sd, wsize->regs);
		aw6120_time = sched_clock();
		if (ret)
			return ret;
	}
	data->i2cflags = 0;
	data->mipi_clk = 282;
	ret = AW6120_get_sensor_vts(sd, &(data->vts));
	if(ret < 0)
		return ret;

	ret = AW6120_get_sensor_lans(sd, &(data->lans));
	if(ret < 0)
		return ret;

	info->win = wsize;
#if 0
	{
	unsigned char h;
	AW6120_write(sd, 0xfffd, 0x80);
	AW6120_write(sd, 0xfffe, 0x80);
	AW6120_write(sd, 0x0137, 0x66);
	udelay(5);
	/* red gain */
	AW6120_write(sd, 0xfffe, 0x80);
	AW6120_read(sd, 0x0137, &h);
//	printk("&&&&&&&&& 0x0137 = 0x%02x\n",h);
//	while(1);
	}
#endif
	return 0;
}

static int AW6120_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = 0;

	if (enable) {
		ret = AW6120_write_array(sd, AW6120_stream_on);
		printk("AW6120 stream on\n");
	}
	else {
		ret = AW6120_write_array(sd, AW6120_stream_off);
		printk("AW6120 stream off\n");
	}
	return ret;
}

static int AW6120_g_crop(struct v4l2_subdev *sd, struct v4l2_crop *a)
{
	a->c.left	= 0;
	a->c.top	= 0;
	a->c.width	= MAX_WIDTH;
	a->c.height	= MAX_HEIGHT;
	a->type		= V4L2_BUF_TYPE_VIDEO_CAPTURE;

	return 0;
}

static int AW6120_cropcap(struct v4l2_subdev *sd, struct v4l2_cropcap *a)
{
	a->bounds.left			= 0;
	a->bounds.top			= 0;
	a->bounds.width			= MAX_WIDTH;
	a->bounds.height		= MAX_HEIGHT;
	a->defrect			= a->bounds;
	a->type				= V4L2_BUF_TYPE_VIDEO_CAPTURE;
	a->pixelaspect.numerator	= 1;
	a->pixelaspect.denominator	= 1;

	return 0;
}

static int AW6120_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int AW6120_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *parms)
{
	return 0;
}

static int AW6120_frame_rates[] = { 90, 60, 45, 30, 15 };

static int AW6120_enum_frameintervals(struct v4l2_subdev *sd,
		struct v4l2_frmivalenum *interval)
{
	if (interval->index >= ARRAY_SIZE(AW6120_frame_rates))
		return -EINVAL;
	interval->type = V4L2_FRMIVAL_TYPE_DISCRETE;
	interval->discrete.numerator = 1;
	interval->discrete.denominator = AW6120_frame_rates[interval->index];
	return 0;
}

static int AW6120_enum_framesizes(struct v4l2_subdev *sd,
		struct v4l2_frmsizeenum *fsize)
{
	int i;
	int num_valid = -1;
	__u32 index = fsize->index;

	/*
	 * If a minimum width/height was requested, filter out the capture
	 * windows that fall outside that.
	 */
	for (i = 0; i < N_WIN_SIZES; i++) {
		struct AW6120_win_setting *win = &AW6120_win_sizes[index];
		if (index == ++num_valid) {
			fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
			fsize->discrete.width = win->width;
			fsize->discrete.height = win->height;
			return 0;
		}
	}

	return -EINVAL;
}

static int AW6120_queryctrl(struct v4l2_subdev *sd,
		struct v4l2_queryctrl *qc)
{
	return 0;
}

static int AW6120_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int AW6120_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	return 0;
}

static int AW6120_g_chip_ident(struct v4l2_subdev *sd,
		struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

//	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_AW6120, 0);
	return v4l2_chip_ident_i2c_client(client, chip, 123, 0);
}

#ifdef CONFIG_VIDEO_ADV_DEBUG
static int AW6120_g_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char val = 0;
	int ret;

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	ret = AW6120_read(sd, reg->reg & 0xffff, &val);
	reg->val = val;
	reg->size = 2;
	return ret;
}

static int AW6120_s_register(struct v4l2_subdev *sd, struct v4l2_dbg_register *reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	if (!v4l2_chip_match_i2c_client(client, &reg->match))
		return -EINVAL;
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	AW6120_write(sd, reg->reg & 0xffff, reg->val & 0xff);
	return 0;
}
#endif

static int AW6120_s_power(struct v4l2_subdev *sd, int on)
{
	return 0;
}

static const struct v4l2_subdev_core_ops AW6120_core_ops = {
	.g_chip_ident = AW6120_g_chip_ident,
	.g_ctrl = AW6120_g_ctrl,
	.s_ctrl = AW6120_s_ctrl,
	.queryctrl = AW6120_queryctrl,
	.reset = AW6120_reset,
	.init = AW6120_init,
	.s_power = AW6120_s_power,
#ifdef CONFIG_VIDEO_ADV_DEBUG
	.g_register = AW6120_g_register,
	.s_register = AW6120_s_register,
#endif
};

static const struct v4l2_subdev_video_ops AW6120_video_ops = {
	.enum_mbus_fmt = AW6120_enum_mbus_fmt,
	.try_mbus_fmt = AW6120_try_mbus_fmt,
	.s_mbus_fmt = AW6120_s_mbus_fmt,
	.s_stream = AW6120_s_stream,
	.cropcap = AW6120_cropcap,
	.g_crop	= AW6120_g_crop,
	.s_parm = AW6120_s_parm,
	.g_parm = AW6120_g_parm,
	.enum_frameintervals = AW6120_enum_frameintervals,
	.enum_framesizes = AW6120_enum_framesizes,
};

static const struct v4l2_subdev_ops AW6120_ops = {
	.core = &AW6120_core_ops,
	.video = &AW6120_video_ops,
};

static ssize_t AW6120_rg_ratio_typical_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", rg_ratio_typical);
}

static ssize_t AW6120_rg_ratio_typical_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	rg_ratio_typical = (unsigned int)value;

	return size;
}

static ssize_t AW6120_bg_ratio_typical_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", bg_ratio_typical);
}

static ssize_t AW6120_bg_ratio_typical_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *endp;
	int value;

	value = simple_strtoul(buf, &endp, 0);
	if (buf == endp)
		return -EINVAL;

	bg_ratio_typical = (unsigned int)value;

	return size;
}

static DEVICE_ATTR(AW6120_rg_ratio_typical, 0664, AW6120_rg_ratio_typical_show, AW6120_rg_ratio_typical_store);
static DEVICE_ATTR(AW6120_bg_ratio_typical, 0664, AW6120_bg_ratio_typical_show, AW6120_bg_ratio_typical_store);

static int AW6120_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct v4l2_subdev *sd;
	struct AW6120_info *info;
	int ret;

	info = kzalloc(sizeof(struct AW6120_info), GFP_KERNEL);
	if (info == NULL)
		return -ENOMEM;
	sd = &info->sd;
	v4l2_i2c_subdev_init(sd, client, &AW6120_ops);

	/* Make sure it's an AW6120 */
//aaa:
	ret = AW6120_detect(sd);
	if (ret) {
		v4l_err(client,
			"chip found @ 0x%x (%s) is not an AW6120 chip.\n",
			client->addr, client->adapter->name);
//		goto aaa;
		kfree(info);
		return ret;
	}
	v4l_info(client, "AW6120 chip found @ 0x%02x (%s)\n",
			client->addr, client->adapter->name);

	ret = device_create_file(&client->dev, &dev_attr_AW6120_rg_ratio_typical);
	if(ret){
		v4l_err(client, "create dev_attr_AW6120_rg_ratio_typical failed!\n");
		goto err_create_dev_attr_AW6120_rg_ratio_typical;
	}

	ret = device_create_file(&client->dev, &dev_attr_AW6120_bg_ratio_typical);
	if(ret){
		v4l_err(client, "create dev_attr_AW6120_bg_ratio_typical failed!\n");
		goto err_create_dev_attr_AW6120_bg_ratio_typical;
	}

	//printk("probe ok ------->AW6120\n");
	return 0;

err_create_dev_attr_AW6120_bg_ratio_typical:
	device_remove_file(&client->dev, &dev_attr_AW6120_rg_ratio_typical);
err_create_dev_attr_AW6120_rg_ratio_typical:
	kfree(info);
	return ret;
}

static int AW6120_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	struct AW6120_info *info = container_of(sd, struct AW6120_info, sd);

	device_remove_file(&client->dev, &dev_attr_AW6120_rg_ratio_typical);
	device_remove_file(&client->dev, &dev_attr_AW6120_bg_ratio_typical);

	v4l2_device_unregister_subdev(sd);
	kfree(info);
	return 0;
}

static const struct i2c_device_id AW6120_id[] = {
	{ "aw6120", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, AW6120_id);

static struct i2c_driver AW6120_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "aw6120",
	},
	.probe		= AW6120_probe,
	.remove		= AW6120_remove,
	.id_table	= AW6120_id,
};

static __init int init_AW6120(void)
{
	return i2c_add_driver(&AW6120_driver);
}

static __exit void exit_AW6120(void)
{
	i2c_del_driver(&AW6120_driver);
}

module_init(init_AW6120);
module_exit(exit_AW6120);

MODULE_DESCRIPTION("A low-level driver for OmniVision AW6120 sensors");
MODULE_LICENSE("GPL");
