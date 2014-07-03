#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <mach/jz_libdmmu.h>
#include <linux/sched.h>
#include <linux/regulator/consumer.h>

#include "ovisp-isp.h"
#include "ovisp-csi.h"
#include "isp-i2c.h"
#include "isp-ctrl.h"
#include "isp-regs.h"
#include "isp-firmware_array.h"
#include "ovisp-video.h"
#include "ovisp-debugtool.h"
#include "mipi_test_bypass.h"
#include "isp_process_raw.h"
#include "isp-debug.h"
#include "../ov5645.h"



/* Timeouts. */
#define ISP_BOOT_TIMEOUT	(3000) /* ms. */
#define ISP_I2C_TIMEOUT		(3000) /* ms. */
#define ISP_ZOOM_TIMEOUT	(3000) /* ms. */
#define ISP_FORMAT_TIMEOUT	(6000) /* ms. */
#define ISP_CAPTURE_TIMEOUT	(8000) /* ms. */
#define ISP_OFFLINE_TIMEOUT	(8000) /* ms. */
#define iSP_BRACKET_TIMEOUT (1000) /* ms */

/* Clock flags. */
#define ISP_CLK_CGU_ISP			(0x00000001)
#define ISP_CLK_GATE_ISP		(0x00000002)
#define ISP_CLK_GATE_CSI		(0x00000004)
#define ISP_CLK_ALL		(0xffffffff)

/* CCLK divider. */
#define ISP_CCLK_DIVIDER	(0x04)
static int isp_s_tlb_base(struct isp_device *isp, unsigned int *tlb_base);
static int isp_mipi_init(struct isp_device *isp);

static int isp_update_buffer(struct isp_device *isp, struct isp_buffer *buf, int);
static int isp_get_zoom_ratio(struct isp_device *isp, int zoom);

static int isp_clk_enable(struct isp_device *isp, unsigned int type);
struct isp_clk_info {
	const char* name;
	unsigned long rate;
	unsigned long flags;
};

#undef ISP_CLK_NUM
#define ISP_CLK_NUM	3
#define DUMMY_CLOCK_RATE		0x0000ffff
/* clk isp , csi */
static struct isp_clk_info isp_clks[ISP_CLK_NUM] = {
	{"cgu_isp", 100000000,	ISP_CLK_CGU_ISP},
	{"isp", DUMMY_CLOCK_RATE,	ISP_CLK_GATE_ISP},
	{"csi", DUMMY_CLOCK_RATE,	ISP_CLK_GATE_CSI},
};

static int isp_intc_disable(struct isp_device *isp, unsigned short mask)
{

	unsigned long flags;
	unsigned char h,l;
	l = mask & 0xff;
	h = (mask >> 0x08) & 0xff;

	spin_lock_irqsave(&isp->lock, flags);
	if (l) {
		isp->intr_l &= ~l;
		isp_reg_writeb(isp, isp->intr_l, REG_ISP_INT_EN_L);
	}
	if (h) {
		isp->intr_h &= ~h;
		isp_reg_writeb(isp, isp->intr_h, REG_ISP_INT_EN_H);
	}
	spin_unlock_irqrestore(&isp->lock, flags);

	return 0;
}
static int isp_intc_enable(struct isp_device * isp, unsigned short mask)
{
	unsigned long flags;
	unsigned char h,l;
	l = mask & 0xff;
	h = (mask >> 0x08) & 0xff;

	spin_lock_irqsave(&isp->lock, flags);
	if (l) {
		isp->intr_l |= l;
		isp_reg_writeb(isp, isp->intr_l, REG_ISP_INT_EN_L);
	}
	if (h) {
		isp->intr_h |= h;
		isp_reg_writeb(isp, isp->intr_h, REG_ISP_INT_EN_H);
	}
	spin_unlock_irqrestore(&isp->lock, flags);

	return 0;
}

static unsigned short isp_intc_state(struct isp_device *isp)
{
	unsigned short h = 0,l = 0;
	h = isp_reg_readb(isp, REG_ISP_INT_STAT_H);
	l = isp_reg_readb(isp, REG_ISP_INT_STAT_L);
	return (h << 0x08) | l;
}

static int isp_mac_int_mask(struct isp_device *isp, unsigned short mask)
{
	unsigned char mask_l = mask & 0x00ff;
	unsigned char mask_h = (mask >> 8) & 0x00ff;
	unsigned long flags;

	spin_lock_irqsave(&isp->lock, flags);
	if (mask_l) {
		isp->mac_intr_l &= ~mask_l;
		isp_reg_writeb(isp, isp->mac_intr_l, REG_ISP_MAC_INT_EN_L);
	}
	if (mask_h) {
		isp->mac_intr_h &= ~mask_h;
		isp_reg_writeb(isp, isp->mac_intr_h, REG_ISP_MAC_INT_EN_H);
	}
	spin_unlock_irqrestore(&isp->lock, flags);

	return 0;
}

static int isp_mac_int_unmask(struct isp_device *isp, unsigned short mask)
{
	unsigned char mask_l = mask & 0x00ff;
	unsigned char mask_h = (mask >> 8) & 0x00ff;
	unsigned long flags;

	spin_lock_irqsave(&isp->lock, flags);
	if (mask_l) {
		isp->mac_intr_l |= mask_l;
		isp_reg_writeb(isp, isp->mac_intr_l, REG_ISP_MAC_INT_EN_L);
	}
	if (mask_h) {
		isp->mac_intr_h |= mask_h;
		isp_reg_writeb(isp, isp->mac_intr_h, REG_ISP_MAC_INT_EN_H);
	}
	spin_unlock_irqrestore(&isp->lock, flags);

	return 0;
}

static unsigned short isp_mac_int_state(struct isp_device *isp)
{
	unsigned short state_l;
	unsigned short state_h;
	state_l = isp_reg_readb(isp, REG_ISP_MAC_INT_STAT_L);
	state_h = isp_reg_readb(isp, REG_ISP_MAC_INT_STAT_H);

	return (state_h << 8) | state_l;
}


static int isp_wait_cmd_done(struct isp_device *isp, unsigned long timeout)
{
	unsigned long tm;
	int ret = 0;
#if 1
	tm = wait_for_completion_timeout(&isp->completion,
			msecs_to_jiffies(timeout));
	if (!tm && !isp->completion.done) {
		ret = -ETIMEDOUT;
	}
#else
		while(--timeout) {
			tm = isp_reg_readb(isp, 0x63928);
			if(tm & 1) {
				//printk("tm :%x, timeout:%x\n", tm, timeout);
				break;
			}
			mdelay(50);
		}
		if(timeout == 0) {
			ret = -ETIMEDOUT;
		}
#endif

	return ret;
}
static inline unsigned char get_current_cmd(struct isp_device *isp)
{
	return isp_reg_readb(isp, COMMAND_FINISHED);
}
static int isp_send_cmd(struct isp_device *isp, unsigned char id,
		unsigned long timeout)
{
	int ret;

	INIT_COMPLETION(isp->completion);

	isp_intc_enable(isp, MASK_INT_CMDSET);
	isp_reg_writeb(isp, id, COMMAND_REG0);

	/* Wait for command set done interrupt. */
	ret = isp_wait_cmd_done(isp, timeout);
	isp_intc_disable(isp, MASK_INT_CMDSET);
	/* determine whether setting cammand successfully */
	if ((CMD_SET_SUCCESS != isp_reg_readb(isp, COMMAND_RESULT))
			|| (id != get_current_cmd(isp))) {
		ISP_PRINT(ISP_INFO, KERN_ERR "Failed to write sequeue to I2C (%02x:%02x)\n",
				isp_reg_readb(isp, COMMAND_RESULT),
				isp_reg_readb(isp, COMMAND_FINISHED));
		ret = -EINVAL;
	}

	return ret;
}

