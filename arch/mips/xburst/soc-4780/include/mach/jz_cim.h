#ifndef __JZ_CIM_H__
#define __JZ_CIM_H__

#define IOCTL_CIM_START				0xf01 // start preview
#define IOCTL_CIM_STOP				0xf02 // stop preview
#define IOCTL_CIM_START_PREVIEW			0xf03	
#define IOCTL_CIM_STOP_PREVIEW			0xf04
#define IOCTL_CIM_GET_PREVIEW_BUF_INFO		0xf05
#define IOCTL_CIM_GET_CAPTURE_BUF_INFO		0xf06
#define IOCTL_CIM_GET_PREVIEW_OFFSET		0xf07
#define IOCTL_CIM_GET_GLOBAL_INFO		0xf08
#define IOCTL_CIM_GET_SENSOR_INFO		0xf09
#define IOCTL_CIM_GET_CAPTURE_PARAM		0xf0a
#define IOCTL_CIM_GET_PREVIEW_PARAM		0xf0b	// get preview size and format
#define IOCTL_CIM_GET_SUPPORT_SIZE		0xf0c
#define IOCTL_CIM_SET_PARAM			0xf0d
#define IOCTL_CIM_SET_PREVIEW_MEM		0xf0e 	// alloc mem for buffers by app
#define IOCTL_CIM_SET_CAPTURE_MEM		0xf0f	// alloc mem for buffers by app
#define IOCTL_CIM_TAKE_SNAPSHOT			0xf10
#define IOCTL_CIM_SELECT_SENSOR			0xf11
#define IOCTL_CIM_DO_FOCUS			0xf12
#define IOCTL_CIM_AF_INIT			0xf13
#define IOCTL_CIM_STOP_FOCUS			0xf14
#define IOCTL_CIM_STOP_SNAPSHOT			0xf15

//-----------------------------------------------------------------------------------------------------------------------
//cim config for sensor YUYV output order
#define SENSOR_OUTPUT_FORMAT_Y0UY1V 0	// 11 22 33 44
#define SENSOR_OUTPUT_FORMAT_UY1VY0 1	//22 33 44 11
#define SENSOR_OUTPUT_FORMAT_Y1VY0U 2	//33 44 11 22
#define SENSOR_OUTPUT_FORMAT_VY0UY1 3	//44 11 22 33
#define SENSOR_OUTPUT_FORMAT_VY1UY0 4	//44 33 22 11
#define SENSOR_OUTPUT_FORMAT_Y1UY0V 5	//33 22 11 44
#define SENSOR_OUTPUT_FORMAT_UY0VY1 6	//22 11 44 33
#define SENSOR_OUTPUT_FORMAT_Y0VY1U 7	//11 44 33 22

//camera param cmd
#define CPCMD_SET_PREVIEW_RESOLUTION		0x01
#define CPCMD_SET_CAPTURE_RESOLUTION		0x02
#define CPCMD_SET_OUTPUT_FORMAT			0x03

#define CPCMD_SET_BALANCE			0x04
#define CPCMD_SET_EFFECT			0x05
#define CPCMD_SET_ANTIBANDING			0x06
#define CPCMD_SET_FLASH_MODE			0x07
#define CPCMD_SET_SCENE_MODE			0x08
#define CPCMD_SET_PIXEL_FORMAT			0x09
#define CPCMD_SET_FOCUS_MODE			0x0a
#define CPCMD_SET_PREVIEW_FPS			0x0b
#define CPCMD_SET_NIGHTSHOT_MODE		0x0c
#define CPCMD_SET_LUMA_ADAPTATION               0x0d

#define CPCMD_SET_BRIGHTNESS			0x0e//add for VT app 
#define CPCMD_SET_CONTRAST			0x0f//add for VT app 
//-----------------------------------------------------------------------------------------------------------------------


// Values for white balance settings.
#define WHITE_BALANCE_AUTO			0x1<<0
#define WHITE_BALANCE_INCANDESCENT 		0x1<<1
#define WHITE_BALANCE_FLUORESCENT 		0x1<<2
#define WHITE_BALANCE_WARM_FLUORESCENT 		0x1<<3
#define WHITE_BALANCE_DAYLIGHT 			0x1<<4
#define WHITE_BALANCE_CLOUDY_DAYLIGHT 		0x1<<5
#define WHITE_BALANCE_TWILIGHT 			0x1<<6
#define WHITE_BALANCE_SHADE 			0x1<<7
//fish add case tungsten(home) mode on 2010.6.9
#define WHITE_BALANCE_TUNGSTEN 			0x1<<8
		
