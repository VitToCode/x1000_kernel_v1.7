#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <mach/jz_cim.h>
#include "ov2650_camera.h"

#define CAMERA_MODE_PREVIEW 0
#define CAMERA_MODE_CAPTURE 1
static int mode = CAMERA_MODE_CAPTURE;
static int g_width = 800;
static int g_height = 600;

int ov2650_init(struct cim_sensor *sensor_info)
{
	struct ov2650_sensor *s;
	struct i2c_client * client ;
	s = container_of(sensor_info, struct ov2650_sensor, cs);
	client = s->client;
    mode = CAMERA_MODE_PREVIEW;
	/***************** init reg set **************************/
	/*** SVGA preview (800X600) 30fps 24MCLK input ***********/

	dev_info(&client->dev, "ov2650 -----------------------------------init\n");

    ov2650_write_reg(client, 0x3012, 0x80);
    mdelay(100);

    ov2650_write_reg(client, 0x308c, 0x80);
    ov2650_write_reg(client, 0x308d, 0x0e);
    ov2650_write_reg(client, 0x360b, 0x00);
    ov2650_write_reg(client, 0x30b0, 0xff);
    ov2650_write_reg(client, 0x30b1, 0xff);
    ov2650_write_reg(client, 0x30b2, 0x24);
    ov2650_write_reg(client, 0x300e, 0x34);
    ov2650_write_reg(client, 0x300f, 0xa6);
    ov2650_write_reg(client, 0x3010, 0x81);
    ov2650_write_reg(client, 0x3082, 0x01);
    ov2650_write_reg(client, 0x30f4, 0x01);
    ov2650_write_reg(client, 0x3090, 0x33);
    ov2650_write_reg(client, 0x3091, 0xc0);
    ov2650_write_reg(client, 0x30ac, 0x42);

    ov2650_write_reg(client, 0x30d1, 0x08);
    ov2650_write_reg(client, 0x30a8, 0x56);
    ov2650_write_reg(client, 0x3015, 0x03);
    ov2650_write_reg(client, 0x3093, 0x00);
    ov2650_write_reg(client, 0x307e, 0xe5);
    ov2650_write_reg(client, 0x3079, 0x00);
    ov2650_write_reg(client, 0x30aa, 0x42);
    ov2650_write_reg(client, 0x3017, 0x40);
    ov2650_write_reg(client, 0x30f3, 0x82);
    ov2650_write_reg(client, 0x306a, 0x0c);
    ov2650_write_reg(client, 0x306d, 0x00);
    ov2650_write_reg(client, 0x336a, 0x3c);
    ov2650_write_reg(client, 0x3076, 0x6a);
    ov2650_write_reg(client, 0x30d9, 0x95);
    ov2650_write_reg(client, 0x3016, 0x82);
    ov2650_write_reg(client, 0x3601, 0x30);
    ov2650_write_reg(client, 0x304e, 0x88);
    ov2650_write_reg(client, 0x30f1, 0x82);
    ov2650_write_reg(client, 0x306f, 0x14);
    ov2650_write_reg(client, 0x302a, 0x02);
    ov2650_write_reg(client, 0x302b, 0x6a);
                                         
    ov2650_write_reg(client, 0x3012, 0x10);
    ov2650_write_reg(client, 0x3011, 0x01);

    // AEC/AGC
    ov2650_write_reg(client, 0x3013, 0xf7);
    ov2650_write_reg(client, 0x301c, 0x13);
    ov2650_write_reg(client, 0x301d, 0x17);
    ov2650_write_reg(client, 0x3070, 0x5d);
    ov2650_write_reg(client, 0x3072, 0x4d);

    //D5060
    ov2650_write_reg(client, 0x30af, 0x00);
    ov2650_write_reg(client, 0x3048, 0x1f);
    ov2650_write_reg(client, 0x3049, 0x4e);
    ov2650_write_reg(client, 0x304a, 0x20);
    ov2650_write_reg(client, 0x304f, 0x20);
    ov2650_write_reg(client, 0x304b, 0x02);
    ov2650_write_reg(client, 0x304c, 0x00);
    ov2650_write_reg(client, 0x304d, 0x02);
    ov2650_write_reg(client, 0x304f, 0x20);
    ov2650_write_reg(client, 0x30a3, 0x10);
    ov2650_write_reg(client, 0x3013, 0xf7);
    ov2650_write_reg(client, 0x3014, 0x44);
    ov2650_write_reg(client, 0x3071, 0x00);
    ov2650_write_reg(client, 0x3070, 0x5d);
    ov2650_write_reg(client, 0x3073, 0x00);
    ov2650_write_reg(client, 0x3072, 0x4d);
    ov2650_write_reg(client, 0x301c, 0x05);
    ov2650_write_reg(client, 0x301d, 0x06);
    ov2650_write_reg(client, 0x304d, 0x42);
    ov2650_write_reg(client, 0x304a, 0x40);
    ov2650_write_reg(client, 0x304f, 0x40);
    ov2650_write_reg(client, 0x3095, 0x07);
    ov2650_write_reg(client, 0x3096, 0x16);
    ov2650_write_reg(client, 0x3097, 0x1d);

    //Window Setup
    ov2650_write_reg(client, 0x300e, 0x38);
    ov2650_write_reg(client, 0x3020, 0x01);
    ov2650_write_reg(client, 0x3021, 0x18);
    ov2650_write_reg(client, 0x3022, 0x00);
    ov2650_write_reg(client, 0x3023, 0x06);
    ov2650_write_reg(client, 0x3024, 0x06);
    ov2650_write_reg(client, 0x3025, 0x58);
    ov2650_write_reg(client, 0x3026, 0x02);
    ov2650_write_reg(client, 0x3027, 0x61);
    ov2650_write_reg(client, 0x3088, 0x03);
    ov2650_write_reg(client, 0x3089, 0x20);
    ov2650_write_reg(client, 0x308a, 0x02);
    ov2650_write_reg(client, 0x308b, 0x58);
    ov2650_write_reg(client, 0x3316, 0x64);
    ov2650_write_reg(client, 0x3317, 0x25);
    ov2650_write_reg(client, 0x3318, 0x80);
    ov2650_write_reg(client, 0x3319, 0x08);
    ov2650_write_reg(client, 0x331a, 0x64);
    ov2650_write_reg(client, 0x331b, 0x4b);
    ov2650_write_reg(client, 0x331c, 0x00);
    ov2650_write_reg(client, 0x331d, 0x38);
    ov2650_write_reg(client, 0x3100, 0x00);

    //AWB
    ov2650_write_reg(client, 0x3320, 0xfa);
    ov2650_write_reg(client, 0x3321, 0x11);
    ov2650_write_reg(client, 0x3322, 0x92);
    ov2650_write_reg(client, 0x3323, 0x01);
    ov2650_write_reg(client, 0x3324, 0x97);
    ov2650_write_reg(client, 0x3325, 0x02);
    ov2650_write_reg(client, 0x3326, 0xff);
    ov2650_write_reg(client, 0x3327, 0x0c);
    ov2650_write_reg(client, 0x3328, 0x10);
    ov2650_write_reg(client, 0x3329, 0x10);
    ov2650_write_reg(client, 0x332a, 0x58);
    ov2650_write_reg(client, 0x332b, 0x50);
    ov2650_write_reg(client, 0x332c, 0xbe);
    ov2650_write_reg(client, 0x332d, 0xe1);
    ov2650_write_reg(client, 0x332e, 0x43);
    ov2650_write_reg(client, 0x332f, 0x36);
    ov2650_write_reg(client, 0x3330, 0x4d);
    ov2650_write_reg(client, 0x3331, 0x44);
    ov2650_write_reg(client, 0x3332, 0xf8);
    ov2650_write_reg(client, 0x3333, 0x0a);
    ov2650_write_reg(client, 0x3334, 0xf0);
    ov2650_write_reg(client, 0x3335, 0xf0);
    ov2650_write_reg(client, 0x3336, 0xf0);
    ov2650_write_reg(client, 0x3337, 0x40);
    ov2650_write_reg(client, 0x3338, 0x40);
    ov2650_write_reg(client, 0x3339, 0x40);
    ov2650_write_reg(client, 0x333a, 0x00);
    ov2650_write_reg(client, 0x333b, 0x00);

    //Color Matrix
    ov2650_write_reg(client, 0x3380, 0x28);
    ov2650_write_reg(client, 0x3381, 0x48);
    ov2650_write_reg(client, 0x3382, 0x10);
    ov2650_write_reg(client, 0x3383, 0x23);
    ov2650_write_reg(client, 0x3384, 0xc0);
    ov2650_write_reg(client, 0x3385, 0xe5);
    ov2650_write_reg(client, 0x3386, 0xc2);
    ov2650_write_reg(client, 0x3387, 0xb3);
    ov2650_write_reg(client, 0x3388, 0x0e);
    ov2650_write_reg(client, 0x3389, 0x98);
    ov2650_write_reg(client, 0x338a, 0x01);

    //Gamma
    ov2650_write_reg(client, 0x3340, 0x0e);
    ov2650_write_reg(client, 0x3341, 0x1a);
    ov2650_write_reg(client, 0x3342, 0x31);
    ov2650_write_reg(client, 0x3343, 0x45);
    ov2650_write_reg(client, 0x3344, 0x5a);
    ov2650_write_reg(client, 0x3345, 0x69);
    ov2650_write_reg(client, 0x3346, 0x75);
    ov2650_write_reg(client, 0x3347, 0x7e);
    ov2650_write_reg(client, 0x3348, 0x88);
    ov2650_write_reg(client, 0x3349, 0x96);
    ov2650_write_reg(client, 0x334a, 0xa3);
    ov2650_write_reg(client, 0x334b, 0xaf);
    ov2650_write_reg(client, 0x334c, 0xc4);
    ov2650_write_reg(client, 0x334d, 0xd7);
    ov2650_write_reg(client, 0x334e, 0xe8);
    ov2650_write_reg(client, 0x334f, 0x20);

    //Lens correction
    ov2650_write_reg(client, 0x3350, 0x32);
    ov2650_write_reg(client, 0x3351, 0x25);
    ov2650_write_reg(client, 0x3352, 0x80);
    ov2650_write_reg(client, 0x3353, 0x1e);
    ov2650_write_reg(client, 0x3354, 0x00);
    ov2650_write_reg(client, 0x3355, 0x85);
    ov2650_write_reg(client, 0x3356, 0x32);
    ov2650_write_reg(client, 0x3357, 0x25);
    ov2650_write_reg(client, 0x3358, 0x80);
    ov2650_write_reg(client, 0x3359, 0x1b);
    ov2650_write_reg(client, 0x335a, 0x00);
    ov2650_write_reg(client, 0x335b, 0x85);
    ov2650_write_reg(client, 0x335c, 0x32);
    ov2650_write_reg(client, 0x335d, 0x25);
    ov2650_write_reg(client, 0x335e, 0x80);
    ov2650_write_reg(client, 0x335f, 0x1b);
    ov2650_write_reg(client, 0x3360, 0x00);
    ov2650_write_reg(client, 0x3361, 0x85);
    ov2650_write_reg(client, 0x3363, 0x70);
    ov2650_write_reg(client, 0x3364, 0x7f);
    ov2650_write_reg(client, 0x3365, 0x00);
    ov2650_write_reg(client, 0x3366, 0x00);

    //UVadjust
    ov2650_write_reg(client, 0x3301, 0xff);
    ov2650_write_reg(client, 0x338b, 0x11);
    ov2650_write_reg(client, 0x338c, 0x10);
    ov2650_write_reg(client, 0x338d, 0x40);

    //Sharpness/De-noise
    ov2650_write_reg(client, 0x3370, 0xd0); 
    ov2650_write_reg(client, 0x3371, 0x00);
    ov2650_write_reg(client, 0x3372, 0x00);
    ov2650_write_reg(client, 0x3373, 0x40);
    ov2650_write_reg(client, 0x3374, 0x10);
    ov2650_write_reg(client, 0x3375, 0x10);
    ov2650_write_reg(client, 0x3376, 0x04);
    ov2650_write_reg(client, 0x3377, 0x00);
    ov2650_write_reg(client, 0x3378, 0x04);
    ov2650_write_reg(client, 0x3379, 0x80);

    //BLC
    ov2650_write_reg(client, 0x3069, 0x84);
    ov2650_write_reg(client, 0x307c, 0x10);
    ov2650_write_reg(client, 0x3087, 0x02);

    //black sun
    //Avdd 2.55~3.0V
    ov2650_write_reg(client, 0x3090, 0x03);
    ov2650_write_reg(client, 0x30a8, 0x54);
    ov2650_write_reg(client, 0x30aa, 0x82);
    ov2650_write_reg(client, 0x30a3, 0x91);
    ov2650_write_reg(client, 0x30a1, 0x41);

    //Other functions
    ov2650_write_reg(client, 0x3300, 0xfc);
    ov2650_write_reg(client, 0x3302, 0x11);
    ov2650_write_reg(client, 0x3400, 0x03);
    ov2650_write_reg(client, 0x3606, 0x20);
    ov2650_write_reg(client, 0x3601, 0x30);
    ov2650_write_reg(client, 0x300e, 0x34);
    ov2650_write_reg(client, 0x3011, 0x00);
    ov2650_write_reg(client, 0x30f3, 0x83);
    ov2650_write_reg(client, 0x304e, 0x88);

    ov2650_write_reg(client, 0x363b, 0x01);
    ov2650_write_reg(client, 0x363c, 0xf2);
    ov2650_write_reg(client, 0x3085, 0x20);
    ov2650_write_reg(client, 0x3086, 0x0f);
    ov2650_write_reg(client, 0x3086, 0x00);

    dev_info(&client->dev, "sensor init ok\n");

	return 0;
}