static int isp_set_address(struct isp_device *isp,
		unsigned int id, unsigned int addr)
{
	struct isp_parm *iparm = &isp->parm;
	unsigned int reg = id ? REG_BASE_ADDR1 : REG_BASE_ADDR0;
	unsigned int regw = reg;
	unsigned int addrw = addr;
	switch(iparm->output[0].addrnums){
		case 3:
			addrw = addr + iparm->output[0].addroff[2];
			regw = reg + 0x08;
			isp_reg_writeb(isp, (addrw >> 24) & 0xff, regw);
			isp_reg_writeb(isp, (addrw >> 16) & 0xff, regw + 1);
			isp_reg_writeb(isp, (addrw >> 8) & 0xff, regw + 2);
			isp_reg_writeb(isp, (addrw >> 0) & 0xff, regw + 3);
		case 2:
			addrw = addr + iparm->output[0].addroff[1];
			regw = reg + 0x04;
			isp_reg_writeb(isp, (addrw >> 24) & 0xff, regw);
			isp_reg_writeb(isp, (addrw >> 16) & 0xff, regw + 1);
			isp_reg_writeb(isp, (addrw >> 8) & 0xff, regw + 2);
			isp_reg_writeb(isp, (addrw >> 0) & 0xff, regw + 3);
		case 1:
			addrw = addr;
			regw = reg;
			isp_reg_writeb(isp, (addrw >> 24) & 0xff, regw);
			isp_reg_writeb(isp, (addrw >> 16) & 0xff, regw + 1);
			isp_reg_writeb(isp, (addrw >> 8) & 0xff, regw + 2);
			isp_reg_writeb(isp, (addrw >> 0) & 0xff, regw + 3);
			break;
		default:
			ISP_PRINT(ISP_ERROR, "%s[%d] addrnums is wrong; it should be 1 ~ 3,but it is %d",
					__func__,__LINE__,iparm->output[0].addrnums);
			break;
	}
	if(iparm->out_videos == 2){
		addr += iparm->output[0].imagesize;
		switch(iparm->output[1].addrnums){
			case 3:
				addrw = addr + iparm->output[1].addroff[2];
				if (0 == id)
					regw = reg + 0x64;
				else
					regw = reg + 0x5c;
				isp_reg_writeb(isp, (addrw >> 24) & 0xff, regw);
				isp_reg_writeb(isp, (addrw >> 16) & 0xff, regw + 1);
				isp_reg_writeb(isp, (addrw >> 8) & 0xff, regw + 2);
				isp_reg_writeb(isp, (addrw >> 0) & 0xff, regw + 3);
			case 2:
				addrw = addr + iparm->output[1].addroff[1];
				if (0 == id)
					regw = reg + 0x60;
				else
					regw = reg + 0x58;
				isp_reg_writeb(isp, (addrw >> 24) & 0xff, regw);
				isp_reg_writeb(isp, (addrw >> 16) & 0xff, regw + 1);
				isp_reg_writeb(isp, (addrw >> 8) & 0xff, regw + 2);
				isp_reg_writeb(isp, (addrw >> 0) & 0xff, regw + 3);
			case 1:
				addrw = addr;
				regw = reg + 0x0c;
				isp_reg_writeb(isp, (addrw >> 24) & 0xff, regw);
				isp_reg_writeb(isp, (addrw >> 16) & 0xff, regw + 1);
				isp_reg_writeb(isp, (addrw >> 8) & 0xff, regw + 2);
				isp_reg_writeb(isp, (addrw >> 0) & 0xff, regw + 3);
				break;
			default:
				ISP_PRINT(ISP_ERROR, "%s[%d] addrnums is wrong; it should be 1 ~ 3,but it is %d",
						__func__,__LINE__,iparm->output[1].addrnums);
				break;
		}
	}
	return 0;
}
#if 0
static int isp_calc_zoom(struct isp_device *isp)
{
	int crop_w = 0;
	int crop_h = 0;
	int crop_x = 0;
	int crop_y = 0;
	int downscale_w;
	int downscale_h;
	int dcw_w;
	int dcw_h;
	int Wi;
	int Wo;
	int Hi;
	int Ho;
	int ratio_h;
	int ratio_w;
	int ratio_gross;
	int ratio;
	int ratio_dcw = 0;
	int dratio = 0;
	int ratio_d = 0;
	int ratio_up = 0;
	int dcwFlag =0;
	int downscaleFlag=0;
	int w_dcw;
	int h_dcw;
	int i;
	int j;
	int zoom_0;
	int zoom_ratio;
	int crop_width;
	int crop_height;
	int in_width;
	int in_height;
	int out_height;
	int out_width;
	int final_crop_width;
	int final_crop_height;
	int crop_x0 = 0;
	int crop_y0 = 0;
	int ret = 1;
	int t1;
	int t2;

	out_width = isp->parm.out_width;
	out_height = isp->parm.out_height;
	in_width = isp->parm.in_width;
	in_height = isp->parm.in_height;

	zoom_0 = isp_get_zoom_ratio(isp, ZOOM_LEVEL_0);
	zoom_ratio = isp_get_zoom_ratio(isp, isp->parm.zoom);

	crop_width = (zoom_0 * in_width)/zoom_ratio;
	crop_height = (zoom_0 * in_height)/zoom_ratio;

	if(((crop_width*1000)/crop_height) <= ((out_width*1000)/out_height)){
		final_crop_width = crop_width;
		final_crop_width = final_crop_width&0xfffe;
		final_crop_height = (final_crop_width * out_height)/out_width;
		final_crop_height = final_crop_height&0xfffe;
	}else{
		final_crop_height = crop_height;
		final_crop_height = final_crop_height&0xfffe;
		final_crop_width = (final_crop_height * out_width)/out_height;
		final_crop_width = final_crop_width&0xfffe;
	}
	crop_x0 = (in_width - final_crop_width)/2;
	crop_y0 = (in_height - final_crop_height)/2;

	Wo = isp->parm.out_width;
	Ho = isp->parm.out_height;
	Wi = final_crop_width;
	Hi = final_crop_height;
	ISP_PRINT(ISP_INFO, "Wo %d;Ho %d;Wi %d;Hi %d\n",Wo,Ho,Wi,Hi);

	if(final_crop_width> isp->parm.out_width) {
		crop_w =crop_h =crop_x =crop_y =downscale_w =downscale_h =dcw_w =dcw_h = 0;
		ratio_h = (Hi*1000/Ho);
		ratio_w = (Wi*1000/Wo);
		ratio_gross = (ratio_h>= ratio_w) ? ratio_w:ratio_h;
		ratio = ratio_gross/1000;
		for (i=0;i<4;i++)
			if((1<<i)<=ratio && (1<<(i+1))>ratio)
				break;
		if(i==4) i--;
		dcw_w = dcw_h = i;
		ratio_dcw = i;
		if(dcw_w == 0)
			dcwFlag = 0;
		else
			dcwFlag = 1;


		h_dcw = (1<<i)*Ho;
		w_dcw = (1<<i)*Wo;

		downscale_w = (256*w_dcw + Wi)/(2*Wi);
		downscale_h = (256*h_dcw + Hi)/(2*Hi);
		dratio = (downscale_w>=downscale_h)?downscale_w:downscale_h;
		if(dratio == 128)
			downscaleFlag = 0;
		else {
			downscaleFlag = 1;
			dratio += 1;
		}

		crop_w = (256*w_dcw + dratio)/(2*dratio);
		crop_h = (256*h_dcw + dratio)/(2*dratio);
		crop_w = crop_w&0xfffe;
		crop_h = crop_h&0xfffe;

		//update by wayne
		for(j=-3;j<=3;j++) {
			crop_w = (256*w_dcw + (dratio+j))/(2*(dratio+j));
			crop_h = (256*h_dcw + (dratio+j))/(2*(dratio+j));
			crop_w = crop_w&0xfffe;
			crop_h = crop_h&0xfffe;

			for(i=0;i<=4;i+=2) {
				t1 = (crop_w+i)*(dratio+j)/128;
				t2 = (crop_h+i)*(dratio+j)/128;
				if((t1&0xfffe) == t1 && t1 >= w_dcw && (t2&0xfffe) == t2 && t2 >= h_dcw && (dratio+j)>=64 &&(dratio+j)<=128 && (crop_w +i)<= Wi && (crop_h+i)<= Hi)
				{
					ret = 0;
					break;
				}

			}
			if(ret == 0)
				break;

		}
		if(j==4) j--;
		if(i==6) i = i-2;
		ISP_PRINT(ISP_INFO, "i = %d,j = %d\n",i,j);
		crop_w += i;
		crop_h += i;
		dratio += j;
		//end
		crop_x = (Wi-crop_w)/2;
		crop_y = (Hi-crop_h)/2;

		ratio_d = dratio;
	}
	else {
		ratio_up= ((final_crop_height* 0x100)/isp->parm.out_height);
		crop_w =  final_crop_width;
		crop_h = final_crop_height;
	}

	isp->parm.ratio_up = ratio_up;
	isp->parm.ratio_d = ratio_d;
	isp->parm.ratio_dcw = ratio_dcw;
	isp->parm.crop_width = crop_w;
	isp->parm.crop_height = crop_h;

	isp->parm.crop_x = crop_x + crop_x0;
	isp->parm.crop_y = crop_y + crop_y0;
	isp->parm.dcwFlag = dcwFlag;
	isp->parm.dowscaleFlag = downscaleFlag;

	/*isp_dump_cal_zoom(isp);*/

	return 0;
}
#endif
static int isp_set_parameters(struct isp_device *isp)
{
	struct ovisp_camera_client *client = isp->client;
	struct isp_parm *iparm = &isp->parm;
	struct ovisp_camera_dev *camera_dev;
	unsigned short iformat;
	unsigned short oformat0;
	unsigned short oformat1;
	u32 snap_paddr;

	camera_dev = (struct ovisp_camera_dev *)(isp->data);
#if 0
	if(isp->hdr_mode==1)
		snap_paddr = camera_dev->offline.paddr;
	else
		snap_paddr = isp->buf_start.addr;
#endif
	if (iparm->input.format == V4L2_MBUS_FMT_YUYV8_2X8)
		iformat = IFORMAT_YUV422;
	else if(iparm->input.format == V4L2_MBUS_FMT_SBGGR10_1X10)
		iformat = IFORMAT_RAW10;
	else
		iformat = IFORMAT_RAW8;
	switch (iparm->output[0].format) {
		case V4L2_PIX_FMT_YUYV:
			oformat0 = OFORMAT_YUV422;
			iparm->output[0].addrnums = 1;
			iparm->output[0].addroff[0] = 0;
			break;
		case V4L2_PIX_FMT_SGBRG8:
			oformat0 = OFORMAT_RAW8;
			iparm->output[0].addrnums = 1;
			iparm->output[0].addroff[0] = 0;
			break;
		case V4L2_PIX_FMT_YUV420:
			oformat0 = OFORMAT_YUV420;
			iparm->output[0].addrnums = 3;
			iparm->output[0].addroff[0] = 0;
			iparm->output[0].addroff[1] = iparm->output[0].width * iparm->output[0].height;
			iparm->output[0].addroff[2] = iparm->output[0].addroff[1] * 5 /4;
			break;
		case V4L2_PIX_FMT_NV12YUV422:
		case V4L2_PIX_FMT_NV12:
			oformat0 = OFORMAT_NV12;
			iparm->output[0].addrnums = 2;
			iparm->output[0].addroff[0] = 0;
			iparm->output[0].addroff[1] = iparm->output[0].width * iparm->output[0].height;
			ISP_PRINT(ISP_INFO, "iparm is addoff[1] = %d imagesize = %d\n",iparm->output[0].addroff[1],
					iparm->output[0].imagesize);
			break;
		default:
			return -EINVAL;
	}
	if (!isp->bypass)
		iformat |= ISP_PROCESS;/*bypass isp or isp processing*/

	//this is for capture raw data for image tuning
#ifdef OVISP_DEBUGTOOL_ENABLE
	if (isp->snapshot) {
		u8 raw_flag = 0;
		raw_flag = ovisp_debugtool_get_flag(OVISP_DEBUGTOOL_GETRAW_FILENAME);
		if ('1' == raw_flag) {
			oformat = OFORMAT_RAW10;
			iformat &= (~ISP_PROCESS);
		}
	}
#endif

	if (client->flags & CAMERA_CLIENT_IF_MIPI)
		iformat |= SENSOR_PRIMARY_MIPI;

	/*isp_dump_set_para(isp, iparm, iformat, oformat);*/
	isp_firmware_writeb(isp, 0x01, 0x1fff9);

	/* 1. INPUT CONFIGURATION */
	isp_firmware_writeb(isp, (iformat >> 8) & 0xff, ISP_INPUT_FORMAT);
	isp_firmware_writeb(isp, iformat & 0xff, ISP_INPUT_FORMAT + 1);

	isp_firmware_writeb(isp, (iparm->input.width >> 8) & 0xff,
			SENSOR_OUTPUT_WIDTH);
	isp_firmware_writeb(isp, iparm->input.width & 0xff,
			SENSOR_OUTPUT_WIDTH + 1);

	isp_firmware_writeb(isp, (iparm->input.height >> 8) & 0xff,
			SENSOR_OUTPUT_HEIGHT);
	isp_firmware_writeb(isp, iparm->input.height & 0xff,
			SENSOR_OUTPUT_HEIGHT + 1);

	/*IDI CONTROL, DISABLE ISP IDI SCALE*/
	isp_firmware_writeb(isp, 0x00, ISP_IDI_CONTROL);
	isp_firmware_writeb(isp, 0x00, ISP_IDI_CONTROL + 1);
	/* idi w,h */
	isp_firmware_writeb(isp, (iparm->input.width >> 8) & 0xff, ISP_IDI_OUTPUT_WIDTH);
	isp_firmware_writeb(isp, iparm->input.width & 0xff, ISP_IDI_OUTPUT_WIDTH + 1);
	isp_firmware_writeb(isp, (iparm->input.height >> 8) & 0xff, ISP_IDI_OUTPUT_HEIGHT);
	isp_firmware_writeb(isp, iparm->input.height & 0xff, ISP_IDI_OUTPUT_HEIGHT + 1);

	isp_firmware_writeb(isp, 0x00, ISP_IDI_OUTPUT_H_START);
	isp_firmware_writeb(isp, 0x00, ISP_IDI_OUTPUT_H_START + 1);
	isp_firmware_writeb(isp, 0x00, ISP_IDI_OUTPUT_V_START);
	isp_firmware_writeb(isp, 0x00, ISP_IDI_OUTPUT_V_START + 1);

	/* 2. OUTPUT CONFIGRATION */
	/* output1 */
	isp_firmware_writeb(isp, (oformat0 >> 8) & 0xff, ISP_OUTPUT_FORMAT);
	isp_firmware_writeb(isp, oformat0 & 0xff, ISP_OUTPUT_FORMAT + 1);

	isp_firmware_writeb(isp, (iparm->output[0].width >> 8) & 0xff,
			ISP_OUTPUT_WIDTH);
	isp_firmware_writeb(isp, iparm->output[0].width & 0xff,
			ISP_OUTPUT_WIDTH + 1);
	isp_firmware_writeb(isp, (iparm->output[0].height >> 8) & 0xff,
			ISP_OUTPUT_HEIGHT);
	isp_firmware_writeb(isp, iparm->output[0].height & 0xff,
			ISP_OUTPUT_HEIGHT + 1);
	/*y memwidth*/
	isp_firmware_writeb(isp, (iparm->output[0].width >> 8) & 0xff, MAC_MEMORY_WIDTH);
	isp_firmware_writeb(isp, iparm->output[0].width & 0xff, MAC_MEMORY_WIDTH + 1);

	isp_firmware_writeb(isp, (iparm->output[0].width >> 8) & 0xff, MAC_MEMORY_UV_WIDTH);
	isp_firmware_writeb(isp, iparm->output[0].width & 0xff, MAC_MEMORY_UV_WIDTH + 1);
	/*uv memwidth*/
	/* output2 */
	if(iparm->out_videos == 2){
		switch (iparm->output[1].format) {
			case V4L2_PIX_FMT_YUYV:
				oformat1 = OFORMAT_YUV422;
				iparm->output[1].addrnums = 1;
				iparm->output[1].addroff[0] = 0;
				break;
			case V4L2_PIX_FMT_SGBRG8:
				oformat1 = OFORMAT_RAW8;
				iparm->output[1].addrnums = 1;
				iparm->output[1].addroff[0] = 0;
				break;
			case V4L2_PIX_FMT_YUV420:
				oformat1 = OFORMAT_YUV420;
				iparm->output[1].addrnums = 3;
				iparm->output[1].addroff[0] = 0;
				iparm->output[1].addroff[1] = iparm->output[1].width * iparm->output[1].height;
				iparm->output[1].addroff[2] = iparm->output[1].addroff[1] * 5 /4;
				break;
			case V4L2_PIX_FMT_NV12YUV422:
			case V4L2_PIX_FMT_NV12:
				oformat1 = OFORMAT_NV12;
				iparm->output[1].addrnums = 2;
				iparm->output[1].addroff[0] = 0;
				iparm->output[1].addroff[1] = iparm->output[1].width * iparm->output[1].height;
				break;
			default:
				return -EINVAL;
		}
		isp_firmware_writeb(isp, (oformat1 >> 8) & 0xff, ISP_OUTPUT_FORMAT_2);
		isp_firmware_writeb(isp, oformat1 & 0xff, ISP_OUTPUT_FORMAT_2 + 1);

		isp_firmware_writeb(isp, (iparm->output[1].width >> 8) & 0xff,
				ISP_OUTPUT_WIDTH_2);
		isp_firmware_writeb(isp, iparm->output[1].width & 0xff,
				ISP_OUTPUT_WIDTH_2 + 1);
		isp_firmware_writeb(isp, (iparm->output[1].height >> 8) & 0xff,
				ISP_OUTPUT_HEIGHT_2);
		isp_firmware_writeb(isp, iparm->output[1].height & 0xff,
				ISP_OUTPUT_HEIGHT_2 + 1);

		isp_firmware_writeb(isp, (iparm->output[1].width >> 8) & 0xff, MAC_MEMORY_WIDTH_2);
		isp_firmware_writeb(isp, iparm->output[1].width & 0xff, MAC_MEMORY_WIDTH_2 + 1);

		isp_firmware_writeb(isp, (iparm->output[1].width >> 8) & 0xff, MAC_MEMORY_UV_WIDTH_2);
		isp_firmware_writeb(isp, iparm->output[1].width & 0xff, MAC_MEMORY_UV_WIDTH_2 + 1);
	}
	/* 3. ISP CONFIGURATION */
	//zoom in 1x
	isp_firmware_writeb(isp, 0x01, 0x1f084);
	isp_firmware_writeb(isp, 0x00, 0x1f085);

	//isp config
	isp_firmware_writeb(isp, 0x01, 0x1f070);
	isp_firmware_writeb(isp, 0x00, 0x1f071);
	isp_firmware_writeb(isp, 0x03, 0x1f072);
	isp_firmware_writeb(isp, 0x46, 0x1f073);

	isp_firmware_writeb(isp, 0x00, 0x1f074);
	isp_firmware_writeb(isp, 0x10, 0x1f075);
	isp_firmware_writeb(isp, 0x00, 0x1f076);
	isp_firmware_writeb(isp, 0xff, 0x1f077);

	isp_firmware_writeb(isp, 0x00, 0x1f078);
	isp_firmware_writeb(isp, 0x10, 0x1f079);
	//isp_firmware_writeb(isp, 0x32, 0x1f07a);
	//isp_firmware_writeb(isp, 0x00, 0x1f07b);
	isp_firmware_writeb(isp, 0x03, 0x1f07a);
	isp_firmware_writeb(isp, 0x21, 0x1f07b);

	/* 4. CAPTURE CONFIGURATION */

	if(isp->snapshot){
		isp_firmware_writeb(isp, (snap_paddr>>24)&0xff, ISP_BASE_ADDR_LEFT);
		isp_firmware_writeb(isp, (snap_paddr>>16)&0xff, ISP_BASE_ADDR_LEFT+1);
		isp_firmware_writeb(isp, (snap_paddr>>8)&0xff, ISP_BASE_ADDR_LEFT+2);
		isp_firmware_writeb(isp, (snap_paddr>>0)&0xff, ISP_BASE_ADDR_LEFT+3);
	}

	isp_firmware_writeb(isp, 0x06, 0x1fff9);
	dump_isp_configuration(isp);
	return 0;
}