// Values for effect settings.
#define EFFECT_NONE				0x1<<0
#define EFFECT_MONO 				0x1<<1
#define EFFECT_NEGATIVE 			0x1<<2
#define EFFECT_SOLARIZE 			0x1<<3
#define EFFECT_SEPIA 				0x1<<4
#define EFFECT_POSTERIZE 			0x1<<5
#define EFFECT_WHITEBOARD 			0x1<<6
#define EFFECT_BLACKBOARD			0x1<<7	
#define EFFECT_AQUA 				0x1<<8
#define EFFECT_PASTEL				0x1<<9
#define EFFECT_MOSAIC				0x1<<10
#define EFFECT_RESIZE				0x1<<11

// Values for antibanding settings.
#define ANTIBANDING_AUTO 			0x1<<0
#define ANTIBANDING_50HZ 			0x1<<1
#define ANTIBANDING_60HZ 			0x1<<2
#define ANTIBANDING_OFF 			0x1<<3

// Values for flash mode settings.
#define FLASH_MODE_OFF				0x1<<0
#define FLASH_MODE_AUTO 			0x1<<1
#define FLASH_MODE_ON 				0x1<<2
#define FLASH_MODE_RED_EYE 			0x1<<3
#define FLASH_MODE_TORCH 		        0x1<<4	

// Values for scene mode settings.
#define SCENE_MODE_AUTO 			0x1<<0
#define SCENE_MODE_ACTION 			0x1<<1
#define SCENE_MODE_PORTRAIT   			0x1<<2
#define SCENE_MODE_LANDSCAPE  			0x1<<3
#define SCENE_MODE_NIGHT     			0x1<<4
#define SCENE_MODE_NIGHT_PORTRAIT   		0x1<<5
#define SCENE_MODE_THEATRE  			0x1<<6
#define SCENE_MODE_BEACH   			0x1<<7
#define SCENE_MODE_SNOW    			0x1<<8
#define SCENE_MODE_SUNSET    			0x1<<9
#define SCENE_MODE_STEADYPHOTO   		0x1<<10
#define SCENE_MODE_FIREWORKS    		0x1<<11
#define SCENE_MODE_SPORTS    			0x1<<12
#define SCENE_MODE_PARTY   			0x1<<13
#define SCENE_MODE_CANDLELIGHT 			0x1<<14

// Values for focus mode settings.
#define FOCUS_MODE_FIXED 			0x1<<0
#define FOCUS_MODE_AUTO				0x1<<1	 
#define FOCUS_MODE_INFINITY 			0x1<<2
#define FOCUS_MODE_MACRO 			0x1<<3

struct frm_size {
	unsigned int w;
	unsigned int h;
};

struct mode_bit_map {
	unsigned long balance;
	unsigned long effect;
	unsigned long antibanding;
	unsigned long flash_mode;
	unsigned long scene_mode;
	unsigned long focus_mode;
};

struct cim_sensor {
	int 	id;
	const char *name;
	struct list_head	list;

        int facing;
        int orientation;

	struct mode_bit_map modes;

	struct frm_size	*preview_size;
	struct frm_size	*capture_size;

	int (*probe)(void *data);
	int (*init)(void *data);
	int (*power_on)(void *data);
	int (*shutdown)(void *data);

	int (*af_init)(void *data);
	int (*start_af)(void *data);
	int (*stop_af)(void *data);

	int (*set_preivew_mode)(void *data);
	int (*set_capture_mode)(void *data);

	int (*set_resolution)(void *data,int width,int height);
	int (*set_parameter)(void *data,int mode, int arg);

	int (*fill_buffer)(void *data,char *buf);
	
	void *private;
};

struct jz_cim_platform_data {
	void (*power_on)(void);
	void (*power_off)(void);
};

#define CAMERA_FACING_FRONT  1
#define CAMERA_FACING_BACK  0

extern int camera_sensor_register(struct cim_sensor *desc);

#endif