int ov2650_preview_set(struct cim_sensor *sensor_info)                   
{                               
    mode = CAMERA_MODE_PREVIEW;
	return 0;
} 

static void change_rate(struct i2c_client *client)
{
    ov2650_write_reg(client, 0x300e, 0x38);
    ov2650_write_reg(client, 0x3011, 0x00);
    ov2650_write_reg(client, 0x302c, 0x00);
    ov2650_write_reg(client, 0x3071, 0x00);
    ov2650_write_reg(client, 0x3070, 0x5d);
    ov2650_write_reg(client, 0x301c, 0x0d);
    ov2650_write_reg(client, 0x3073, 0x00);
    ov2650_write_reg(client, 0x3072, 0x4e);
    ov2650_write_reg(client, 0x301d, 0x0f);
}

static uint32_t stop_ae(struct i2c_client *client)
{
	uint32_t Reg0x3013;
	Reg0x3013 = ov2650_read_reg(client,0x3013);
	Reg0x3013 = Reg0x3013 & 0xfa;
	ov2650_write_reg(client,0x3013,Reg0x3013);
	return 0;
}

static uint32_t start_ae(struct i2c_client *client)
{
	uint32_t Reg0x3013;
	Reg0x3013 = ov2650_read_reg(client,0x3013);
	Reg0x3013 = Reg0x3013 & 0x05;
	ov2650_write_reg(client,0x3013,Reg0x3013);
	return 0;
}