static int isp_i2c_config(struct isp_device *isp)
{
	unsigned char val;

	if (isp->pdata->flags & CAMERA_I2C_FAST_SPEED)
		val = I2C_SPEED_200;
	else
		val = I2C_SPEED_100;

	isp_reg_writeb(isp, val, REG_SCCB_MAST1_SPEED);

	return 0;
}
static int isp_i2c_xfer_cmd_sccb(struct isp_device * isp, struct isp_i2c_cmd * cmd)
{
	unsigned char sccb_cmd;
	sccb_cmd = 0;


	isp_reg_writeb(isp, cmd->addr, 0x63601);

	/*sensore reg*/
	isp_reg_writeb(isp, (cmd->reg >> 8) & 0xff, 0x63602);
	isp_reg_writeb(isp, cmd->reg & 0xff, 0x63603);

	isp_reg_writeb(isp, 1, 0x63606);/*16bit addr enable*/

	if(!(cmd->flags & I2C_CMD_READ)) {
		if(cmd->flags & I2C_CMD_DATA_16BIT) {
			/**/
		} else {

			/*write data*/
			isp_reg_writeb(isp, (cmd->data >> 8) & 0xff, 0x63604);
			isp_reg_writeb(isp, cmd->data & 0xff, 0x63605);
		}

		isp_reg_writeb(isp, 0x37, 0x63609);
	}


	if (cmd->flags & I2C_CMD_READ) {

		isp_reg_writeb(isp, 0x33, 0x63609);
		mdelay(10);
		isp_reg_writeb(isp, 0xf9, 0x63609);
		mdelay(10);
		if(cmd->flags & I2C_CMD_DATA_16BIT) {

		}
		else {
			cmd->data = isp_reg_readb(isp, 0x63608); /*read data*/
		}
	}
	//mdelay(100);
	mdelay(2);
	//dump_i2c_regs(isp);
	return 0;
}

static int isp_i2c_xfer_cmd(struct isp_device *isp, struct isp_i2c_cmd *cmd);
struct v4l2_fmt_data gdata;
static int reg_num = 0;
static int isp_i2c_xfer_cmd_grp(struct isp_device * isp , struct isp_i2c_cmd * cmd)
{

	unsigned char val = 0;
	unsigned char i;
	if (cmd->flags & I2C_CMD_READ) {
		isp_i2c_xfer_cmd(isp, cmd);
		return 0;
	}
	/* I2C CMD WRITE */
		if(cmd->reg == 0xffff) { /*cmd list end*/
		/* send group read/write command */

		/*16bit: addr, 8bit:data, 8bit:mask*/
		for (i = 0; i < gdata.reg_num; i++) {
			isp_reg_writew(isp, gdata.reg[i].addr, COMMAND_BUFFER + i * 4 + 2);
			if (gdata.flags & I2C_CMD_DATA_16BIT) {
				isp_reg_writew(isp, gdata.reg[i].data,
						COMMAND_BUFFER + i * 4);
			} else {
				isp_reg_writeb(isp, 0xff, COMMAND_BUFFER + i * 4);
				isp_reg_writeb(isp, gdata.reg[i].data & 0xff,
						COMMAND_BUFFER + i * 4 + 1);
			}
		}

		if (gdata.reg_num) {
			val |= SELECT_I2C_PRIMARY | SELECT_I2C_WRITE;
			if (gdata.flags & V4L2_I2C_ADDR_16BIT)
				val |= SELECT_I2C_16BIT_ADDR;
			if (gdata.flags & V4L2_I2C_DATA_16BIT)
				val |= SELECT_I2C_16BIT_DATA;

			isp_reg_writeb(isp, val, COMMAND_REG1);
			isp_reg_writeb(isp, gdata.slave_addr, COMMAND_REG2);
			isp_reg_writeb(isp, gdata.reg_num, COMMAND_REG3);
		}

		__dump_isp_regs(isp, 0x63900, 0x63911);

		/* Wait for command set successfully. */
		if (isp_send_cmd(isp, CMD_I2C_GRP_WR, ISP_I2C_TIMEOUT)) {
			ISP_PRINT(ISP_INFO, KERN_ERR "Failed to wait i2c set done (%02x)!\n",
					isp_reg_readb(isp, REG_ISP_INT_EN_H));
			return -EINVAL;
		}

	} else {
		reg_num ++;
		gdata.reg_num = reg_num;
		gdata.slave_addr = cmd->addr;
		gdata.flags = cmd->flags;
		gdata.reg[reg_num-1].addr = cmd->reg;
		gdata.reg[reg_num-1].data = cmd->data;
		printk("data.slave_addr:%x, flags:%x,reg[%d].addr:%x,data:%x\n", gdata.slave_addr, gdata.flags,
				reg_num-1, gdata.reg[reg_num-1].addr, gdata.reg[reg_num-1].data);
	}
	return 0;
}

static int isp_i2c_xfer_cmd(struct isp_device *isp, struct isp_i2c_cmd *cmd)
{
	unsigned char val = 0;

	//dump_i2c_regs(isp);
	/*isp_firmware_writeb(isp, 0x78, 0x1e056);*/
	isp_reg_writew(isp, cmd->reg, COMMAND_BUFFER + 2);
	if (!(cmd->flags & I2C_CMD_READ)) {
		if (cmd->flags & I2C_CMD_DATA_16BIT) {
			isp_reg_writew(isp, cmd->data, COMMAND_BUFFER);
		} else { /*16bit:8bitdata,8bitmask*/
			isp_reg_writeb(isp, 0xff, COMMAND_BUFFER);
			isp_reg_writeb(isp, cmd->data & 0xff, COMMAND_BUFFER + 1);
		}
	}

	val |= SELECT_I2C_PRIMARY;
	if (!(cmd->flags & I2C_CMD_READ))
		val |= SELECT_I2C_WRITE;
	if (cmd->flags & I2C_CMD_ADDR_16BIT)
		val |= SELECT_I2C_16BIT_ADDR;
	if (cmd->flags & I2C_CMD_DATA_16BIT)
		val |= SELECT_I2C_16BIT_DATA;

	isp_reg_writeb(isp, val, COMMAND_REG1);
	isp_reg_writeb(isp, cmd->addr, COMMAND_REG2);
	isp_reg_writeb(isp, 0x01, COMMAND_REG3);

	/* Wait for command set successfully. */
	if (isp_send_cmd(isp, CMD_I2C_GRP_WR, ISP_I2C_TIMEOUT)) {
		ISP_PRINT(ISP_INFO, KERN_ERR "Failed to wait i2c set done (%02x)!\n",
				isp_reg_readb(isp, REG_ISP_INT_EN_H));
		return -EINVAL;
	}

	if (cmd->flags & I2C_CMD_READ) {
		if (cmd->flags & I2C_CMD_DATA_16BIT)
			cmd->data = isp_reg_readw(isp, COMMAND_BUFFER);
		else
			cmd->data = isp_reg_readb(isp, COMMAND_BUFFER);
	}

	//dump_i2c_regs(isp);
	//mdelay(150);

	return 0;
}

static int isp_i2c_fill_buffer(struct isp_device *isp)
{
	struct v4l2_fmt_data *data = &isp->fmt_data;
	unsigned char val = 0;
	unsigned char i;

	/*16bit: addr, 8bit:data, 8bit:mask*/
	for (i = 0; i < data->reg_num; i++) {
		isp_reg_writew(isp, data->reg[i].addr, COMMAND_BUFFER + i * 4 + 2);
		if (data->flags & I2C_CMD_DATA_16BIT) {
			isp_reg_writew(isp, data->reg[i].data,
					COMMAND_BUFFER + i * 4);
		} else {
			isp_reg_writeb(isp, 0xff, COMMAND_BUFFER + i * 4);
			isp_reg_writeb(isp, data->reg[i].data & 0xff,
					COMMAND_BUFFER + i * 4 + 1);
		}
	}

	if (data->reg_num) {
		val |= SELECT_I2C_PRIMARY | SELECT_I2C_WRITE;
		if (data->flags & V4L2_I2C_ADDR_16BIT)
			val |= SELECT_I2C_16BIT_ADDR;
		if (data->flags & V4L2_I2C_DATA_16BIT)
			val |= SELECT_I2C_16BIT_DATA;

		isp_reg_writeb(isp, val, COMMAND_REG1);
		isp_reg_writeb(isp, data->slave_addr << 1, COMMAND_REG2);
		isp_reg_writeb(isp, data->reg_num, COMMAND_REG3);
	} else {
		isp_reg_writeb(isp, 0x00, COMMAND_REG1);
		isp_reg_writeb(isp, 0x00, COMMAND_REG2);
		isp_reg_writeb(isp, 0x00, COMMAND_REG3);
	}
	if(!isp->bypass&&!isp->debug.status&&!isp->snapshot) {
		isp_reg_writeb(isp, 0x00, COMMAND_REG1);
		isp_reg_writeb(isp, 0x00, COMMAND_REG2);
		isp_reg_writeb(isp, 0x00, COMMAND_REG3);
	}

	return 0;
}
#if 1
static int isp_set_format(struct isp_device *isp)
{
	isp_intc_enable(isp, MASK_INT_CMDSET);
	isp_i2c_fill_buffer(isp);
	isp_reg_writeb(isp, ISP_CCLK_DIVIDER, COMMAND_REG4);

	if(isp->first_init){
		isp->first_init = 0;
		isp_reg_writeb(isp, 0x80, COMMAND_REG5);
	}
	else if(isp->snapshot){
		isp_reg_writeb(isp, 0x00, COMMAND_REG5);
	}
	else {
		isp_reg_writeb(isp, 0x00, COMMAND_REG5);
	}

	{	/*pzqi add*/
		isp_reg_writeb(isp, 0x00, COMMAND_REG6);
		if(isp->parm.out_videos == 1)
			isp_reg_writeb(isp, 0x01, COMMAND_REG7);
		else
			isp_reg_writeb(isp, 0x02, COMMAND_REG7);
	}
	if (isp_send_cmd(isp, CMD_SET_FORMAT, ISP_FORMAT_TIMEOUT)) {
		ISP_PRINT(ISP_ERROR, KERN_ERR "Failed to wait format set done!\n");
		return -EINVAL;
	}

	{
		unsigned int save_reg = isp_reg_readb(isp, 0x63047);
		save_reg |= (0x1 << 2);
		isp_reg_writeb(isp, save_reg, 0x63047);
		ISP_PRINT(ISP_INFO, "---- IDI reset ----\n");
		ISP_PRINT(ISP_INFO, "pcnt_crop_16:			0x%02x%02x\n", isp_reg_readb(isp, 0x63c42), isp_reg_readb(isp, 0x63c43));
		ISP_PRINT(ISP_INFO, "lcnt_crop_16:			0x%02x%02x\n", isp_reg_readb(isp, 0x63c44), isp_reg_readb(isp, 0x63c45));
		save_reg = isp_reg_readb(isp, 0x63047);

		save_reg &= ~(0x1 << 2);
		isp_reg_writeb(isp, save_reg, 0x63047);
		mdelay(600);
		ISP_PRINT(ISP_INFO, "---- IDI reset ok ----\n");
		ISP_PRINT(ISP_INFO, "pcnt_crop_16:			0x%02x%02x\n", isp_reg_readb(isp, 0x63c42), isp_reg_readb(isp, 0x63c43));
		ISP_PRINT(ISP_INFO, "lcnt_crop_16:			0x%02x%02x\n", isp_reg_readb(isp, 0x63c44), isp_reg_readb(isp, 0x63c45));
	}

	return 0;
}
#endif
static int isp_set_capture(struct isp_device *isp)
{
	struct ovisp_camera_dev *camera_dev;
	camera_dev = (struct ovisp_camera_dev *)(isp->data);
	isp_i2c_fill_buffer(isp);
	isp_reg_writeb(isp, ISP_CCLK_DIVIDER, COMMAND_REG4);
	isp_reg_writeb(isp, 0x40, COMMAND_REG5);

	/* Wait for command set successfully. */
	if (isp_send_cmd(isp, CMD_CAPTURE, ISP_CAPTURE_TIMEOUT)) {
		ISP_PRINT(ISP_ERROR, KERN_ERR "Failed to wait capture set done!\n");
		return -EINVAL;
	}

	return 0;
}

