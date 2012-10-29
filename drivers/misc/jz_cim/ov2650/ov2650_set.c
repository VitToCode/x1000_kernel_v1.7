#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <mach/jz_cim.h>
#include "ov2650_camera.h"



int ov2650_init(struct cim_sensor *sensor_info)
{
	struct ov2650_sensor *s;
	struct i2c_client * client ;
	s = container_of(sensor_info, struct ov2650_sensor, cs);
	client = s->client;
	/***************** init reg set **************************/
	/*** VGA preview (640X480) 30fps 24MCLK input ***********/

	dev_info(&client->dev,"ov2650 -----------------------------------init\n");
    ov2650_write_reg(client,0x3012,0x80);
    mdelay(10);
    
    //IO & Clock & Analog Setup    
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
    ov2650_write_reg(client, 0x30d9, 0x8c);
    ov2650_write_reg(client, 0x3016, 0x82);
    ov2650_write_reg(client, 0x3601, 0x30);
    ov2650_write_reg(client, 0x304e, 0x88);
    ov2650_write_reg(client, 0x30f1, 0x82);
    ov2650_write_reg(client, 0x306f, 0x14);

    ov2650_write_reg(client, 0x3012, 0x10);
    ov2650_write_reg(client, 0x3011, 0x01);
    ov2650_write_reg(client, 0x302A, 0x02);
    ov2650_write_reg(client, 0x302B, 0xE6);
    ov2650_write_reg(client, 0x3028, 0x07);
    ov2650_write_reg(client, 0x3029, 0x93);

    //saturation
    ov2650_write_reg(client, 0x3391, 0x06);
    ov2650_write_reg(client, 0x3394, 0x38);
    ov2650_write_reg(client, 0x3395, 0x38);
    //auto frame
    ov2650_write_reg(client, 0x3015, 0x22);
    ov2650_write_reg(client, 0x302d, 0x00);
    ov2650_write_reg(client, 0x302e, 0x00);
    //AEC/AGC
    ov2650_write_reg(client, 0x3013, 0xf7);
    ov2650_write_reg(client, 0x3018, 0x78);
    ov2650_write_reg(client, 0x3019, 0x68);
    ov2650_write_reg(client, 0x301a, 0xd4);

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
    ov2650_write_reg(client, 0x3014, 0x8c);
    ov2650_write_reg(client, 0x3071, 0x00);
    ov2650_write_reg(client, 0x3070, 0x5d);
    ov2650_write_reg(client, 0x3073, 0x00);
    ov2650_write_reg(client, 0x3072, 0x5d);
    ov2650_write_reg(client, 0x301c, 0x07);
    ov2650_write_reg(client, 0x301d, 0x07);
    ov2650_write_reg(client, 0x304d, 0x42);   
    ov2650_write_reg(client, 0x304a, 0x40);
    ov2650_write_reg(client, 0x304f, 0x40);
    ov2650_write_reg(client, 0x3095, 0x07);
    ov2650_write_reg(client, 0x3096, 0x16);
    ov2650_write_reg(client, 0x3097, 0x1d);

    //Window Setup
    ov2650_write_reg(client, 0x3020, 0x01);
    ov2650_write_reg(client, 0x3021, 0x18);
    ov2650_write_reg(client, 0x3022, 0x00);
    ov2650_write_reg(client, 0x3023, 0x06);
    ov2650_write_reg(client, 0x3024, 0x06);
    ov2650_write_reg(client, 0x3025, 0x58);
    ov2650_write_reg(client, 0x3026, 0x02);
    ov2650_write_reg(client, 0x3027, 0x61);
    ov2650_write_reg(client, 0x3088, 0x02);
    ov2650_write_reg(client, 0x3089, 0x80);
    ov2650_write_reg(client, 0x308a, 0x01);
    ov2650_write_reg(client, 0x308b, 0xe0);
    ov2650_write_reg(client, 0x3316, 0x64);
    ov2650_write_reg(client, 0x3317, 0x25);
    ov2650_write_reg(client, 0x3318, 0x80);
    ov2650_write_reg(client, 0x3319, 0x08);
    ov2650_write_reg(client, 0x331a, 0x28);
    ov2650_write_reg(client, 0x331b, 0x1e);
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
    ov2650_write_reg(client, 0x332b, 0x56);
    ov2650_write_reg(client, 0x332c, 0xbe);
    ov2650_write_reg(client, 0x332d, 0xe1);
    ov2650_write_reg(client, 0x332e, 0x3a);
    ov2650_write_reg(client, 0x332f, 0x38);
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
    ov2650_write_reg(client, 0x3383, 0x22);
    ov2650_write_reg(client, 0x3384, 0xc0);
    ov2650_write_reg(client, 0x3385, 0xe2);
    ov2650_write_reg(client, 0x3386, 0xe2);
    ov2650_write_reg(client, 0x3387, 0xf2);
    ov2650_write_reg(client, 0x3388, 0x10);
    ov2650_write_reg(client, 0x3389, 0x98);
    ov2650_write_reg(client, 0x338a, 0x00);

    //Gamma
    ov2650_write_reg(client, 0x3340, 0x04);
    ov2650_write_reg(client, 0x3341, 0x07);
    ov2650_write_reg(client, 0x3342, 0x19);
    ov2650_write_reg(client, 0x3343, 0x34);
    ov2650_write_reg(client, 0x3344, 0x4a);
    ov2650_write_reg(client, 0x3345, 0x5a);
    ov2650_write_reg(client, 0x3346, 0x67);
    ov2650_write_reg(client, 0x3347, 0x71);
    ov2650_write_reg(client, 0x3348, 0x7c);
    ov2650_write_reg(client, 0x3349, 0x8c);
    ov2650_write_reg(client, 0x334a, 0x9b);
    ov2650_write_reg(client, 0x334b, 0xa9);
    ov2650_write_reg(client, 0x334c, 0xc0);
    ov2650_write_reg(client, 0x334d, 0xd5);
    ov2650_write_reg(client, 0x334e, 0xe8);
    ov2650_write_reg(client, 0x334f, 0x20);

    //Lens correction
    ov2650_write_reg(client, 0x3350, 0x33);
    ov2650_write_reg(client, 0x3351, 0x28);
    ov2650_write_reg(client, 0x3352, 0x00);
    ov2650_write_reg(client, 0x3353, 0x14);
    ov2650_write_reg(client, 0x3354, 0x00);
    ov2650_write_reg(client, 0x3355, 0x85);
    ov2650_write_reg(client, 0x3356, 0x35);
    ov2650_write_reg(client, 0x3357, 0x28);
    ov2650_write_reg(client, 0x3358, 0x00);
    ov2650_write_reg(client, 0x3359, 0x13);
    ov2650_write_reg(client, 0x335a, 0x00);
    ov2650_write_reg(client, 0x335b, 0x85);
    ov2650_write_reg(client, 0x335c, 0x34);
    ov2650_write_reg(client, 0x335d, 0x28);
    ov2650_write_reg(client, 0x335e, 0x00);
    ov2650_write_reg(client, 0x335f, 0x13);
    ov2650_write_reg(client, 0x3360, 0x00);
    ov2650_write_reg(client, 0x3361, 0x85);
    ov2650_write_reg(client, 0x3363, 0x70);
    ov2650_write_reg(client, 0x3364, 0x7f);
    ov2650_write_reg(client, 0x3365, 0x00);
    ov2650_write_reg(client, 0x3366, 0x00);

    ov2650_write_reg(client, 0x3362, 0x90);

    //UVadjust
    ov2650_write_reg(client, 0x3301, 0xff);
    ov2650_write_reg(client, 0x338B, 0x14);
    ov2650_write_reg(client, 0x338c, 0x10);
    ov2650_write_reg(client, 0x338d, 0x40);

    //Sharpness/De-noise
    ov2650_write_reg(client, 0x3370, 0xd0);
    ov2650_write_reg(client, 0x3371, 0x00);
    ov2650_write_reg(client, 0x3372, 0x00);
    ov2650_write_reg(client, 0x3373, 0x30);
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

    //Other functions
    ov2650_write_reg(client, 0x3300, 0xfc);
    ov2650_write_reg(client, 0x3302, 0x11);
    ov2650_write_reg(client, 0x3400, 0x00);
    ov2650_write_reg(client, 0x3606, 0x20);
    ov2650_write_reg(client, 0x3601, 0x30);
    ov2650_write_reg(client, 0x30f3, 0x83);
    ov2650_write_reg(client, 0x304e, 0x88);

    ov2650_write_reg(client, 0x30aa, 0x72);
    ov2650_write_reg(client, 0x30a3, 0x80);
    ov2650_write_reg(client, 0x30a1, 0x41);
    ov2650_write_reg(client, 0x3086, 0x0f);
    ov2650_write_reg(client, 0x3086, 0x00);

	return 0;
}