static uint32_t read_shutter(struct i2c_client *client)
{
	uint32_t Reg0x3002,Reg0x3003;
	Reg0x3002 = ov2650_read_reg(client,0x3002);
	Reg0x3003 = ov2650_read_reg(client,0x3003);
	return (Reg0x3002 << 8) + Reg0x3003;
}

static uint32_t read_exter_line(struct i2c_client *client)
{
	uint32_t Reg0x302d,Reg0x302e;
	Reg0x302d = ov2650_read_reg(client,0x302d);
	Reg0x302e = ov2650_read_reg(client,0x302e);
	return Reg0x302e + (Reg0x302d << 8);
}

static uint32_t read_gain(struct i2c_client *client)
{
	uint32_t Reg0x3000;
	Reg0x3000 = ov2650_read_reg(client,0x3000);
	//return (((Reg0x3000>>4) & 0x0f) + 1) * (16 + (Reg0x3000 & 0x0f));
	return (((Reg0x3000 & 0xf0) >> 4) + 1) * (16 + (Reg0x3000 & 0x0f));
}

static uint32_t read_dummy_pixels(struct i2c_client *client)
{
#define Default_Reg0x3028 0x07
#define Default_Reg0x3029 0x94
	uint8_t Reg0x3028,Reg0x3029;
	Reg0x3028 = ov2650_read_reg(client,0x3028);
	Reg0x3029 = ov2650_read_reg(client,0x3029);
	return (((Reg0x3028 - Default_Reg0x3028) & 0xf0)<<8) + Reg0x3029-Default_Reg0x3029; 
}