static int isp_get_zoom_ratio(struct isp_device *isp, int zoom)
{
	int zoom_ratio;

	switch (zoom) {
	case ZOOM_LEVEL_5:
		zoom_ratio = 0x350;
		break;
	case ZOOM_LEVEL_4:
		zoom_ratio = 0x300;
		break;
	case ZOOM_LEVEL_3:
		zoom_ratio = 0x250;
		break;
	case ZOOM_LEVEL_2:
		zoom_ratio = 0x200;
		break;
	case ZOOM_LEVEL_1:
		zoom_ratio = 0x140;
		break;
	case ZOOM_LEVEL_0:
	default:
		zoom_ratio = 0x100;
		break;
	}

	return zoom_ratio;
}

static int isp_set_zoom(struct isp_device *isp, int zoom)
{
	int zoom_ratio;

	zoom_ratio = isp_get_zoom_ratio(isp, zoom);

	isp_reg_writeb(isp, 0x01, COMMAND_REG1);
	isp_reg_writeb(isp, zoom_ratio >> 8, COMMAND_REG2);
	isp_reg_writeb(isp, zoom_ratio & 0xFF, COMMAND_REG3);
	isp_reg_writeb(isp, zoom_ratio >> 8, COMMAND_REG4);
	isp_reg_writeb(isp, zoom_ratio & 0xFF, COMMAND_REG5);

	/* Wait for command set successfully. */
	if (isp_send_cmd(isp, CMD_ZOOM_IN_MODE, ISP_ZOOM_TIMEOUT)) {
		ISP_PRINT(ISP_ERROR, KERN_ERR "Failed to wait zoom set done!\n");
		return -EINVAL;
	}

	return 0;
}

static void fw_copy(unsigned int *dst,unsigned int *src, int cnt)
{
	int i = 0;

	for (i = 0; i < cnt; i++) {
		unsigned char *dt = (unsigned char *)dst;
		unsigned char *st = (unsigned char *)src;

		dt[3] = st[0];
		dt[2] = st[1];
		dt[1] = st[2];
		dt[0] = st[3];

		dst++;
		src++;
	}
}

static int isp_boot(struct isp_device *isp)
{
	unsigned char val;

	if (isp->boot)
		return 0;

	/* Mask all interrupts. */
	isp_intc_disable(isp, 0xffff);
	isp_mac_int_mask(isp, 0xffff);

	/* Reset ISP.  */
	isp_reg_writeb(isp, DO_SOFT_RST, REG_ISP_SOFT_RST);
	printk("REG_ISP_SOFT_RST:%x\n", isp_reg_readb(isp, REG_ISP_SOFT_RST));

	/* Enable interrupt (only set_cmd_done interrupt). */
	isp_intc_enable(isp, MASK_INT_CMDSET);

	isp_reg_writeb(isp, DO_SOFTWARE_STAND_BY, REG_ISP_SOFT_STANDBY);

	printk("REG_ISP_SOFT_STANDBY:%x\n", isp_reg_readb(isp, REG_ISP_SOFT_STANDBY));
	/* Enable the clk used by mcu. */
	isp_reg_writeb(isp, 0xf1, REG_ISP_CLK_USED_BY_MCU);
	printk("REG_ISP_CLK_USED_BY_MCU:%x\n", isp_reg_readb(isp, REG_ISP_CLK_USED_BY_MCU));
	/* Download firmware to ram of mcu. */
#ifdef OVISP_DEBUGTOOL_ENABLE
	ovisp_debugtool_load_firmware(OVISP_DEBUGTOOL_FIRMWARE_FILENAME, (u8*)(isp->base + FIRMWARE_BASE), (u8*)isp_firmware, ARRAY_SIZE(isp_firmware));
#else
	fw_copy((unsigned int *)(isp->base + FIRMWARE_BASE), (unsigned int *)isp_firmware,
			ARRAY_SIZE(isp_firmware) / 4);
#endif

	/* MCU initialize. */
	isp_reg_writeb(isp, 0xf0, REG_ISP_CLK_USED_BY_MCU);

	printk("REG_ISP_CLK_USED_BY_MCU:%x\n", isp_reg_readb(isp, REG_ISP_CLK_USED_BY_MCU));

	/* Wait for command set done interrupt. */
	if (isp_wait_cmd_done(isp, ISP_BOOT_TIMEOUT)) {
		ISP_PRINT(ISP_ERROR, KERN_ERR "MCU not respond when init ISP!\n");
		return -ETIME;
	}

	val = isp_reg_readb(isp, COMMAND_FINISHED);
	if (val != CMD_FIRMWARE_DOWNLOAD) {
		ISP_PRINT(ISP_ERROR, KERN_ERR "Failed to download isp firmware (%02x)\n",
				val);
		return -EINVAL;
	}

	isp_reg_writeb(isp, DO_SOFTWARE_STAND_BY, REG_ISP_SOFT_STANDBY);
	isp_i2c_config(isp);


	ISP_PRINT(ISP_INFO,"versionh  %08x\n",isp_reg_readb(isp, 0x6303d));
	ISP_PRINT(ISP_INFO,"versionl  %08x\n",isp_reg_readb(isp, 0x6303e));

	return 0;
}

static int isp_irq_notify(struct isp_device *isp, unsigned int status)
{
	int ret = 0;

	if (isp->irq_notify)
		ret = isp->irq_notify(status, isp->data);

	return ret;
}
static unsigned long long start_time;
static unsigned long long end_time;
static irqreturn_t isp_irq(int this_irq, void *dev_id)
{
	struct isp_device *isp = dev_id;
	unsigned short irq_status;
	unsigned short mac_irq_status = 0;
	unsigned int notify = 0;
	//start_time = sched_clock();
	irq_status = isp_intc_state(isp);
	mac_irq_status = isp_mac_int_state(isp);
	/* Command set done interrupt. */
	if (irq_status & MASK_INT_CMDSET) {
		complete(&isp->completion);
	}
	if (irq_status & MASK_INT_MAC) {
		//	mac_irq_status = isp_mac_int_state(isp);

		/* Drop. */
		if (mac_irq_status & (MASK_INT_DROP0 | MASK_INT_DROP1)){
			if(mac_irq_status & MASK_INT_DROP0){
				notify |= ISP_NOTIFY_DROP_FRAME | ISP_NOTIFY_DROP_FRAME0;
				ISP_PRINT(ISP_INFO,"drop 0 !!\n");
				//isp->pp_buf = true;
			}
			if(mac_irq_status & MASK_INT_DROP1){
				notify |= ISP_NOTIFY_DROP_FRAME | ISP_NOTIFY_DROP_FRAME1;
				ISP_PRINT(ISP_INFO,"drop 1 !!\n");
				//isp->pp_buf = false;
			}
		}
		/* Done. */
		if (mac_irq_status & (MASK_INT_WRITE_DONE0 | MASK_INT_WRITE_DONE1)){
			if (mac_irq_status & MASK_INT_WRITE_DONE0) {
				notify |= ISP_NOTIFY_DATA_DONE | ISP_NOTIFY_DATA_DONE0;
				ISP_PRINT(ISP_INFO,"done - 0!\n");
				//isp->pp_buf = false;
			}
			else if (mac_irq_status & MASK_INT_WRITE_DONE1) {
				notify |= ISP_NOTIFY_DATA_DONE | ISP_NOTIFY_DATA_DONE1;
				ISP_PRINT(ISP_INFO,"done - 1!\n");
				//isp->pp_buf = true;
			}
#if 0
			//dump_mac(isp);
			//dump_isp_debug_regs(isp);

			//		ISP_PRINT(ISP_INFO, "1C056 IS %#x\n",isp_firmware_readb(isp, 0x1c056));
			/*dump_isp_exposure(isp);*/

			//__dump_isp_regs(isp, 0x63b00, 0x63bd2);
			//dump_isp_configs(isp);
			dump_firmware_reg(isp, 0x1e010, 0x16);
			dump_firmware_reg(isp, 0x1e056, 0x12);
			dump_firmware_reg(isp, 0x1e070, 8);
			__dump_isp_regs(isp, 0x66500, 0x34);
			ISP_PRINT(ISP_INFO,"1e022 IS %#x\n", isp_firmware_readb(isp, 0x1e022));
			dump_firmware_reg(isp, 0x1e056, 18);
			ISP_PRINT(ISP_INFO,"65003 IS %#x\n", isp_reg_readb(isp, 0x65003));
			dump_firmware_reg(isp, 0x1e030, 6);
			/*dump_isp_configs(isp);*/
			/*dump_isp_range_regs(isp,0x1ee90,0x0f);*/
			/*dump_isp_debug_regs(isp);*/
#endif
		}
		/* FIFO overflow */
		if (mac_irq_status & (MASK_INT_OVERFLOW0 | MASK_INT_OVERFLOW1)) {
			ISP_PRINT(ISP_WARNING,"overflow\n");
			notify |= ISP_NOTIFY_OVERFLOW;

		}

		if(mac_irq_status & MASK_INT_WRITE_START0) {
			ISP_PRINT(ISP_INFO,"start 0\n");
			notify |= ISP_NOTIFY_DATA_START | ISP_NOTIFY_DATA_START0;
			/*dump_firmware_reg(isp, 0x1ee90, 12);*/
		}
		if(mac_irq_status & MASK_INT_WRITE_START1) {
			ISP_PRINT(ISP_INFO,"start 1\n");
			notify |= ISP_NOTIFY_DATA_START | ISP_NOTIFY_DATA_START1;
			/*dump_firmware_reg(isp, 0x1ee90, 12);*/
		}
	}
//	end_time = sched_clock();
//	printk("()time0 = %lldns\n", (end_time - start_time));
//	start_time = sched_clock();
	isp_irq_notify(isp, notify);
//	end_time = sched_clock();
//	printk("()time1 = %lldns\n", (end_time - start_time));
	//printk("^^*^^\n");
	return IRQ_HANDLED;
}

static int isp_mipi_init(struct isp_device *isp)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(isp_mipi_regs_init); i++) {
		isp_reg_writeb(isp, isp_mipi_regs_init[i].value,
				isp_mipi_regs_init[i].reg);
	}

#ifdef OVISP_DEBUGTOOL_ENABLE
	ovisp_debugtool_load_isp_setting(isp, OVISP_DEBUGTOOL_ISPSETTING_FILENAME);
#endif

	return 0;
}

static int isp_dvp_init(struct isp_device *isp)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(isp_dvp_regs_init); i++) {
		isp_reg_writeb(isp, isp_dvp_regs_init[i].value,
				isp_dvp_regs_init[i].reg);
	}

	return 0;
}

static int isp_int_init(struct isp_device *isp)
{
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&isp->lock, flags);

	isp->intr_l = 0;
	isp->intr_h = 0;
	isp->mac_intr_l = 0;
	isp->mac_intr_h = 0;
	isp_reg_writeb(isp, 0x00, REG_ISP_INT_EN_H);
	isp_reg_writeb(isp, 0x00, REG_ISP_INT_EN_L);
	isp_reg_writeb(isp, 0x00, REG_ISP_MAC_INT_EN_L);
	isp_reg_writeb(isp, 0x00, REG_ISP_MAC_INT_EN_H);

	spin_unlock_irqrestore(&isp->lock, flags);

	ret = request_irq(isp->irq, isp_irq, IRQF_SHARED,
			"isp", isp);

	return ret;
}