int ov2650_preview_set(struct cim_sensor *sensor_info)                   
{                               

	struct ov2650_sensor *s;
	struct i2c_client * client ;
	s = container_of(sensor_info, struct ov2650_sensor, cs);
	client = s->client;
	/***************** preview reg set **************************/
	return 0;
} 


int ov2650_size_switch(struct cim_sensor *sensor_info,int width,int height)
{	
	struct ov2650_sensor *s;
	struct i2c_client * client ;
	s = container_of(sensor_info, struct ov2650_sensor, cs);
	client = s->client;
	dev_info(&client->dev,"ov2650-----------------size switch %d * %d\n",width,height);
	
	if(width == 1600 && height == 1200) 
    {
        ov2650_write_reg(client, 0x3013, 0xf2);
        ov2650_write_reg(client, 0x3014, 0x84);
        ov2650_write_reg(client, 0x3015, 0x01);
        ov2650_write_reg(client, 0x3012, 0x00);
        ov2650_write_reg(client, 0x302a, 0x05);
        ov2650_write_reg(client, 0x302b, 0xCB);
        ov2650_write_reg(client, 0x306f, 0x54);
        ov2650_write_reg(client, 0x3362, 0x80);
        ov2650_write_reg(client, 0x3070, 0x5d);
        ov2650_write_reg(client, 0x3072, 0x5d);
        ov2650_write_reg(client, 0x301c, 0x0f);
        ov2650_write_reg(client, 0x301d, 0x0f);
        ov2650_write_reg(client, 0x3020, 0x01);
        ov2650_write_reg(client, 0x3021, 0x18);
        ov2650_write_reg(client, 0x3022, 0x00);
        ov2650_write_reg(client, 0x3023, 0x0A);
        ov2650_write_reg(client, 0x3024, 0x06);
        ov2650_write_reg(client, 0x3025, 0x58);
        ov2650_write_reg(client, 0x3026, 0x04);
        ov2650_write_reg(client, 0x3027, 0xbc);
        ov2650_write_reg(client, 0x3088, 0x06);
        ov2650_write_reg(client, 0x3089, 0x40);
        ov2650_write_reg(client, 0x308A, 0x04);
        ov2650_write_reg(client, 0x308B, 0xB0);
        ov2650_write_reg(client, 0x3316, 0x64);
        ov2650_write_reg(client, 0x3317, 0x4B);
        ov2650_write_reg(client, 0x3318, 0x00);
        ov2650_write_reg(client, 0x3319, 0x6C);
        ov2650_write_reg(client, 0x331A, 0x64);
        ov2650_write_reg(client, 0x331B, 0x4B);
        ov2650_write_reg(client, 0x331C, 0x00);
        ov2650_write_reg(client, 0x331D, 0x6C);
        ov2650_write_reg(client, 0x3302, 0x01);
    }
    else if(width == 640 && height == 480)
    {
        ov2650_write_reg(client, 0x3012, 0x10);
        ov2650_write_reg(client, 0x302a, 0x02);
        ov2650_write_reg(client, 0x302b, 0xE6);
        ov2650_write_reg(client, 0x306f, 0x14);
        ov2650_write_reg(client, 0x3362, 0x90);
        ov2650_write_reg(client, 0x3070, 0x5D);
        ov2650_write_reg(client, 0x3072, 0x5D);
        ov2650_write_reg(client, 0x301c, 0x07);
        ov2650_write_reg(client, 0x301d, 0x07);
        ov2650_write_reg(client, 0x3020, 0x01);
        ov2650_write_reg(client, 0x3021, 0x18);
        ov2650_write_reg(client, 0x3022, 0x00);
        ov2650_write_reg(client, 0x3023, 0x06);
        ov2650_write_reg(client, 0x3024, 0x06);
        ov2650_write_reg(client, 0x3025, 0x58);
        ov2650_write_reg(client, 0x3026, 0x02);
        ov2650_write_reg(client, 0x3027, 0x61);
        ov2650_write_reg(client, 0x3088, 0x02);
        ov2650_write_reg(client, 0x3089, 0x80);
        ov2650_write_reg(client, 0x308A, 0x01);
        ov2650_write_reg(client, 0x308B, 0xe0);
        ov2650_write_reg(client, 0x3316, 0x64);
        ov2650_write_reg(client, 0x3317, 0x25);
        ov2650_write_reg(client, 0x3318, 0x80);
        ov2650_write_reg(client, 0x3319, 0x08);
        ov2650_write_reg(client, 0x331A, 0x28);
        ov2650_write_reg(client, 0x331B, 0x1e);
        ov2650_write_reg(client, 0x331C, 0x00);
        ov2650_write_reg(client, 0x331D, 0x38);
        ov2650_write_reg(client, 0x3302, 0x11);
    }
	else if(width == 352 && height == 288)
	{

	}
	else if(width == 176 && height == 144)
	{
	}
	else if(width == 320 && height == 240)
	{
	}
	else
		return 0;

	
	return 0;
}