static void capture_set_size_1600_1200(struct i2c_client *client)
{
        ov2650_write_reg(client, 0x301d, 0x0f);
        ov2650_write_reg(client, 0x300e, 0x34);
        ov2650_write_reg(client, 0x3011, 0x01);
        ov2650_write_reg(client, 0x3012, 0x00);
        ov2650_write_reg(client, 0x302A, 0x04);
        ov2650_write_reg(client, 0x302B, 0xd4);
        ov2650_write_reg(client, 0x306f, 0x54);
        ov2650_write_reg(client, 0x3020, 0x01);
        ov2650_write_reg(client, 0x3021, 0x18);
        ov2650_write_reg(client, 0x3022, 0x00);
        ov2650_write_reg(client, 0x3023, 0x0a);
        ov2650_write_reg(client, 0x3024, 0x06);
        ov2650_write_reg(client, 0x3025, 0x58);
        ov2650_write_reg(client, 0x3026, 0x04);
        ov2650_write_reg(client, 0x3027, 0xbc);
        ov2650_write_reg(client, 0x3088, 0x06);
        ov2650_write_reg(client, 0x3089, 0x40);
        ov2650_write_reg(client, 0x308a, 0x04);
        ov2650_write_reg(client, 0x308b, 0xb0);
        ov2650_write_reg(client, 0x3316, 0x64);
        ov2650_write_reg(client, 0x3317, 0x4b);
        ov2650_write_reg(client, 0x3318, 0x00);
        ov2650_write_reg(client, 0x3319, 0x2c);
        ov2650_write_reg(client, 0x331a, 0x64);
        ov2650_write_reg(client, 0x331b, 0x4b);
        ov2650_write_reg(client, 0x331c, 0x00);
        ov2650_write_reg(client, 0x331d, 0x4c);
        ov2650_write_reg(client, 0x3302, 0x01);
}

static void ov2650_dcw_size_switch(struct i2c_client *client, int width, int height)
{
    unsigned char isp_x_lo = width & 0xff;
    unsigned char isp_x_hi = (width & 0xff00) >> 8;
    unsigned char isp_y_lo = height & 0xff;
    unsigned char isp_y_hi = (height & 0xff00) >> 8;

    unsigned char dsp_x_lo = width & 0xf;
    unsigned char dsp_x_hi = (width & 0xef0) >> 4;
    unsigned char dsp_y_lo = height & 0xf;
    unsigned char dsp_y_hi = (height & 0xef0) >> 4;

    ov2650_write_reg(client,0x3302,0x11);
    ov2650_write_reg(client,0x3088, isp_x_hi);
    ov2650_write_reg(client,0x3089, isp_x_lo);
    ov2650_write_reg(client,0x308a, isp_y_hi);
    ov2650_write_reg(client,0x308b, isp_y_lo);
    ov2650_write_reg(client,0x331a, dsp_x_hi);
    ov2650_write_reg(client,0x331b, dsp_y_hi);
    ov2650_write_reg(client,0x331c, (dsp_y_lo >> 4) | dsp_x_lo);
    ov2650_write_reg(client,0x3302,0x11);
}