static int isp_i2c_init(struct isp_device *isp)
{
	int ret;
	if (isp->pdata->flags & CAMERA_USE_ISP_I2C) {
		ISP_PRINT(ISP_INFO,"CAMERA USE ISP I2C, NOT SOC I2C\n");
		isp->i2c.xfer_cmd = isp_i2c_xfer_cmd_sccb;
		ret = isp_i2c_register(isp);
		if (ret)
			return ret;
	} else {
		ISP_PRINT(ISP_INFO,"CAMERA USE SOC I2C\n");
	}
	return 0;
}

static int isp_i2c_release(struct isp_device *isp)
{
	if (isp->pdata->flags & CAMERA_USE_ISP_I2C) {
		isp_i2c_unregister(isp);
	} else {
		platform_device_register(to_platform_device(isp->dev));
		kfree(isp->i2c.pdata);
	}

	return 0;
}

static int isp_mfp_init(struct isp_device *isp)
{
	return 0;
}

static int isp_clk_init(struct isp_device *isp)
{
	int i;
	int ret;

	for (i = 0; i < ISP_CLK_NUM; i++) {
		isp->clk[i] = clk_get(isp->dev, isp_clks[i].name);
		if (IS_ERR(isp->clk[i])) {
			ISP_PRINT(ISP_ERROR, KERN_ERR "Failed to get %s clock %ld\n",
					isp_clks[i].name, PTR_ERR(isp->clk[i]));
			return PTR_ERR(isp->clk[i]);
		}
		if(isp_clks[i].rate != DUMMY_CLOCK_RATE) {
			ret = clk_set_rate(isp->clk[i], isp_clks[i].rate);
			if(ret){
				printk("set rate failed ! XXXXXXXXXXXXXXXXXx \n");
			}
		}

		isp->clk_enable[i] = 0;
	}

	return 0;
}

static int isp_clk_release(struct isp_device *isp)
{
	int i;

	for (i = 0; i < ISP_CLK_NUM; i++)
		clk_put(isp->clk[i]);

	return 0;
}

static int isp_clk_enable(struct isp_device *isp, unsigned int type)
{
	int i;

	for (i = 0; i < ISP_CLK_NUM; i++) {
		if (!isp->clk_enable[i] && (isp_clks[i].flags & type)) {
			clk_enable(isp->clk[i]);
			isp->clk_enable[i] = 1;
		}
	}

	return 0;
}

static int isp_clk_disable(struct isp_device *isp, unsigned int type)
{
	int i;

	for (i = 0; i < ISP_CLK_NUM; i++) {
		if (isp->clk_enable[i] && (isp_clks[i].flags & type)) {
			clk_disable(isp->clk[i]);
			isp->clk_enable[i] = 0;
		}
	}

	return 0;
}


static int isp_powerdown(struct isp_device * isp)
{

	return 0;
}

static int isp_powerup(struct isp_device * isp)
{
	return 0;
}

static int isp_init(struct isp_device *isp, void *data)
{
	isp->boot = 0;
	isp->poweron = 1;
	isp->snapshot = 0;
	isp->bypass = 0;
	isp->running = 0;
	isp->format_active = 0;
	isp->bracket_end = 0;
	memset(&isp->parm, 0, sizeof(isp->parm));

	isp_mfp_init(isp);

	return 0;
}

static int isp_release(struct isp_device *isp, void *data)
{
	isp->boot = 0;
	isp->poweron = 0;
	isp->snapshot = 0;
	isp->bypass = 0;
	isp->running = 0;
	isp->format_active = 0;

	return 0;
}

static int isp_open(struct isp_device *isp, struct isp_prop *prop)
{
	struct ovisp_camera_client *client = &isp->pdata->client[prop->index];
	int ret = 0;

	if (!isp->poweron)
		return -ENODEV;
	isp_powerup(isp);
	isp_clk_enable(isp, ISP_CLK_CGU_ISP | ISP_CLK_GATE_ISP);

	/*check if clk ok!*/
	printk("cpm cpccr:%x\n", *((volatile unsigned int *)0xB0000000));
	printk("isp cpm regs:%08x\n", *((volatile unsigned int *)0xB0000080));

	if (!isp->boot) {
		ret = isp_boot(isp);
		if (ret)
			return ret;
		isp->boot = 1;
	}

	if (client->flags & CAMERA_CLIENT_IF_MIPI) {
		isp_clk_enable(isp, ISP_CLK_GATE_CSI);
		ret = isp_mipi_init(isp);
	} else if (client->flags & CAMERA_CLIENT_IF_DVP) {
		ret = isp_dvp_init(isp);
	}

	isp->debug.status = 1;
	isp->input = prop->index;
	isp->bypass = prop->bypass;
	isp->snapshot = 0;
	isp->format_active = 0;
	memset(&isp->fmt_data, 0, sizeof(isp->fmt_data));

	isp->pp_buf = true;
	isp->first_init = 1;

	//isp_dump_firmware(isp);
	//isp_dump_configuration(isp);

	memset(&isp->debug, 0, sizeof(isp->debug));
	return ret;
}

static int isp_close(struct isp_device *isp, struct isp_prop *prop)
{
	struct ovisp_camera_client *client = &isp->pdata->client[prop->index];

	/*clk should be disabled here, but error happens, add by pzqi*/
	/* wait for mac wirte ram finish */
	msleep(80);

	if (!isp->poweron)
		return -ENODEV;
	if (client->flags & CAMERA_CLIENT_IF_MIPI) {
		csi_phy_stop(prop->index);
		isp_clk_disable(isp, ISP_CLK_GATE_CSI);
	}


	/*disable interrupt*/
	isp_reg_writeb(isp, 0x00, REG_ISP_INT_EN_H);
	isp_reg_writeb(isp, 0x00, REG_ISP_INT_EN_L);
	isp_reg_writeb(isp, 0x00, REG_ISP_MAC_INT_EN_H);
	isp_reg_writeb(isp, 0x00, REG_ISP_MAC_INT_EN_L);

	isp_powerdown(isp);

	isp_clk_disable(isp, ISP_CLK_GATE_ISP);

	/*for fpga test*/
	isp_clk_disable(isp, ISP_CLK_CGU_ISP);

	/*release the subdev index*/
	isp->input = -1;

	return 0;
}

static int isp_config(struct isp_device *isp, void *data)
{
	return 0;
}

static int isp_suspend(struct isp_device *isp, void *data)
{
	return 0;
}

static int isp_resume(struct isp_device *isp, void *data)
{
	return 0;
}

static int isp_mclk_on(struct isp_device *isp, int index)
{
	/*ovisp_mfp_config(MFP_PIN_GPIO72, MFP_GPIO72_ISP_SCLK0);*/
	//isp_clk_enable(isp, ISP_CLK_DEV);
	//isp_clk_enable(isp, ISP_CLK_ALL);

	return 0;
}

static int isp_mclk_off(struct isp_device *isp, int index)
{
//	isp_clk_disable(isp, ISP_CLK_GATE_ISP);
	/*ovisp_mfp_config(MFP_PIN_GPIO72, MFP_PIN_MODE_GPIO);*/

	return 0;
}

static int isp_update_buffer(struct isp_device *isp, struct isp_buffer *buf, int index)
{
	if(buf->addr == 0)
		return -1;
	if(index == 0){
		isp_set_address(isp, 0, buf->addr);
		isp_reg_writeb(isp, 0x01, REG_BASE_ADDR_READY);
	}else{
		isp_set_address(isp, 1, buf->addr);
		isp_reg_writeb(isp, 0x02, REG_BASE_ADDR_READY);
	}

	return 0;
}

int isp_set_capture_raw_test(struct isp_device *isp)
{
	isp_firmware_writeb(isp, 0x01, 0x1fff9);
	/*0x48: isp enable, 0x08, isp bypass*/
	isp_firmware_writeb(isp, 0x00, 0x1f000);
	isp_firmware_writeb(isp, 0x48, 0x1f001);

	/*640 * 480*/
	isp_firmware_writeb(isp, 0x02, 0x1f002); /*sensor output size*/
	isp_firmware_writeb(isp, 0x80, 0x1f003);
	isp_firmware_writeb(isp, 0x01, 0x1f004);
	isp_firmware_writeb(isp, 0xe0, 0x1f005);

	/*idi control*/
	isp_firmware_writeb(isp, 0x00, 0x1f006);
	isp_firmware_writeb(isp, 0x00, 0x1f007);

	/* 640*480 */
	isp_firmware_writeb(isp, 0x02, 0x1f008); /*idi output*/
	isp_firmware_writeb(isp, 0x80, 0x1f009);
	isp_firmware_writeb(isp, 0x01, 0x1f00a);
	isp_firmware_writeb(isp, 0xe0, 0x1f00b);

	isp_firmware_writeb(isp, 0x00, 0x1f00c); /*start crop position,x,y*/
	isp_firmware_writeb(isp, 0x00, 0x1f00d);
	isp_firmware_writeb(isp, 0x00, 0x1f00e);
	isp_firmware_writeb(isp, 0x00, 0x1f00f);

	// video 16
	/* output type
	 * 0:RAW8
	 * 4:yuv422
	 * */
	/*output 1*/
	isp_firmware_writeb(isp, 0x00, 0x1f022);
	isp_firmware_writeb(isp, 0x00, 0x1f023);
	/* 640*480 */
	isp_firmware_writeb(isp, 0x02, 0x1f024);
	isp_firmware_writeb(isp, 0x80, 0x1f025);
	isp_firmware_writeb(isp, 0x01, 0x1f026);
	isp_firmware_writeb(isp, 0xe0, 0x1f027);/*w:h*/

	/* memory width */
	isp_firmware_writeb(isp, 0x02, 0x1f028);
	isp_firmware_writeb(isp, 0x80, 0x1f029);/*m_y*/
	isp_firmware_writeb(isp, 0x06, 0x1fff9);

	/* set cmd register */
	isp_reg_writeb(isp, 0x01, 0x63901); /*enable */

	/* Wait for command set successfully. */
	if (isp_send_cmd(isp, CMD_CAPTURE_RAW, ISP_FORMAT_TIMEOUT)) {
		ISP_PRINT(ISP_ERROR, KERN_ERR "Failed to wait capture raw done!\n");
		return -EINVAL;
	}

	ISP_PRINT(ISP_INFO, "capture raw command send successed!\n");
	return 0;
}
int isp_zoom_in_test(struct isp_device * isp)
{
	/* set cmd register */
	//isp_reg_writeb(isp, 0x89, 0x63901);
	isp_reg_writeb(isp, 0x03, 0x63902);
	isp_reg_writeb(isp, 0x80, 0x63903);
	/* Wait for command set successfully. */
	if (isp_send_cmd(isp, CMD_ZOOM_IN_MODE, ISP_FORMAT_TIMEOUT)) {
		ISP_PRINT(ISP_ERROR, KERN_ERR "Failed to wait capture done!\n");
		return -EINVAL;
	}
	return 0;
}
int isp_normal_capture_test(struct isp_device * isp)
{
	unsigned long addr = isp->buf_start.addr;

	isp_firmware_writeb(isp, 0x01, 0x1fff9);
	/*0x48: isp enable, 0x08, isp bypass*/
	isp_firmware_writeb(isp, 0x00, 0x1f000);
	isp_firmware_writeb(isp, 0x48, 0x1f001);

	/*640 * 480*/
	isp_firmware_writeb(isp, 0x02, 0x1f002); /*sensor output size*/
	isp_firmware_writeb(isp, 0x80, 0x1f003);
	isp_firmware_writeb(isp, 0x01, 0x1f004);
	isp_firmware_writeb(isp, 0xe0, 0x1f005);

	isp_firmware_writeb(isp, 0x00, 0x1f006);
	isp_firmware_writeb(isp, 0x00, 0x1f007);

	/* 640*480 */
	isp_firmware_writeb(isp, 0x02, 0x1f008); /*idi output*/
	isp_firmware_writeb(isp, 0x80, 0x1f009);
	isp_firmware_writeb(isp, 0x01, 0x1f00a);
	isp_firmware_writeb(isp, 0xe0, 0x1f00b);

	isp_firmware_writeb(isp, 0x00, 0x1f00c); /*start crop position,x,y*/
	isp_firmware_writeb(isp, 0x00, 0x1f00d);
	isp_firmware_writeb(isp, 0x00, 0x1f00e);
	isp_firmware_writeb(isp, 0x00, 0x1f00f);

	// video 16
	/* output type
	 * 0:RAW8
	 * 4:yuv422
	 * 8:yuv420
	 * */
	isp_firmware_writeb(isp, 0x00, 0x1f022);
	isp_firmware_writeb(isp, 0x04, 0x1f023);

	/* iamge output */
	//set_oimage_size1(isp, ); /* modify by hyli */
	/* 640*480 */
	isp_firmware_writeb(isp, 0x02, 0x1f024);
	isp_firmware_writeb(isp, 0x80, 0x1f025);
	isp_firmware_writeb(isp, 0x01, 0x1f026);
	isp_firmware_writeb(isp, 0xe0, 0x1f027);/*w:h*/

	/* memory width */
	isp_firmware_writeb(isp, 0x02, 0x1f028);
	isp_firmware_writeb(isp, 0x80, 0x1f029);/*m_y*/
	isp_firmware_writeb(isp, 0x01, 0x1f02a);
	isp_firmware_writeb(isp, 0xe0, 0x1f02b);/*m_uv*/
	// video 64
	isp_firmware_writeb(isp, 0x00, 0x1f02c);
	isp_firmware_writeb(isp, 0x04, 0x1f02d);/**/

	/* image output */
	//640*480
	isp_firmware_writeb(isp, 0x02, 0x1f02e);
	isp_firmware_writeb(isp, 0x80, 0x1f02f);
	isp_firmware_writeb(isp, 0x01, 0x1f030);
	isp_firmware_writeb(isp, 0xe0, 0x1f031);/*w:h*/

	/* memory width*/
	isp_firmware_writeb(isp, 0x02, 0x1f032);
	isp_firmware_writeb(isp, 0x80, 0x1f033);/*m_y*/
	isp_firmware_writeb(isp, 0x02, 0x1f034);
	isp_firmware_writeb(isp, 0x80, 0x1f035);/*m_uv*/

	//bracket ratio 1x
	isp_firmware_writeb(isp, 0x01, 0x1f080);
	isp_firmware_writeb(isp, 0x00, 0x1f081);
	isp_firmware_writeb(isp, 0x01, 0x1f082);
	isp_firmware_writeb(isp, 0x00, 0x1f083);

	//zoom in 1x
	isp_firmware_writeb(isp, 0x01, 0x1f084);
	isp_firmware_writeb(isp, 0x00, 0x1f085);

	//isp config
	isp_firmware_writeb(isp, 0x01, 0x1f070);
	isp_firmware_writeb(isp, 0x00, 0x1f071);
	isp_firmware_writeb(isp, 0x03, 0x1f072);
	isp_firmware_writeb(isp, 0x46, 0x1f073);

	isp_firmware_writeb(isp, 0x00, 0x1f074);
	isp_firmware_writeb(isp, 0x10, 0x1f075);
	isp_firmware_writeb(isp, 0x00, 0x1f076);
	isp_firmware_writeb(isp, 0xff, 0x1f077);

	isp_firmware_writeb(isp, 0x00, 0x1f078);
	isp_firmware_writeb(isp, 0x10, 0x1f079);
	//isp_firmware_writeb(isp, 0x32, 0x1f07a);
	//isp_firmware_writeb(isp, 0x00, 0x1f07b);
	isp_firmware_writeb(isp, 0x03, 0x1f07a);
	isp_firmware_writeb(isp, 0x21, 0x1f07b);

	/* configuer output addr */
	isp_firmware_writeb(isp, (addr >> 24) & 0xff, 0x1f0a0);
	isp_firmware_writeb(isp, (addr >> 16) & 0xff, 0x1f0a1);
	isp_firmware_writeb(isp, (addr >> 8) & 0xff, 0x1f0a2);
	isp_firmware_writeb(isp, (addr >> 0) & 0xff, 0x1f0a3);

	isp_firmware_writeb(isp, 0x06, 0x1fff9);

	/* set cmd register */
	isp_reg_writeb(isp, 0x89, 0x63901);
	isp_reg_writeb(isp, 0x20, 0x63902);
	isp_reg_writeb(isp, 0x00, 0x63903);
	isp_reg_writeb(isp, 0x02, 0x63904);

	if(isp->first_init){
		isp->first_init = 0;
		isp_reg_writeb(isp, 0x80, COMMAND_REG5);
	}
	else {
		isp_reg_writeb(isp, 0x00, COMMAND_REG5);
	}
	isp_reg_writeb(isp, 0x01, 0x63907);/*video num*/


	/* Wait for command set successfully. */
	if (isp_send_cmd(isp, CMD_CAPTURE_IMAGE, ISP_FORMAT_TIMEOUT)) {
		ISP_PRINT(ISP_ERROR, KERN_ERR "Failed to wait capture done!\n");
		return -EINVAL;
	}

	return 0;
}

