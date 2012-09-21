#ifndef __JZ_X2D_H__
#define __JZ_X2D_H__

/*
 * IOCTL_XXX commands
 */
#define X2D_IOCTL_MAGIC 'X'

#define IOCTL_X2D_SET_CONFIG			0xf01
#define IOCTL_X2D_START_COMPOSE		0xf02
#define IOCTL_X2D_GET_MY_CONFIG		0xf03
#define IOCTL_X2D_GET_SYSINFO			0xf04
#define IOCTL_X2D_STOP					0xf05
#define IOCTL_X2D_MAP_GRAPHIC_BUF		0xf06
#define IOCTL_X2D_FREE_GRAPHIC_BUF		0xf07


#define X2D_RGBORDER_RGB 			 0  
#define X2D_RGBORDER_BGR			 1  
#define X2D_RGBORDER_GRB			 2  
#define X2D_RGBORDER_BRG			 3  
#define X2D_RGBORDER_RBG			 4  
#define X2D_RGBORDER_GBR			 5  

#define X2D_ALPHA_POSLOW			 1 
#define X2D_ALPHA_POSHIGH			 0 

#define X2D_H_MIRROR				 4
#define X2D_V_MIRROR				 8
#define X2D_ROTATE_0				 0 
#define	X2D_ROTATE_90 				 1 
#define X2D_ROTATE_180				 2 
#define X2D_ROTATE_270				 3 

#define X2D_INFORMAT_ARGB888		0 
#define X2D_INFORMAT_RGB555			1 
#define X2D_INFORMAT_RGB565			2 
#define X2D_INFORMAT_YUV420SP		3 
#define X2D_INFORMAT_TILE420		4 
#define X2D_INFORMAT_NV12			5 
#define X2D_INFORMAT_NV21			6 

#define X2D_OUTFORMAT_ARGB888		0 
#define X2D_OUTFORMAT_XARGB888		1 
#define X2D_OUTFORMAT_RGB565		2 
#define X2D_OUTFORMAT_RGB555		3 

#define X2D_OSD_MOD_CLEAR			3 
#define X2D_OSD_MOD_SOURCE			1 
#define X2D_OSD_MOD_DST				2 
#define X2D_OSD_MOD_SRC_OVER		0 
#define X2D_OSD_MOD_DST_OVER		4 
#define X2D_OSD_MOD_SRC_IN			5 
#define X2D_OSD_MOD_DST_IN			6 
#define X2D_OSD_MOD_SRC_OUT			7 
#define X2D_OSD_MOD_DST_OUT			8 
#define X2D_OSD_MOD_SRC_ATOP		9 
#define X2D_OSD_MOD_DST_ATOP		0xa 
#define X2D_OSD_MOD_XOR				0xb

enum jz_x2d_state{
	x2d_state_idle,			 //空闲
	x2d_state_calc,			 //运算
	x2d_state_complete,		 //完成
	x2d_state_error,				 //错误
	x2d_state_suspend,		//being suspend
};

enum jz_x2d_errcode{
	error_none,				//无错误
	error_calc,				//运算错误
	error_wthdog,				//看门狗超时
	error_TLB,				//TLB异常
};

typedef struct x2d_lay_info
{ // Order Cannot be Changed!
	uint8_t lay_ctrl;
	uint8_t lay_galpha;
	uint8_t rom_ctrl; //rotate and mirror control
	uint8_t RGBM;

	uint32_t y_addr;

	uint32_t u_addr;
	uint32_t v_addr;

	uint16_t swidth;
	uint16_t sheight;
	uint16_t ystr;
	uint16_t uvstr;

	uint16_t owidth;
	uint16_t oheight;
	uint16_t oxoffset;
	uint16_t oyoffset;

	uint16_t rsz_hcoef;
	uint16_t rsz_vcoef;
	uint32_t bk_argb;
}x2d_lay_info, *x2d_lay_info_p;

typedef struct x2d_chain_info
{ // Order Cannot be Changed!
	uint16_t   overlay_num;
	uint16_t   dst_tile_en;
	uint32_t   dst_addr;
	uint32_t   dst_ctrl_str;
	uint16_t   dst_width;
	uint16_t   dst_height;
	uint32_t   dst_argb;
	uint32_t   dst_fmt;
	x2d_lay_info x2d_lays[4];
} x2d_chain_info, *x2d_chain_info_p;

struct src_layer {
	int format;
	int transform;// such as rotate or mirror
	int global_alpha_val;
	int argb_order;
	int osd_mode;
	int preRGB_en;
	int glb_alpha_en;
	int mask_en;
	int color_cov_en;
	//input  output size
	int in_width;			//LAY0_SGS
	int in_height;
	int out_width;			//LAY0_OGS
	int out_height;
	int out_w_offset;		//LAY0_OOSF
	int out_h_offset;

	int v_scale_ratio;
	int h_scale_ratio;
	int msk_val;

	//yuv address
	int addr;
	int u_addr;
	int v_addr;
	int y_stride;
	int v_stride;
};

struct jz_x2d_config{
	//global
	int watchdog_cnt; 
	unsigned int tlb_base;

	//dst 
	int dst_address;		//DST_BASE	
	int dst_alpha_val;		//DST__CTRL_STR - alpha
	int dst_stride;			//DST__CTRL_STR -DST_STR
	int dst_mask_val;		//DST_MASK_ARGB
	int dst_width;			//DST_GS
	int dst_height;			//DST_GS
	int dst_bcground;		//DST_MASK_ARGB
	int dst_format;			//DST_FMT -RGB_FMT
	int dst_back_en;		//DST_CTRL_STR -back_en
	int dst_preRGB_en;		//DST_CTRL_STR -preM_en
	int dst_glb_alpha_en;	//DST_CTRL_STR -Glba_en
	int dst_mask_en;	    //DST_CTRL_STR -Msk_en
	//int dst_backpic_alpha_en;

	//src layers
	int layer_num;
	struct src_layer lay[4];
};

struct x2d_process_info{
	pid_t pid;						//进程号
	unsigned int tlb_base;					//该进程tlb表
	int * record_addr_list;
	int record_addr_num;
	struct jz_x2d_config configs;			//该进程存储所需运算的配置
	struct list_head list;//链表下一结点
};


struct jz_x2d_platform_data {
	void (*power_on)(void);
	void (*power_off)(void);

};



#endif // __JZ_CIM_H__