static void preview_set_size_800_600(struct i2c_client *client)
{
        ov2650_write_reg(client, 0x300e, 0x34);
        ov2650_write_reg(client, 0x3011, 0x00);
        ov2650_write_reg(client, 0x3012, 0x10);
        ov2650_write_reg(client, 0x302A, 0x02);
        ov2650_write_reg(client, 0x302B, 0x6a);
        ov2650_write_reg(client, 0x306f, 0x14);
        ov2650_write_reg(client, 0x3020, 0x01);
        ov2650_write_reg(client, 0x3021, 0x18);
        ov2650_write_reg(client, 0x3022, 0x00);
        ov2650_write_reg(client, 0x3023, 0x06);
        ov2650_write_reg(client, 0x3024, 0x06);
        ov2650_write_reg(client, 0x3025, 0x58);
        ov2650_write_reg(client, 0x3026, 0x02);
        ov2650_write_reg(client, 0x3027, 0x61);
        ov2650_write_reg(client, 0x3088, 0x03);
        ov2650_write_reg(client, 0x3089, 0x20);
        ov2650_write_reg(client, 0x308a, 0x02);
        ov2650_write_reg(client, 0x308b, 0x58);
        ov2650_write_reg(client, 0x3316, 0x64);
        ov2650_write_reg(client, 0x3317, 0x25);
        ov2650_write_reg(client, 0x3318, 0x80);
        ov2650_write_reg(client, 0x3319, 0x08);
        ov2650_write_reg(client, 0x331a, 0x64);
        ov2650_write_reg(client, 0x331b, 0x4b);
        ov2650_write_reg(client, 0x331c, 0x00);
        ov2650_write_reg(client, 0x331d, 0x38);
        ov2650_write_reg(client, 0x3302, 0x11);
}

void ov2650_capture_size_switch(struct i2c_client *client, int width, int height)
{
    capture_set_size_1600_1200(client);
    if ((width == 1280 && height == 1024) ||
            (width == 1280 && height == 960) ||
            (width == 1024 && height == 768) ||
            (width == 800 && height == 600) ||
            (width == 640 && height == 480) ||
            (width == 352 && height == 288) ||
            (width == 320 && height == 240) ||
            (width == 176 && height == 144)) {

        ov2650_dcw_size_switch(client, width, height);
    } else if (width != 1600 && height != 1200) {
        dev_err(&client->dev,"do not support size(%d, %d)\n", width, height); 
    }
}