static int isp_offline_process( struct isp_device * isp, void *pridata, void *data)
{
	return 0;
}

static void isp_set_exposure_init(struct isp_device *isp)
{
	isp_reg_writeb(isp, 0x3f, 0x65000);
	isp_reg_writeb(isp, 0xef, 0x65001);
	isp_reg_writeb(isp, 0x25, 0x65002);
	isp_reg_writeb(isp, 0xff, 0x65003);
	isp_reg_writeb(isp, 0x30, 0x65004);
	isp_reg_writeb(isp, 0x14, 0x65005);
	isp_reg_writeb(isp, 0xc0, 0x6502f);

	isp_firmware_writeb(isp, 0x01, 0x1e010);
	isp_firmware_writeb(isp, 0x02, 0x1e012);
	isp_firmware_writeb(isp, 0x30, 0x1e014);
	isp_firmware_writeb(isp, 0x35, 0x1e015);
	isp_firmware_writeb(isp, 0x0a, 0x1e01a);
	isp_firmware_writeb(isp, 0x08, 0x1e01b);
	isp_firmware_writeb(isp, 0x00, 0x1e024);
	isp_firmware_writeb(isp, 0xff, 0x1e025);
	isp_firmware_writeb(isp, 0x00, 0x1e026);
	isp_firmware_writeb(isp, 0x10, 0x1e027);
	isp_firmware_writeb(isp, 0x00, 0x1e028);
	isp_firmware_writeb(isp, 0x00, 0x1e029);
	isp_firmware_writeb(isp, 0x03, 0x1e02a);
	isp_firmware_writeb(isp, 0xd8, 0x1e02b);
	isp_firmware_writeb(isp, 0x00, 0x1e02c);
	isp_firmware_writeb(isp, 0x00, 0x1e02d);
	isp_firmware_writeb(isp, 0x00, 0x1e02e);
	isp_firmware_writeb(isp, 0x10, 0x1e02f);
	isp_firmware_writeb(isp, 0x00, 0x1e048);
	isp_firmware_writeb(isp, 0x01, 0x1e049);
	isp_firmware_writeb(isp, 0x01, 0x1e04a);
	isp_firmware_writeb(isp, 0xe8, 0x1e04f);
	isp_firmware_writeb(isp, 0x18, 0x1e050);
	isp_firmware_writeb(isp, 0x1e, 0x1e051);
	isp_firmware_writeb(isp, 0x03, 0x1e04c);
	isp_firmware_writeb(isp, 0xd8, 0x1e04d);
	isp_firmware_writeb(isp, 0x08, 0x1e013);

	isp_firmware_writeb(isp, 0x78, 0x1e056);

	isp_firmware_writeb(isp, 0x09, 0x1e057);
	isp_firmware_writeb(isp, 0x35, 0x1e058);
	isp_firmware_writeb(isp, 0x00, 0x1e059);
	isp_firmware_writeb(isp, 0x35, 0x1e05a);
	isp_firmware_writeb(isp, 0x01, 0x1e05b);
	isp_firmware_writeb(isp, 0x35, 0x1e05c);
	isp_firmware_writeb(isp, 0x02, 0x1e05d);
	isp_firmware_writeb(isp, 0x00, 0x1e05e);
	isp_firmware_writeb(isp, 0x00, 0x1e05f);
	isp_firmware_writeb(isp, 0x00, 0x1e060);
	isp_firmware_writeb(isp, 0x00, 0x1e061);
	isp_firmware_writeb(isp, 0x00, 0x1e062);
	isp_firmware_writeb(isp, 0x00, 0x1e063);
	isp_firmware_writeb(isp, 0x35, 0x1e064);
	isp_firmware_writeb(isp, 0x0a, 0x1e065);
	isp_firmware_writeb(isp, 0x35, 0x1e066);
	isp_firmware_writeb(isp, 0x0b, 0x1e067);
	isp_firmware_writeb(isp, 0xff, 0x1e070);
	isp_firmware_writeb(isp, 0xff, 0x1e071);
	isp_firmware_writeb(isp, 0xff, 0x1e072);
	isp_firmware_writeb(isp, 0x00, 0x1e073);
	isp_firmware_writeb(isp, 0x00, 0x1e074);
	isp_firmware_writeb(isp, 0x00, 0x1e075);
	isp_firmware_writeb(isp, 0xff, 0x1e076);
	isp_firmware_writeb(isp, 0xff, 0x1e077);

	isp_reg_writeb(isp, 0x00, 0x66501);
	isp_reg_writeb(isp, 0x00, 0x66502);
	isp_reg_writeb(isp, 0x00, 0x66503);
	isp_reg_writeb(isp, 0x00, 0x66504);
	isp_reg_writeb(isp, 0x00, 0x66505);
	isp_reg_writeb(isp, 0x00, 0x66506);
	isp_reg_writeb(isp, 0x00, 0x66507);
	isp_reg_writeb(isp, 0x80, 0x66508);
	isp_reg_writeb(isp, 0x00, 0x66509);
	isp_reg_writeb(isp, 0xc8, 0x6650a);
	isp_reg_writeb(isp, 0x00, 0x6650b);
	isp_reg_writeb(isp, 0x96, 0x6650c);
	isp_reg_writeb(isp, 0x04, 0x6650d);
	isp_reg_writeb(isp, 0xb0, 0x6650e);
	isp_reg_writeb(isp, 0x01, 0x6650f);
	isp_reg_writeb(isp, 0x00, 0x66510);
	isp_reg_writeb(isp, 0x01, 0x6651c);
	isp_reg_writeb(isp, 0x01, 0x6651d);
	isp_reg_writeb(isp, 0x01, 0x6651e);
	isp_reg_writeb(isp, 0x01, 0x6651f);
	isp_reg_writeb(isp, 0x02, 0x66520);
	isp_reg_writeb(isp, 0x02, 0x66521);
	isp_reg_writeb(isp, 0x02, 0x66522);
	isp_reg_writeb(isp, 0x02, 0x66523);
	isp_reg_writeb(isp, 0x04, 0x66524);
	isp_reg_writeb(isp, 0x02, 0x66525);
	isp_reg_writeb(isp, 0x02, 0x66526);
	isp_reg_writeb(isp, 0x02, 0x66527);
	isp_reg_writeb(isp, 0x02, 0x66528);
	isp_reg_writeb(isp, 0x04, 0x66529);
	isp_reg_writeb(isp, 0xf0, 0x6652a);
	isp_reg_writeb(isp, 0x2b, 0x6652c);

	isp_reg_writeb(isp, 0x2b, 0x6652c);

	isp_reg_writeb(isp, 0x2c, 0x6652d);
	isp_reg_writeb(isp, 0x2d, 0x6652e);
	isp_reg_writeb(isp, 0x2e, 0x6652f);
	isp_reg_writeb(isp, 0x2f, 0x66530);
	isp_reg_writeb(isp, 0x0a, 0x66531);
	isp_reg_writeb(isp, 0x14, 0x66532);
	isp_reg_writeb(isp, 0x14, 0x66533);

	isp_firmware_writeb(isp, 0x01, 0x1e022);
	isp_firmware_writeb(isp, 0x00, 0x1e030);
	isp_firmware_writeb(isp, 0x00, 0x1e031);
	isp_firmware_writeb(isp, 0x34, 0x1e032);
	isp_firmware_writeb(isp, 0x60, 0x1e033);
	isp_firmware_writeb(isp, 0x00, 0x1e034);
	isp_firmware_writeb(isp, 0x40, 0x1e035);

}

static int isp_start_capture(struct isp_device *isp, struct isp_capture *cap)
{
	int ret = 0;
	struct ovisp_camera_dev *camera_dev;
	struct ovisp_camera_frame frame;

	camera_dev = (struct ovisp_camera_dev *)(isp->data);
	frame = camera_dev->frame;
	isp->snapshot = cap->snapshot;
	isp->client = cap->client;

	printk( "width is %d,height is %d,v4l2_field is %d\n", frame.width,frame.height,frame.field);
	if (isp->format_active) {
		isp_set_parameters(isp);
		if (!isp->snapshot) {
			//isp_set_exposure_init(isp);
			ret = isp_set_format(isp);
		} else {
			ret = isp_set_capture(isp);
		}
		//while(1);
		if (ret)
			return ret;
	}
	if(isp->bypass&&isp->snapshot&&!isp->format_active) {
		ISP_PRINT(ISP_INFO,"--%s: %d\n", __func__, __LINE__);
		isp_set_parameters(isp);
		ret = isp_set_capture(isp);
	}
#ifdef OVISP_CSI_TEST
	printk("csi sensor test ! \n");
	while(1) {
		check_csi_error();
		//mdelay(10);
	}
#endif
	/*mask all interrupt*/
#if 1
	isp_intc_disable(isp, 0xffff);
	isp_mac_int_mask(isp, 0xffff);
	/*read to clear interrupt*/
	isp_intc_state(isp);
	isp_mac_int_state(isp);

	isp_intc_enable(isp, MASK_INT_MAC);
	isp_mac_int_unmask(isp,
			MASK_INT_WRITE_DONE0 | MASK_INT_WRITE_DONE1 |
			MASK_INT_OVERFLOW0 | MASK_INT_OVERFLOW1 |
			MASK_INT_DROP0 | MASK_INT_DROP1 |
			MASK_INT_WRITE_START0 | MASK_INT_WRITE_START1
			);
#endif
	ISP_PRINT(ISP_INFO, "now start capturing , waiting for interrupt\n");
	isp->running = 1;
	/* call the update buffer in drop int */

	isp->hdr_mode = 0;
	isp->bracket_end = 0;
	isp->debug.status = isp->snapshot;
	//__dump_isp_regs(isp, 0x63022, 0x63047);
//	dump_isp_syscontrol(isp);
//	dump_isp_top_register(isp);

	return 0;
}