int ov2650_capture_set(struct cim_sensor *sensor_info)
{
	
	struct ov2650_sensor *s;
	struct i2c_client * client ;
	s = container_of(sensor_info, struct ov2650_sensor, cs);
	client = s->client;
	/***************** capture reg set **************************/

	dev_info(&client->dev,"------------------------------------capture\n");
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
#if 0
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
#endif
	return 0;
}
void ov2650_set_effect_normal(struct i2c_client *client)
{
   ov2650_write_reg(client,0x3A,0x0C);  
   ov2650_write_reg(client,0x67,0x80);  
   ov2650_write_reg(client,0x68,0x80);
   ov2650_write_reg(client,0x56,0x40);  

}

void ov2650_set_effect_grayscale(struct i2c_client *client)
{
   ov2650_write_reg(client,0x3A,0x1C);  
   ov2650_write_reg(client,0x67,0x80);  
   ov2650_write_reg(client,0x68,0x80);
   ov2650_write_reg(client,0x56,0x40); 
}

void ov2650_set_effect_sepia(struct i2c_client *client)
{
   ov2650_write_reg(client,0x3A,0x1C);  
   ov2650_write_reg(client,0x67,0x40);  
   ov2650_write_reg(client,0x68,0xa0);
   ov2650_write_reg(client,0x56,0x40);  
}