void init_capture_and_set_size(struct i2c_client *client, int width, int height)
{
	uint32_t Shutter,
		 Extra_lines,

		 Preview_Gain16,
		 Preview_Exposure,
		 Preview_dummy_pixel,
		 Preview_Line_width,
		 Preview_PCLK_Frequency,

		 Capture_dummy_pixel,
		 Capture_dummy_line,
		 Capture_PCLK_Frequency,
		 Capture_Line_Width,
		 Capture_Maximum_Shutter,
		 Capture_Exposure,

		 Capture_Gain,
		 Capture_Banding_Filter,

		 Default_SVGA_Line_Width = 1940,
		 Default_UXGA_Line_Width = 1940,
		 Default_Capture_Max_Gain16 = 31 * 16,
		 Default_UXGA_maximum_shutter = 1236,
		 Default_Reg0x302b = 0xd4,
		 Default_Reg0x302a = 0x04,

		 Reg0x3029,
		 Reg0x3028,
		 Reg0x302b,
		 Reg0x302a,
		 Reg0x3003,
		 Reg0x3002,
		 Reg0x302e,
		 Reg0x302d,

		 Gain_Exposure;

    stop_ae(client);
    change_rate(client);
	Shutter = read_shutter(client);
	Extra_lines = read_exter_line(client); 
	Preview_Gain16 = read_gain(client);

	Preview_dummy_pixel = read_dummy_pixels(client); 
	//printk("read_exter_line : %u\n",Extra_lines);
	//printk("read_dummy_pixels : %u\n\n",Preview_dummy_pixel); 

	//printk("read_gain : %u\n",Preview_Gain16);
	//printk("read_shutter : %u\n",Shutter);

	Preview_Exposure = Shutter + Extra_lines;

	Capture_dummy_pixel = 0;
	Capture_dummy_line = 0;
	Preview_PCLK_Frequency = 18;
	Capture_PCLK_Frequency = 36;
    
	Preview_Line_width = Default_SVGA_Line_Width + Preview_dummy_pixel ;
	Capture_Line_Width = Default_UXGA_Line_Width + Capture_dummy_pixel;
	Capture_Maximum_Shutter = Default_UXGA_maximum_shutter + Capture_dummy_line;

	Capture_Exposure = 
		Preview_Exposure * Capture_PCLK_Frequency/Preview_PCLK_Frequency *
		Preview_Line_width/Capture_Line_Width;

	//Capture_Banding_Filter = Capture_PCLK_Frequency * 5000/ (Capture_Line_Width);
	Capture_Banding_Filter = Capture_PCLK_Frequency/100/( 2 * Capture_Line_Width);
	//Capture_Banding_Filter = Capture_PCLK_Frequency * 1000000 / 120 /(2*Capture_Line_Width);

	Gain_Exposure = Preview_Gain16 * Capture_Exposure;

	//printk("Capture_Exposure : %u\n",Capture_Exposure);
	//printk("Gain_Exposure : %u\n",Gain_Exposure);
	//printk("Capture_Banding_Filter : %u | %u\n",Capture_Banding_Filter,Capture_Banding_Filter * 16);
	if (Gain_Exposure < Capture_Banding_Filter * 16)
	{
		// Exposure < 1/100
		Capture_Exposure = Gain_Exposure /16;
		Capture_Gain = (Gain_Exposure*2 + 1)/Capture_Exposure/2;
	}
	else
	{
		if (Gain_Exposure > Capture_Maximum_Shutter * 16)
		{
			// // Exposure > Capture_Maximum_Shutter
			Capture_Exposure = Capture_Maximum_Shutter;
			Capture_Gain = (Gain_Exposure*2 + 1)/Capture_Maximum_Shutter/2;
			if (Capture_Gain > Default_Capture_Max_Gain16)
			{
				// // gain reach maximum, insert extra line
				Capture_Exposure = Gain_Exposure*1.1/Default_Capture_Max_Gain16;
				// For 50Hz, Exposure = n/100; For 60Hz, Exposure = n/120
				Capture_Exposure = Gain_Exposure/16/Capture_Banding_Filter;
				Capture_Exposure = Capture_Exposure * Capture_Banding_Filter;
				Capture_Gain = (Gain_Exposure *2+1)/ Capture_Exposure/2;
			}
			else
			{
				Capture_Exposure = Capture_Exposure/Capture_Banding_Filter;
				Capture_Exposure = Capture_Exposure * Capture_Banding_Filter;
				Capture_Gain = (Gain_Exposure*2 + 1)/Capture_Maximum_Shutter/2;
			}
		}
		else
		{
			//1/100(120) < Exposure < Capture_Maximum_Shutter, Exposure = n/100(120)
			Capture_Exposure = Gain_Exposure/16/Capture_Banding_Filter;
			Capture_Exposure = Capture_Exposure * Capture_Banding_Filter;
			Capture_Gain = (Gain_Exposure*2 +1) / Capture_Exposure/2;
		}
	}

    ov2650_capture_size_switch(client, width, height);

	//write dummy pixels
	Reg0x3029 = Capture_dummy_pixel & 0x00ff;
	Reg0x3028 = ov2650_read_reg(client,0x3028);
	Reg0x3028 = (Reg0x3028 & 0x0f) | ((Capture_dummy_pixel & 0x0f00)>>4);
	ov2650_write_reg(client,0x3028, Reg0x3028);
	ov2650_write_reg(client,0x302b, Reg0x3029);
	//printk("dummy pixels\n");
	//printk("Reg0x3028 : %x\n",Reg0x3028);
	printk("Reg0x3029 : %x\n",Reg0x3029);
	//printk("\n");
	////Write Dummy Lines
	Reg0x302b = (Capture_dummy_line & 0x00ff) + Default_Reg0x302b;
	Reg0x302a = (Capture_dummy_line >>8) + Default_Reg0x302a;
	ov2650_write_reg(client,0x302a, Reg0x302a);
	ov2650_write_reg(client,0x302b, Reg0x302b);
	//printk("dummy lines\n");
	//printk("Reg0x302a : %x\n",Reg0x302a);
	//printk("Reg0x302b : %x\n",Reg0x302b);
	//printk("\n");
    
	////Write Exposure
	if (Capture_Exposure > Capture_Maximum_Shutter)
	{
		Shutter = Capture_Maximum_Shutter;
		Extra_lines = Capture_Exposure - Capture_Maximum_Shutter;
	}
	else
	{
		Shutter = Capture_Exposure;
		Extra_lines = 0;
	}

	Reg0x3003 = Shutter & 0x00ff;
	Reg0x3002 = (Shutter >>8) & 0x00ff;
	ov2650_write_reg(client,0x3003, Reg0x3003);
	ov2650_write_reg(client,0x3002, Reg0x3002);
	//printk("Exposure\n");
	//printk("Reg0x3003 : %x\n",Reg0x3003);
	//printk("Reg0x3002 : %x\n",Reg0x3002);
	//printk("\n");
	//// Write extra line
	Reg0x302e = Extra_lines & 0x00ff;
	Reg0x302d = Extra_lines >> 8;
	ov2650_write_reg(client,0x302d, Reg0x302d);
	ov2650_write_reg(client,0x302e, Reg0x302e);
	//printk("extra line\n");
	//printk("Reg0x302d : %x\n",Reg0x302d);
	//printk("Reg0x302e : %x\n",Reg0x302e);
	//printk("\n");

	// Write Gain
	uint8_t Gain = Capture_Gain & 0x0f;
	dev_info(&client->dev, "Capture_Gain : %x\n",Capture_Gain);
	dev_info(&client->dev, "gain : %x\n",Gain);

    Gain = 0x0;
	if (Capture_Gain > 16) 
	{ 
		Capture_Gain = Capture_Gain /2; 
		Gain = 0x10; 
	} 
	if (Capture_Gain > 16) 
	{ 
		Capture_Gain = Capture_Gain /2; 
		Gain = Gain| 0x20; 
	} 
	if (Capture_Gain > 16) 
	{ 
		Capture_Gain = Capture_Gain /2; 
		Gain = Gain | 0x40; 
	} 
	if (Capture_Gain > 16) 
	{ 
		Capture_Gain = Capture_Gain /2; 
		Gain = Gain | 0x80; 
	} 

    Gain = Gain | ((uint32_t)Capture_Gain -16); 

	ov2650_write_reg(client,0x3000, Gain);
	dev_info(&client->dev, "Reg0x3000 : %x\n",Gain);
}