static int isp_stop_capture(struct isp_device *isp, void *data)
{
	isp->running = 0;
	isp_intc_disable(isp, 0xffff);
	isp_mac_int_mask(isp, 0xffff);
	isp_reg_writeb(isp, 0x00, REG_BASE_ADDR_READY);
	isp_intc_state(isp);
	isp_mac_int_state(isp);

	return 0;
}

static int isp_enable_capture(struct isp_device *isp, struct isp_buffer *buf)

{
	ISP_PRINT(ISP_INFO,"[ISP] %s\n", __func__);
	isp_intc_state(isp);
	isp_mac_int_state(isp);
	isp->buf_start = *buf;

	isp_intc_enable(isp, MASK_INT_MAC);
	isp_mac_int_unmask(isp, (MASK_INT_DROP0 | MASK_INT_DROP1));

	return 0;
}

static int isp_disable_capture(struct isp_device *isp, void *data)
{
	isp_reg_writeb(isp, 0x00, REG_BASE_ADDR_READY);
	ISP_PRINT(ISP_INFO,"[ISP] %s\n", __func__);

	return 0;
}

static int isp_check_fmt(struct isp_device *isp, struct isp_format *f)
{
	if (isp->bypass)
		return 0;

	ISP_PRINT(ISP_INFO, "isp check fmt, f->fourcc:%d\n", f->fourcc);
	switch (f->fourcc) {
	case V4L2_PIX_FMT_YUYV:
		break;
	case V4L2_PIX_FMT_SGBRG8:
		break;
	case V4L2_PIX_FMT_NV12YUV422:
		break;
	case V4L2_PIX_FMT_NV12:
		break;
		/* Now, we don't support yuv420sp. */
	default:
		return -EINVAL;
	}

	return 0;
}

static int isp_try_fmt(struct isp_device *isp, struct isp_format *f)
{
	int ret;

	ret = isp_check_fmt(isp, f);
	if (ret)
		return ret;

	return 0;
}

static int isp_pre_fmt(struct isp_device *isp, struct isp_format *f)
{
	int ret = 0;

	if (!isp->bypass && (isp->fmt_data.mipi_clk != f->fmt_data->mipi_clk)) {
		ISP_PRINT(ISP_INFO,"isp pre fmt restart phy!!!\n");
		ISP_PRINT(ISP_INFO,"f->fmt_data->mipi_clk = %d\n",f->fmt_data->mipi_clk);
		ret = csi_phy_start(0, f->fmt_data->mipi_clk);
		if (!ret)
			isp->fmt_data.mipi_clk = f->fmt_data->mipi_clk;
	}
	return ret;
}

static int isp_s_fmt(struct isp_device *isp, struct isp_format *f)
{
	struct isp_parm *iparm = &isp->parm;
	int in_width, in_height;

	if (isp->bypass) {
		in_width = f->width;
		in_height = f->height;
	} else {
#if 1
		in_width = f->dev_width;
		in_height = f->dev_height;
#else
		/*pzqi modify here*/
		in_width	= f->width;
		in_height	= f->height;
#endif
	}

	/* check if need set format to isp. */
	isp->format_active = 0;
	if ((isp->fmt_data.vts != f->fmt_data->vts)
			|| (isp->fmt_data.hts != f->fmt_data->hts))
		isp->format_active = 1;

	if ((iparm->input.width != in_width)
			|| (iparm->input.height != in_height)
			|| (iparm->output[0].width != f->width)
			|| (iparm->output[0].height != f->height)
			|| (iparm->output[0].format != f->fourcc)
			|| (iparm->input.format != f->code))
		isp->format_active = 1;

	/* Save the parameters. */
	if(f->fourcc == V4L2_PIX_FMT_NV12YUV422){
		iparm->out_videos = 2;
		iparm->output[1].width = 640;
		iparm->output[1].height = 360;
		iparm->output[1].format = V4L2_PIX_FMT_YUYV;
		iparm->output[1].imagesize = 640*360*2;
	}else{
		iparm->out_videos = 1;
	}
	iparm->output[0].width = f->width;
	iparm->output[0].height = f->height;
	iparm->output[0].format = f->fourcc;
	iparm->output[0].imagesize = (f->width * f->height * f->depth) >> 3;
	iparm->input.width = in_width;
	iparm->input.height = in_height;
	iparm->input.format = f->code;
	iparm->crop_width = iparm->input.width;
	iparm->crop_height = iparm->input.height;
	iparm->crop_x = 0;
	iparm->crop_y = 0;
	memcpy(&isp->fmt_data, f->fmt_data, sizeof(isp->fmt_data));
	ISP_PRINT(ISP_INFO,"[%s]isp->fmt_data.vts = %x;isp->fmt_data.mipi_clk = %d\n",__func__,isp->fmt_data.vts,isp->fmt_data.mipi_clk);
	return 0;
}