void ov2650_set_effect_colorinv(struct i2c_client *client)
{
   ov2650_write_reg(client,0x3A,0x2C);  
   ov2650_write_reg(client,0x67,0x80);  
   ov2650_write_reg(client,0x68,0x80);
   ov2650_write_reg(client,0x56,0x40); 
}

void ov2650_set_effect_sepiagreen(struct i2c_client *client)
{
   ov2650_write_reg(client,0x3A,0x1C);  
   ov2650_write_reg(client,0x67,0x40);  
   ov2650_write_reg(client,0x68,0x40);
   ov2650_write_reg(client,0x56,0x40);  
}

void ov2650_set_effect_sepiablue(struct i2c_client *client)
{
   ov2650_write_reg(client,0x3A,0x1C);  
   ov2650_write_reg(client,0x67,0xc0);  
   ov2650_write_reg(client,0x68,0x80);
   ov2650_write_reg(client,0x56,0x40);  
}


int ov2650_set_effect(struct cim_sensor *sensor_info,unsigned short arg)
{
#if 0
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
#endif
		return 0;
}

void ov2650_set_wb_auto(struct i2c_client *client)
{       
  int temp = ov2650_read_reg(client,0x13);
  ov2650_write_reg(client,0x13,temp|0x02);   // Enable AWB	
  ov2650_write_reg(client,0x01,0x56);
  ov2650_write_reg(client,0x02,0x44);		
}

void ov2650_set_wb_cloud(struct i2c_client *client)
{       
  int temp = ov2650_read_reg(client,0x13);
  ov2650_write_reg(client,0x13,temp&~0x02);   // Disable AWB	
  ov2650_write_reg(client,0x01,0x58);
  ov2650_write_reg(client,0x02,0x60);
  ov2650_write_reg(client,0x6a,0x40);
}

void ov2650_set_wb_daylight(struct i2c_client *client)
{              
  int temp = ov2650_read_reg(client,0x13);
  ov2650_write_reg(client,0x13,temp&~0x02);   // Disable AWB	
  ov2650_write_reg(client,0x01,0x56);
  ov2650_write_reg(client,0x02,0x5c);
  ov2650_write_reg(client,0x6a,0x42);

}


void ov2650_set_wb_incandescence(struct i2c_client *client)
{    
  int temp = ov2650_read_reg(client,0x13);
  ov2650_write_reg(client,0x13,temp&~0x02);   // Disable AWB	
  ov2650_write_reg(client,0x01,0x9a);
  ov2650_write_reg(client,0x02,0x40);
  ov2650_write_reg(client,0x6a,0x48);                             
}

void ov2650_set_wb_fluorescent(struct i2c_client *client)
{
  int temp = ov2650_read_reg(client,0x13);
  ov2650_write_reg(client,0x13,temp&~0x02);   // Disable AWB	
  ov2650_write_reg(client,0x01,0x8b);
  ov2650_write_reg(client,0x02,0x42);
  ov2650_write_reg(client,0x6a,0x40);
                       
}

void ov2650_set_wb_tungsten(struct i2c_client *client)
{   
  int temp = ov2650_read_reg(client,0x13);
  ov2650_write_reg(client,0x13,temp&~0x02);   // Disable AWB	
  ov2650_write_reg(client,0x01,0xb8);
  ov2650_write_reg(client,0x02,0x40);
  ov2650_write_reg(client,0x6a,0x4f);
}

int ov2650_set_balance(struct cim_sensor *sensor_info,unsigned short arg)
{
#if 0
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
			ov2650_set_wb_fluorescent(client);
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
#endif
	return 0;
}