int ov2650_size_switch(struct cim_sensor *sensor_info,int width,int height)
{	
	struct ov2650_sensor *s;
	struct i2c_client *client;
	s = container_of(sensor_info, struct ov2650_sensor, cs);
	client = s->client;
	dev_info(&client->dev,"ov2650-mode = %d--size switch (%d, %d) -> (%d, %d)\n", 
            mode, g_width, g_height, width, height);
    g_width = width;
    g_height = height;

    if (mode == CAMERA_MODE_PREVIEW) {
        preview_set_size_800_600(client);
        if ((width == 640 && height == 480) ||
                (width == 352 && height == 288) ||
                (width == 320 && height == 240) ||
                (width == 176 && height == 144)) {
            ov2650_dcw_size_switch(client, width, height);
        } else if (!(width == 800 && height == 600)){
            dev_err(&client->dev,"do not support size(%d, %d)\n", width, height); 
        }
        start_ae(client);
    } else {
       init_capture_and_set_size(client, width, height); 
    }

	return 0;
}

int ov2650_capture_set(struct cim_sensor *sensor_info)
{
    mode = CAMERA_MODE_CAPTURE;

	return 0;
}

void ov2650_set_ab_50hz(struct i2c_client *client)
{
#if 0
	int temp = ov2650_read_reg(client,0x3b);
	ov2650_write_reg(client,0x3b,temp|0x08);	    /* 50 Hz */	
	ov2650_write_reg(client,0x9d,0x4c);  
	ov2650_write_reg(client,0xa5,0x06);  
#endif
}

void ov2650_set_ab_60hz(struct i2c_client *client)
{
#if 0
	int temp = ov2650_read_reg(client,0x3b);
	ov2650_write_reg(client,0x3b,temp&0xf7);	    /* 60 Hz */
	ov2650_write_reg(client,0x9e,0x3f);  
	ov2650_write_reg(client,0xab,0x07);  
#endif
}

int ov2650_set_antibanding(struct cim_sensor *sensor_info,unsigned short arg)
{
	struct ov2650_sensor *s;
	struct i2c_client * client ;
	s = container_of(sensor_info, struct ov2650_sensor, cs);
	client = s->client;
	dev_info(&client->dev,"ov2650_set_antibanding");
	switch(arg)
	{
		case ANTIBANDING_AUTO :
			ov2650_set_ab_50hz(client);
			dev_info(&client->dev,"ANTIBANDING_AUTO ");
			break;
		case ANTIBANDING_50HZ :
			ov2650_set_ab_50hz(client);
			dev_info(&client->dev,"ANTIBANDING_50HZ ");
			break;
		case ANTIBANDING_60HZ :
			ov2650_set_ab_60hz(client);
			dev_info(&client->dev,"ANTIBANDING_60HZ ");
			break;
		case ANTIBANDING_OFF :
			dev_info(&client->dev,"ANTIBANDING_OFF ");
			break;
	}
	return 0;
}
void ov2650_set_effect_normal(struct i2c_client *client)
{
   ov2650_write_reg(client, 0x3391, 0x00);  
}

void ov2650_set_effect_grayscale(struct i2c_client *client)
{
}

void ov2650_set_effect_sepia(struct i2c_client *client)
{
   ov2650_write_reg(client, 0x3391, 0x00);  
   ov2650_write_reg(client, 0x3396, 0x40);  
   ov2650_write_reg(client, 0x3397, 0xA6);  
}

void ov2650_set_effect_colorinv(struct i2c_client *client)
{
}

void ov2650_set_effect_sepiagreen(struct i2c_client *client)
{
   ov2650_write_reg(client, 0x3391, 0x00);  
   ov2650_write_reg(client, 0x3396, 0x60);  
   ov2650_write_reg(client, 0x3397, 0x60);  
}

void ov2650_set_effect_sepiablue(struct i2c_client *client)
{
}