static int isp_s_ctrl(struct isp_device *isp, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct isp_parm *iparm = &isp->parm;

	switch (ctrl->id) {
	case V4L2_CID_AUTO_WHITE_BALANCE:
		ISP_PRINT(ISP_INFO,"set white_balance %d\n", ctrl->value);
		ret = isp_set_auto_white_balance(isp, ctrl->value);
		if (!ret){
			iparm->white_balance = 0;
			iparm->auto_white_balance = ctrl->value;
		}
		break;
	case V4L2_CID_DO_WHITE_BALANCE:
		ISP_PRINT(ISP_INFO,"set white_balance %d\n", ctrl->value);
		ret = isp_set_do_white_balance(isp, ctrl->value);
		if (!ret){
			iparm->white_balance = ctrl->value;
			iparm->auto_white_balance = 0;
		}
		break;
	case V4L2_CID_BRIGHTNESS:
		ISP_PRINT(ISP_INFO,"set brightness %d\n", ctrl->value);
		ret = isp_set_brightness(isp, ctrl->value);
		if (!ret)
			iparm->brightness = ctrl->value;
		break;
	case V4L2_CID_EXPOSURE_AUTO:
		ISP_PRINT(ISP_INFO,"set exposure manual %d\n", ctrl->value);
		if (ctrl->value == V4L2_EXPOSURE_MANUAL){
			ret = isp_set_exposure_manual(isp);
			if (!ret)
				iparm->auto_exposure = 1;
		}
		break;
	case V4L2_CID_EXPOSURE:
		ISP_PRINT(ISP_INFO,"set exposure %d\n", ctrl->value);
		ret = isp_set_exposure(isp, ctrl->value);
		if (!ret){
			iparm->exposure = ctrl->value;
			iparm->auto_exposure = 0;
		}
		break;
	case V4L2_CID_GAIN:
		ISP_PRINT(ISP_INFO,"set exposure gain%d\n", ctrl->value);
		ret = isp_set_gain(isp, ctrl->value);
		if (!ret){
			iparm->gain = ctrl->value;
		}
		break;
	case V4L2_CID_CONTRAST:
		ISP_PRINT(ISP_INFO,"set contrast %d\n", ctrl->value);
		ret = isp_set_contrast(isp, ctrl->value);
		if (!ret)
			iparm->contrast = ctrl->value;
		break;
	case V4L2_CID_SATURATION:
		ISP_PRINT(ISP_INFO,"set saturation %d\n", ctrl->value);
		ret = isp_set_saturation(isp, ctrl->value);
		if (!ret)
			iparm->saturation = ctrl->value;
		break;
	case V4L2_CID_SHARPNESS:
		ISP_PRINT(ISP_INFO,"set sharpness %d\n", ctrl->value);
		ret = isp_set_sharpness(isp, ctrl->value);
		if (!ret)
			iparm->sharpness = ctrl->value;
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY:
		ISP_PRINT(ISP_INFO,"set flicker %d\n", ctrl->value);
		ret = isp_set_flicker(isp, ctrl->value);
		if (!ret)
			iparm->flicker = ctrl->value;
		break;
	case V4L2_CID_FOCUS_AUTO:
		break;
	case V4L2_CID_FOCUS_RELATIVE:
		break;
	case V4L2_CID_FOCUS_ABSOLUTE:
		break;
	case V4L2_CID_ZOOM_RELATIVE:
		ISP_PRINT(ISP_INFO,"set zoom %d\n", ctrl->value);
		if (isp->running)
			ret = isp_set_zoom(isp, ctrl->value);
		if (!ret)
			iparm->zoom = ctrl->value;
		break;
	case V4L2_CID_HFLIP:
		ISP_PRINT(ISP_INFO,"set hflip %d\n", ctrl->value);
		ret = isp_set_hflip(isp, ctrl->value);
		if (!ret)
			iparm->hflip = ctrl->value;
		break;
	case V4L2_CID_VFLIP:
		ISP_PRINT(ISP_INFO,"set vflip %d\n", ctrl->value);
		ret = isp_set_vflip(isp, ctrl->value);
		if (!ret)
			iparm->vflip = ctrl->value;
		break;
		/* Private. */
	case V4L2_CID_ISO:
		ISP_PRINT(ISP_INFO,"set iso %d\n", ctrl->value);
		ret = isp_set_iso(isp, ctrl->value);
		if (!ret)
			iparm->iso = ctrl->value;
		break;
	case V4L2_CID_EFFECT:
		ISP_PRINT(ISP_INFO,"set effect %d\n", ctrl->value);
		ret = isp_set_effect(isp, ctrl->value);
		if (!ret)
			iparm->effects = ctrl->value;
		break;
	case V4L2_CID_FLASH_MODE:
		break;
	case V4L2_CID_SCENE:
		ISP_PRINT(ISP_INFO,"set scene %d\n", ctrl->value);
		ret = isp_set_scene(isp, ctrl->value);
		if (!ret)
			iparm->scene_mode = ctrl->value;
		break;
	case V4L2_CID_FRAME_RATE:
		iparm->frame_rate = ctrl->value;
		break;
	case V4L2_CID_RED_BALANCE:
		ISP_PRINT(ISP_INFO,"set scene %d\n", ctrl->value);
		ret = isp_set_red_balance(isp, ctrl->value);
		break;
	case V4L2_CID_BLUE_BALANCE:
		ISP_PRINT(ISP_INFO,"set scene %d\n", ctrl->value);
		ret = isp_set_blue_balance(isp, ctrl->value);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int isp_g_ctrl(struct isp_device *isp, struct v4l2_control *ctrl)
{
	int ret = 0;
	struct isp_parm *iparm = &isp->parm;

	switch (ctrl->id) {
	case V4L2_CID_DO_WHITE_BALANCE:
		ctrl->value = iparm->white_balance;
		break;
	case V4L2_CID_BRIGHTNESS:
		ctrl->value = iparm->brightness;
		break;
	case V4L2_CID_EXPOSURE:
		ctrl->value = iparm->exposure;
		break;
	case V4L2_CID_CONTRAST:
		ctrl->value = iparm->contrast;
		break;
	case V4L2_CID_SATURATION:
		ctrl->value = iparm->saturation;
		break;
	case V4L2_CID_SHARPNESS:
		ctrl->value = iparm->sharpness;
		break;
	case V4L2_CID_POWER_LINE_FREQUENCY:
		ctrl->value = iparm->flicker;
		break;
	case V4L2_CID_FOCUS_AUTO:
		break;
	case V4L2_CID_FOCUS_RELATIVE:
		break;
	case V4L2_CID_FOCUS_ABSOLUTE:
		break;
	case V4L2_CID_ZOOM_RELATIVE:
		ctrl->value = iparm->zoom;
		break;
	case V4L2_CID_HFLIP:
		ctrl->value = iparm->hflip;
		break;
	case V4L2_CID_VFLIP:
		ctrl->value = iparm->vflip;
		break;
		/* Private. */
	case V4L2_CID_ISO:
		ctrl->value = iparm->iso;
		break;
	case V4L2_CID_EFFECT:
		ctrl->value = iparm->effects;
		break;
	case V4L2_CID_FLASH_MODE:
		break;
	case V4L2_CID_SCENE:
		ctrl->value = iparm->scene_mode;
		break;
	case V4L2_CID_FRAME_RATE:
		ctrl->value = iparm->frame_rate;
		break;
	case V4L2_CID_RED_BALANCE:
		ret = isp_get_red_balance(isp, &(ctrl->value));
		break;
	case V4L2_CID_BLUE_BALANCE:
		ret = isp_get_blue_balance(isp, &(ctrl->value));
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int isp_s_parm(struct isp_device *isp, struct v4l2_streamparm *parm)
{
	return 0;
}

static int isp_g_parm(struct isp_device *isp, struct v4l2_streamparm *parm)
{
	return 0;
}

static int isp_s_tlb_base(struct isp_device *isp, unsigned int *tlb_base)
{

	int tlb_en = 1;
	int mipi_sel = 1;
	int tlb_invld = 1;
	int tlb_gcn = DMMU_PTE_CHECK_PAGE_VALID;
	int tlb_cnm = DMMU_PTE_CHECK_PAGE_VALID;
	int tlb_ridx = 0;  /*6 bits TLB entry read-index*/
	unsigned int _tlb_base  = *tlb_base;

	ISP_PRINT(ISP_INFO," Read ISP TLB CTRL : 0x%x\n", isp_reg_readl(isp, 0xF0004));
	ISP_PRINT(ISP_INFO,"Read ISP TLB BASE : 0x%x\n", isp_reg_readl(isp, 0xF0008));

	if(tlb_invld)
		isp_reg_writel(isp, 0x00000010, 0xF0000);/*TLB Reset*/
	/*TLB Control*/
	isp_reg_writel(isp,( tlb_en |
				(mipi_sel << 1) |
				(tlb_gcn << 2) |
				(tlb_cnm << 14) |
				(tlb_ridx << 26)
				), 0xF0004);

	/*TLB Table Base Address*/
	isp_reg_writel(isp, _tlb_base, 0xF0008);
	dump_tlb_regs(isp);

	return 0;
}
static int isp_tlb_map_one_vaddr(struct isp_device *isp, unsigned int vaddr, unsigned int size)
{
	int ret = 0;
	struct isp_tlb_pidmanager *p = NULL;
	struct isp_tlb_pidmanager *tmp = NULL;
	struct isp_tlb_vaddrmanager *v = NULL;
	struct isp_tlb_vaddrmanager *prev = NULL;
	struct isp_tlb_vaddrmanager *after = NULL;
	struct isp_tlb_vaddrmanager *tmv = NULL;
	pid_t pid = current->tgid;

	if(list_empty(&(isp->tlb_list))){
		ISP_PRINT(ISP_ERROR,"%s[%d] vaddr can't map because tlb isn't inited!\n",__func__,__LINE__);
		ret = -EINVAL;
		goto out;
	}
	list_for_each_entry(tmp, &(isp->tlb_list), pid_entry){
		if(tmp->pid == pid){
			p = tmp;
			break;
		}
	}
	if(!p){
		ret = -EINVAL;
		goto out;
	}

	list_for_each_entry(tmv, &(p->vaddr_list), vaddr_entry){
		if(tmv->vaddr <= vaddr && vaddr < tmv->vaddr + tmv->size){
			v = tmv;
			break;
		}else if(vaddr < tmv->vaddr){
			after = tmv;
			break;
		}else
			prev = tmv;
	}
	/* after != NULL or prev != NULL */
	if(!v){
		if(prev && (prev->vaddr + prev->size == vaddr)){
			/* prev and current are sequential */
			ISP_PRINT(ISP_INFO,"^^^%s[%d] *prev-current* vaddr = 0x%08x\n",__func__,__LINE__,vaddr);
			prev->size += size;
			v = prev;
		}
		if(after && (vaddr + size == after->vaddr)){
			if(!v){
				ISP_PRINT(ISP_INFO,"^^^%s[%d] *current-after* vaddr = 0x%08x\n",__func__,__LINE__,vaddr);
				/* after and current are sequential */
				after->vaddr = vaddr;
				after->size += size;
				v = after;
			}else{
				ISP_PRINT(ISP_INFO,"^^^%s[%d] *prev-current-after* vaddr = 0x%08x\n",__func__,__LINE__,vaddr);
				/* prev, after and current are sequential */
				prev->size += after->size;
				list_del(&(after->vaddr_entry));
				kfree(after);
			}
		}
		if(!v){
			/* prev ,after and current are discontinuous */
			ISP_PRINT(ISP_INFO,"^^^%s[%d] alloc_vaddrmanager! vaddr = 0x%08x\n",__func__,__LINE__,vaddr);
			v = kzalloc(sizeof(*v), GFP_KERNEL);
			if(!v){
				ISP_PRINT(ISP_ERROR,"%s[%d]\n",__func__,__LINE__);
				ret = -EINVAL;
				goto alloc_fail;
			}
			v->vaddr = vaddr;
			v->size = size;
			if(prev){
				ISP_PRINT(ISP_INFO,"^^^%s[%d] *instand prev* vaddr = 0x%08x\n",__func__,__LINE__,vaddr);
				prev->vaddr_entry.next->prev = &(v->vaddr_entry);
				v->vaddr_entry.prev = &(prev->vaddr_entry);
				v->vaddr_entry.next = prev->vaddr_entry.next;
				prev->vaddr_entry.next = &(v->vaddr_entry);
			}else if(after){
				ISP_PRINT(ISP_INFO,"^^^%s[%d] *instand after* vaddr = 0x%08x\n",__func__,__LINE__,vaddr);
				after->vaddr_entry.prev->next = &(v->vaddr_entry);
				v->vaddr_entry.next = &(after->vaddr_entry);
				v->vaddr_entry.prev = after->vaddr_entry.prev;
				after->vaddr_entry.prev = &(v->vaddr_entry);
			}else
				list_add_tail(&(v->vaddr_entry), &(p->vaddr_list));
		}
		ret = dmmu_match_user_mem_tlb((void*)vaddr, size);
		if(ret < 0)
			goto match_fail;
		ret = dmmu_map_user_mem((void *)vaddr, size);
		if(ret < 0)
			goto map_fail;
	}
	return 0;
map_fail:
match_fail:
	kfree(v);
alloc_fail:
out:
	return ret;
}
static int isp_tlb_unmap_all_vaddr(struct isp_device *isp)
{
	struct isp_tlb_pidmanager *p = NULL;
	struct isp_tlb_pidmanager *tmp = NULL;
	struct isp_tlb_vaddrmanager *v = NULL;
	struct list_head *pos = NULL;
	pid_t pid = current->tgid;

	if(!list_empty(&(isp->tlb_list))){
		list_for_each_entry(tmp, &(isp->tlb_list), pid_entry){
			if(tmp->pid == pid){
				p = tmp;
				break;
			}
		}
		/* if find, release it's vaddr_list */
		if(p){
			ISP_PRINT(ISP_INFO,"^^^%s[%d] unmap!\n",__func__,__LINE__);
			/* if vaddr_list  of the tlb_pidmanager isn't empty, release it */
			if(!list_empty(&(p->vaddr_list))){
				list_for_each(pos, &(p->vaddr_list)){
					v = list_entry(pos, struct isp_tlb_vaddrmanager,vaddr_entry);
					list_del(&(v->vaddr_entry));
					dmmu_unmap_user_mem((void *)(v->vaddr), v->size);
					kfree(v);
					pos = &(p->vaddr_list);
				}
			}
		}
	}
	return 0;
}
static int isp_tlb_init(struct isp_device *isp)
{
	int ret = 0;
	struct isp_tlb_pidmanager *p = NULL;
	struct isp_tlb_pidmanager *tmp = NULL;
	pid_t pid = current->tgid;

	/* first */
	if(list_empty(&(isp->tlb_list))){
		ISP_PRINT(ISP_INFO,"^^^%s[%d] mmu_init! \n",__func__,__LINE__);
		ret = dmmu_init();
		if(ret < 0)
			goto dmmu_fail;
		isp->tlb_flag = 1;
	}

	/* if tlb_list isn't empty , check pid */
	if(!list_empty(&(isp->tlb_list))){
		list_for_each_entry(tmp, &(isp->tlb_list), pid_entry){
			if(tmp->pid == pid){
				p = tmp;
				break;
			}
		}
	}

	/* if current->tgid isn't in tlb_list, add a isp_tlb_pidmanager */
	if(!p){
		p = kzalloc(sizeof(*p), GFP_KERNEL);
		if(!p){
			ISP_PRINT(ISP_ERROR,"%s[%d]\n",__func__,__LINE__);
			ret = -EINVAL;
			goto alloc_fail;
		}
		p->pid = pid;
		INIT_LIST_HEAD(&(p->vaddr_list));
		ret = dmmu_get_page_table_base_phys(&(p->tlbbase));
		ISP_PRINT(ISP_INFO,"^^^%s[%d] alloc pidmanager! tlbbase = 0x%08x\n",__func__,__LINE__,p->tlbbase);
		isp_s_tlb_base(isp,&(p->tlbbase));
		if(ret < 0)
			goto tlbbase_fail;
		list_add_tail(&(p->pid_entry), &(isp->tlb_list));
	}
	return 0;
tlbbase_fail:
	kfree(p);
alloc_fail:
dmmu_fail:
	return ret;
}
static int isp_tlb_deinit(struct isp_device *isp)
{
	int ret = 0;
	struct isp_tlb_pidmanager *p = NULL;
	struct isp_tlb_pidmanager *tmp = NULL;
	struct isp_tlb_vaddrmanager *vaddr = NULL;
	struct list_head *pos = NULL;
	pid_t pid = current->tgid;

	if(isp->tlb_flag == 0)
		return 0;
	/* if tlb_list isn't empty , check pid */
	if(!list_empty(&(isp->tlb_list))){
		list_for_each_entry(tmp, &(isp->tlb_list), pid_entry){
			if(tmp->pid == pid){
				p = tmp;
				break;
			}
		}
	}

	/* if current->tgid is find, release it  */
	if(p){
		list_del(&(p->pid_entry));

		/* if vaddr_list  of the tlb_pidmanager, release it */
		if(!list_empty(&(p->vaddr_list))){
			list_for_each(pos, &(p->vaddr_list)){
				vaddr = list_entry(pos, struct isp_tlb_vaddrmanager,vaddr_entry);
				list_del(&(vaddr->vaddr_entry));
				kfree(vaddr);
				pos = &(p->vaddr_list);
			}
		}
		kfree(p);
		ISP_PRINT(ISP_INFO,"%s[%d] kfree pidmanager! \n",__func__,__LINE__);
	}
	/* last */
	if(list_empty(&(isp->tlb_list))){
		ISP_PRINT(ISP_INFO,"%s[%d] mmu_deinit! \n",__func__,__LINE__);
		ret = dmmu_deinit();
		isp->tlb_flag = 0;
	}
	return ret;
}
static struct isp_ops isp_ops = {

	.init = isp_init,
	.release = isp_release,
	.open = isp_open,
	.close = isp_close,
	.config = isp_config,
	.suspend = isp_suspend,
	.resume = isp_resume,
	.mclk_on = isp_mclk_on,
	.mclk_off = isp_mclk_off,
	.offline_process = isp_offline_process,
	.start_capture = isp_start_capture,
	.stop_capture = isp_stop_capture,
	.enable_capture = isp_enable_capture,
	.disable_capture = isp_disable_capture,
	.update_buffer = isp_update_buffer,
	.check_fmt = isp_check_fmt,
	.try_fmt = isp_try_fmt,
	.pre_fmt = isp_pre_fmt,
	.s_fmt = isp_s_fmt,
	.s_ctrl = isp_s_ctrl,
	.g_ctrl = isp_g_ctrl,
	.s_parm = isp_s_parm,
	.g_parm = isp_g_parm,
	.tlb_init = isp_tlb_init,
	.tlb_deinit = isp_tlb_deinit,
	.tlb_map_one_vaddr = isp_tlb_map_one_vaddr,
	.tlb_unmap_all_vaddr = isp_tlb_unmap_all_vaddr,
};

int isp_device_init(struct isp_device* isp)
{
	struct resource *res = isp->res;
	int ret = 0;

	spin_lock_init(&isp->lock);
	init_completion(&isp->completion);

	ISP_PRINT(ISP_INFO,"enter device init %d\n",ret);
	isp->base = ioremap(res->start, res->end - res->start + 1);
	if (!isp->base) {
		ISP_PRINT(ISP_ERROR, KERN_ERR "Unable to ioremap registers.n");
		ret = -ENXIO;
		goto exit;
	}

	isp->boot = 0;
	ret = isp_int_init(isp);
	if (ret)
		goto io_unmap;

	ret = isp_i2c_init(isp);
	if (ret)
		goto irq_free;

	ret = isp_clk_init(isp);
	if (ret)
		goto i2c_release;

	csi_phy_init();

	isp->ops = &isp_ops;

	INIT_LIST_HEAD(&(isp->tlb_list)); // add by xhshen
	isp->tlb_flag = 0;
	return 0;

i2c_release:
	isp_i2c_release(isp);
irq_free:
	free_irq(isp->irq, isp);
io_unmap:
	iounmap(isp->base);
exit:
	return ret;
}
EXPORT_SYMBOL(isp_device_init);

int isp_device_release(struct isp_device* isp)
{
	if (!isp)
		return -EINVAL;

	csi_phy_release();
	isp_clk_release(isp);
	isp_i2c_release(isp);
	free_irq(isp->irq, isp);
	iounmap(isp->base);

	return 0;
}
EXPORT_SYMBOL(isp_device_release);