int ov2650_set_effect(struct cim_sensor *sensor_info,unsigned short arg)
{
	struct ov2650_sensor *s;
	struct i2c_client * client ;
	s = container_of(sensor_info, struct ov2650_sensor, cs);
	client = s->client;
	dev_info(&client->dev,"ov2650_set_effect");
		switch(arg)
		{
			case EFFECT_NONE:
				ov2650_set_effect_normal(client);
				dev_info(&client->dev,"EFFECT_NONE");
				break;
			case EFFECT_MONO :
				ov2650_set_effect_grayscale(client);  
				dev_info(&client->dev,"EFFECT_MONO ");
				break;
			case EFFECT_NEGATIVE :
				ov2650_set_effect_colorinv(client);
				dev_info(&client->dev,"EFFECT_NEGATIVE ");
				break;
			case EFFECT_SOLARIZE ://bao guang
				dev_info(&client->dev,"EFFECT_SOLARIZE ");
				break;
			case EFFECT_SEPIA :
				ov2650_set_effect_sepia(client);
				dev_info(&client->dev,"EFFECT_SEPIA ");
				break;
			case EFFECT_POSTERIZE ://se diao fen li
				dev_info(&client->dev,"EFFECT_POSTERIZE ");
				break;
			case EFFECT_WHITEBOARD :
				dev_info(&client->dev,"EFFECT_WHITEBOARD ");
				break;
			case EFFECT_BLACKBOARD :
				dev_info(&client->dev,"EFFECT_BLACKBOARD ");
				break;
			case EFFECT_AQUA  ://qian lv se
				ov2650_set_effect_sepiagreen(client);
				dev_info(&client->dev,"EFFECT_AQUA  ");
				break;
			case EFFECT_PASTEL:
				dev_info(&client->dev,"EFFECT_PASTEL");
				break;
			case EFFECT_MOSAIC:
				dev_info(&client->dev,"EFFECT_MOSAIC");
				break;
			case EFFECT_RESIZE:
				dev_info(&client->dev,"EFFECT_RESIZE");
				break;
		}
		return 0;
}

void ov2650_set_wb_auto(struct i2c_client *client)
{       
	ov2650_write_reg(client,0x3306,0x00);   // select Auto WB
}

void ov2650_set_wb_cloud(struct i2c_client *client)
{       
	ov2650_write_reg(client,0x3306, 0x82);  // Disable Auto WB, select Manual WB
	ov2650_write_reg(client,0x3337, 0x68); //manual R G B
	ov2650_write_reg(client,0x3338, 0x40);
	ov2650_write_reg(client,0x3339, 0x4e);
}

void ov2650_set_wb_daylight(struct i2c_client *client)
{              
	ov2650_write_reg(client,0x3306, 0x02);  // Disable Auto WB, select Manual WB
	ov2650_write_reg(client,0x3337, 0x5e);
	ov2650_write_reg(client,0x3338, 0x40);
	ov2650_write_reg(client,0x3339, 0x46);
}


void ov2650_set_wb_incandescence(struct i2c_client *client)
{    
	ov2650_write_reg(client,0x3306, 0x02);  // Disable Auto WB, select Manual WB
	ov2650_write_reg(client,0x3337, 0x52);
	ov2650_write_reg(client,0x3338, 0x40);
	ov2650_write_reg(client,0x3339, 0x58);
}

void ov2650_set_wb_tungsten(struct i2c_client *client)
{   
}

int ov2650_set_balance(struct cim_sensor *sensor_info,unsigned short arg)
{
	struct ov2650_sensor *s;
	struct i2c_client * client ;
	s = container_of(sensor_info, struct ov2650_sensor, cs);
	client = s->client;
	
	dev_info(&client->dev,"ov2650_set_balance");
	switch(arg)
	{
		case WHITE_BALANCE_AUTO:
			ov2650_set_wb_auto(client);
			dev_info(&client->dev,"WHITE_BALANCE_AUTO");
			break;
		case WHITE_BALANCE_INCANDESCENT :
			ov2650_set_wb_incandescence(client);
			dev_info(&client->dev,"WHITE_BALANCE_INCANDESCENT ");
			break;
		case WHITE_BALANCE_FLUORESCENT ://ying guang
			//ov2650_set_wb_fluorescent(client);
			dev_info(&client->dev,"WHITE_BALANCE_FLUORESCENT ");
			break;
		case WHITE_BALANCE_WARM_FLUORESCENT :
			dev_info(&client->dev,"WHITE_BALANCE_WARM_FLUORESCENT ");
			break;
		case WHITE_BALANCE_DAYLIGHT ://ri guang
			ov2650_set_wb_daylight(client);
			dev_info(&client->dev,"WHITE_BALANCE_DAYLIGHT ");
			break;
		case WHITE_BALANCE_CLOUDY_DAYLIGHT ://ying tian
			ov2650_set_wb_cloud(client);
			dev_info(&client->dev,"WHITE_BALANCE_CLOUDY_DAYLIGHT ");
			break;
		case WHITE_BALANCE_TWILIGHT :
			dev_info(&client->dev,"WHITE_BALANCE_TWILIGHT ");
			break;
		case WHITE_BALANCE_SHADE :
			dev_info(&client->dev,"WHITE_BALANCE_SHADE ");
			break;
	}

	return 0;
}
