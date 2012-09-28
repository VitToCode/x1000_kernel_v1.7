/* kernel/drivers/video/jz4780/input/jz4780_ipu.c
 *
 * Copyright (c) 2012 Ingenic Semiconductor Co., Ltd.
 *              http://www.ingenic.com/
 *
 * Input file for Ingenic IPU driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

//#define DEBUG
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/fs.h>
#include <linux/irq.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/ctype.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/memory.h>
#include <linux/suspend.h>
#include <linux/miscdevice.h>
#include <linux/proc_fs.h>

#include "regs_ipu.h"
#include "jz4780_ipu.h"

#ifdef PHYS
#undef PHYS
#endif

#define PHYS(x) (x)

static int bypass;
static int ipu_id;
static int ipu0_direct;
static int ipu1_direct;

struct ipu_reg_struct jz47_ipu_regs_name[] = {
	{"IPU_FM_CTRL",	        IPU_FM_CTRL},
	{"IPU_STATUS",	        IPU_STATUS},
	{"IPU_D_FMT",	        IPU_D_FMT},
	{"IPU_Y_ADDR",	        IPU_Y_ADDR},
	{"IPU_U_ADDR",	        IPU_U_ADDR},
	{"IPU_V_ADDR",	        IPU_V_ADDR},
	{"IPU_IN_FM_GS",        IPU_IN_FM_GS},
	{"IPU_Y_STRIDE",        IPU_Y_STRIDE},
	{"IPU_UV_STRIDE",       IPU_UV_STRIDE},
	{"IPU_OUT_ADDR",        IPU_OUT_ADDR},
	{"IPU_OUT_GS",	        IPU_OUT_GS},
	{"IPU_OUT_STRIDE",      IPU_OUT_STRIDE},
	{"RSZ_COEF_INDEX",      IPU_RSZ_COEF_INDEX},
	{"IPU_CSC_C0_COEF",     IPU_CSC_C0_COEF},
	{"IPU_CSC_C1_COEF",     IPU_CSC_C1_COEF},
	{"IPU_CSC_C2_COEF",     IPU_CSC_C2_COEF},
	{"IPU_CSC_C3_COEF",     IPU_CSC_C3_COEF},
	{"HRSZ_LUT_BASE",       HRSZ_LUT_BASE}, /* write only */
	{"VRSZ_LUT_BASE",       VRSZ_LUT_BASE}, /* write only */
	{"IPU_CSC_OFFSET_PARA", IPU_CSC_OFFSET_PARA},
	{"IPU_SRC_TLB_ADDR",    IPU_SRC_TLB_ADDR},
	{"IPU_DEST_TLB_ADDR",   IPU_DEST_TLB_ADDR},
	{"IPU_ADDR_CTRL",       IPU_ADDR_CTRL},
	{"IPU_Y_ADDR_N",        IPU_Y_ADDR_N},
	{"IPU_U_ADDR_N",        IPU_U_ADDR_N},
	{"IPU_V_ADDR_N",        IPU_V_ADDR_N},
	{"IPU_OUT_ADDR_N",      IPU_OUT_ADDR_N},
	{"IPU_SRC_TLB_ADDR_N",  IPU_SRC_TLB_ADDR_N},
	{"IPU_DEST_TLB_ADDR_N", IPU_DEST_TLB_ADDR_N},
	{"IPU_REG_EN_MSK",      IPU_REG_EN_MSK},
	{"IPU_TRIG",            IPU_TRIG},
	{"IPU_FM_XYOFT",        IPU_FM_XYOFT},
	{"IPU_GLB_CTRL",        IPU_GLB_CTRL},
	{"IPU_OSD_CTRL",        IPU_OSD_CTRL},
};

/* max timeout 100ms */
static inline int jz47_ipu_wait_frame_end_flag(struct jz_ipu *ipu)
{
	unsigned long long clock_start;
	unsigned long long clock_now;
	unsigned int ipu_frame_end;

	clock_start = sched_clock();
	while ( 1 ) {
		ipu_frame_end = (reg_read(ipu, IPU_STATUS) & OUT_END);
		if (ipu_frame_end) 
			break;	
		clock_now = sched_clock();
		if ((clock_now - clock_start) > (30 * 1000000)) { /* timeout: 30ms */
			dev_err(ipu->dev, "jz47_ipu_wait_frame_end_flag() timeout....\n");
			return -1;	/* wait the end flag */
		}
	}

	return 0;
}

static void enable_ipu(struct jz_ipu *ipu)
{
	unsigned int tmp;
	
	tmp = reg_read(ipu, IPU_FM_CTRL);
	tmp |= IPU_EN;
	reg_write(ipu, IPU_FM_CTRL, tmp);
	reg_write(ipu, IPU_STATUS, 0);	
}

static void reset_ipu(struct jz_ipu *ipu)
{
	unsigned int tmp;

	tmp = reg_read(ipu, IPU_TRIG);
	tmp |= IPU_RESET;
	reg_write(ipu, IPU_TRIG, tmp);
	tmp &= ~IPU_RESET;
	reg_write(ipu, IPU_TRIG, tmp);
}

static void enable_csc_mode(struct jz_ipu *ipu)
{
	unsigned int tmp;

	tmp = reg_read(ipu, IPU_FM_CTRL);
	tmp &= ~CSC_EN;
	reg_write(ipu, IPU_FM_CTRL, tmp); 
}

static void enable_lcdc_mode(struct jz_ipu *ipu)
{
	unsigned int tmp;

	tmp = reg_read(ipu, IPU_FM_CTRL);			
	tmp |= LCDC_SEL;
	reg_write(ipu, IPU_FM_CTRL, tmp);
}

static void enable_pkg_mode(struct jz_ipu *ipu)
{
	unsigned int tmp;

	tmp = reg_read(ipu, IPU_FM_CTRL);
	tmp |= SPKG_SEL;
	reg_write(ipu, IPU_FM_CTRL, tmp);
}

static void disable_pkg_mode(struct jz_ipu *ipu)
{
	unsigned int tmp;

	tmp = reg_read(ipu, IPU_FM_CTRL);
	tmp &= ~SPKG_SEL;
	reg_write(ipu, IPU_FM_CTRL, tmp);
}

static void enable_blk_mode(struct jz_ipu *ipu)
{
	unsigned int tmp;

	tmp = reg_read(ipu, IPU_D_FMT);
	tmp |= 1 << 4; //BLK_SEL
	reg_write(ipu, IPU_D_FMT, tmp); 
}

static void clear_hvsz_vrsz_bits(struct jz_ipu *ipu)
{
	unsigned int tmp;

	tmp = reg_read(ipu, IPU_FM_CTRL);
	tmp &= ~(VRSZ_EN | HRSZ_EN);
	reg_write(ipu, IPU_FM_CTRL, tmp);
}

static void enable_hrsz(struct jz_ipu *ipu)
{
	unsigned int tmp;

	tmp = reg_read(ipu, IPU_FM_CTRL);
	tmp |= HRSZ_EN;
	reg_write(ipu, IPU_FM_CTRL, tmp);
}

static void enable_vrsz(struct jz_ipu *ipu)
{
	unsigned int tmp;

	tmp = reg_read(ipu, IPU_FM_CTRL);
	tmp |= VRSZ_EN;
	reg_write(ipu, IPU_FM_CTRL, tmp);
}

static void sel_zoom_mode(struct jz_ipu *ipu)
{
	unsigned int tmp;

	tmp = reg_read(ipu, IPU_FM_CTRL);
	tmp |= ZOOM_SEL;
	reg_write(ipu, IPU_FM_CTRL, tmp);
}

static void disable_zoom_mode(struct jz_ipu *ipu)
{
	unsigned int tmp;

	tmp = reg_read(ipu, IPU_FM_CTRL);
	tmp &= ~ZOOM_SEL;
	reg_write(ipu, IPU_FM_CTRL, tmp);
}

static void set_hrsz_lut_weigth_cube(struct jz_ipu *ipu)
{
	int i;
	unsigned int tmp;
	int *oft_table;
	struct ipu_table *table = &ipu->table;

	oft_table = &table->hoft[1];
	tmp = 1 << START_N_SFT;
	reg_write(ipu, HRSZ_LUT_BASE, tmp);
	for (i=0;i<ipu->img.hcoef_real_heiht;i++) {
		tmp = ((table->cube_hcoef[i][0] & W_COEF_20_MSK)<<W_COEF_20_SFT) | 
			((table->cube_hcoef[i][1] & W_COEF_31_MSK)<<W_COEF_31_SFT);
		reg_write(ipu, HRSZ_LUT_BASE, tmp); 
		tmp = ((table->cube_hcoef[i][2] & W_COEF_20_MSK)<<W_COEF_20_SFT) |
			(((table->cube_hcoef[i][3]) & W_COEF_31_MSK)<<W_COEF_31_SFT) | 
			((oft_table[i] & HRSZ_OFT_MSK) << HRSZ_OFT_SFT);
		reg_write(ipu, HRSZ_LUT_BASE, tmp);
	}
}

static void set_vrsz_lut_weigth_cube(struct jz_ipu *ipu)
{
	int i;
	unsigned int tmp;
	int *oft_table;
	struct ipu_table *table = &ipu->table;

	oft_table = &table->voft[1];
	tmp = 1 << START_N_SFT;
	reg_write(ipu, VRSZ_LUT_BASE, tmp);
	for (i=0;i<ipu->img.vcoef_real_heiht;i++) {
		tmp = ((table->cube_vcoef[i][0] & W_COEF_20_MSK)<<W_COEF_20_SFT) | 
			((table->cube_vcoef[i][1] & W_COEF_31_MSK)<<W_COEF_31_SFT);
		reg_write(ipu, VRSZ_LUT_BASE, tmp); 
		tmp = ((table->cube_vcoef[i][2] & W_COEF_20_MSK)<<W_COEF_20_SFT) | 
			((table->cube_vcoef[i][3] & W_COEF_31_MSK)<<W_COEF_31_SFT) | 
			((oft_table[i] & VRSZ_OFT_MSK) << VRSZ_OFT_SFT);
		reg_write(ipu, VRSZ_LUT_BASE, tmp); 
	}
}

static void set_vrsz_lut_coef_line(struct jz_ipu *ipu)
{
	int i;
	unsigned int tmp;
	int *oft_table, *coef_table;
	struct ipu_table *table = &ipu->table;
	struct ipu_img_param *img = &ipu->img;

	oft_table = &table->voft[1];
	coef_table = &table->vcoef[1];

	tmp = 1 << START_N_SFT;
	reg_write(ipu, VRSZ_LUT_BASE, tmp);
	for (i = 0; i < img->vcoef_real_heiht; i++) {
		tmp = ((coef_table[i]&W_COEF0_MSK)<< W_COEF0_SFT) |
			((oft_table[i]&V_OFT_MSK) << V_CONF_SFT);
		reg_write(ipu, VRSZ_LUT_BASE, tmp); 
	}
}

static void set_hrsz_lut_coef_line(struct jz_ipu *ipu)
{
	int i;
	unsigned int tmp;
	int *oft_table, *coef_table;
	struct ipu_table *table = &ipu->table;
	struct ipu_img_param *img = &ipu->img;

	oft_table = &table->hoft[1];
	coef_table = &table->hcoef[1];
	
	tmp = (1 << START_N_SFT); 
	reg_write(ipu, HRSZ_LUT_BASE, tmp);
	for (i = 0; i < img->hcoef_real_heiht; i++) {
		tmp = ((coef_table[i]&W_COEF0_MSK)<< W_COEF0_SFT) | 
			((oft_table[i]&H_OFT_MSK) << H_CONF_SFT);
		reg_write(ipu, HRSZ_LUT_BASE, tmp); 
	}
}

static void set_gs_regs(struct jz_ipu *ipu,int Wdiff,int Hdiff,int outW,int outH)
{
	unsigned int tmp;
	unsigned int tmp1;

	//	printk("ipu->img.in_width = %d,  Wdiff = %d\n", ipu->img.in_width, Wdiff);
	//	printk("outW = %d, outH = %d\n",  outW, outH);
	tmp1 =ipu->img.in_width - Wdiff;
	//	tmp1 >>= 4;	
	tmp = IN_FM_W(tmp1) | IN_FM_H((ipu->img.in_height - Hdiff) & ~0x1);
	reg_write(ipu, IPU_IN_FM_GS, tmp);
	tmp = OUT_FM_W(outW) | OUT_FM_H(outH);
	reg_write(ipu, IPU_OUT_GS, tmp);
}

static void set_csc_param(struct jz_ipu *ipu, unsigned int in_fmt, unsigned int out_fmt)
{
	unsigned int tmp;

	if ((in_fmt != IN_FMT_YUV444) && (out_fmt != OUT_FMT_YUV422)) {
		tmp = reg_read(ipu, IPU_FM_CTRL);
		tmp |= CSC_EN; 
		reg_write(ipu, IPU_FM_CTRL, tmp);
		reg_write(ipu, IPU_CSC_C0_COEF, YUV_CSC_C0);
		if (in_fmt == IN_FMT_YUV420_B) {
			// interchange C1 with C4, C2 with C3 for IPU Block format
			reg_write(ipu, IPU_CSC_C1_COEF, YUV_CSC_C4);
			reg_write(ipu, IPU_CSC_C2_COEF, YUV_CSC_C3);
			reg_write(ipu, IPU_CSC_C3_COEF, YUV_CSC_C2);
			reg_write(ipu, IPU_CSC_C4_COEF, YUV_CSC_C1);
		} else {
			reg_write(ipu, IPU_CSC_C1_COEF, YUV_CSC_C1);
			reg_write(ipu, IPU_CSC_C2_COEF, YUV_CSC_C2);
			reg_write(ipu, IPU_CSC_C3_COEF, YUV_CSC_C3);
			reg_write(ipu, IPU_CSC_C4_COEF, YUV_CSC_C4);
		}
		reg_write(ipu, IPU_CSC_OFFSET_PARA, YUV_CSC_OFFSET_PARA);
	} else {
		tmp = reg_read(ipu, IPU_FM_CTRL);
		tmp &= ~CSC_EN;
		reg_write(ipu, IPU_FM_CTRL, tmp); 
		reg_write(ipu, IPU_CSC_OFFSET_PARA, 0x0);
	}
}

static void clear_ipu_out_end(struct jz_ipu *ipu)
{
	unsigned int tmp;

	tmp = reg_read(ipu, IPU_STATUS);
	tmp &= ~OUT_END;
	reg_write(ipu, IPU_STATUS, tmp);
}

static void enable_ctrl_regs(struct jz_ipu *ipu)
{
	unsigned int tmp = 0;

	tmp = 0xffffffff;
	reg_write(ipu, IPU_ADDR_CTRL, tmp);
	tmp = reg_read(ipu, IPU_ADDR_CTRL);
}

static void ipu_enable_irq(struct jz_ipu *ipu)
{
	unsigned int tmp;

	tmp = reg_read(ipu, IPU_GLB_CTRL);
	tmp |= (IRQ_EN | DMA_OPT_ENA);
	reg_write(ipu, IPU_GLB_CTRL, tmp);

	tmp = reg_read(ipu, IPU_FM_CTRL);
	tmp |= FM_IRQ_EN;
	reg_write(ipu, IPU_FM_CTRL, tmp);
}

static void ipu_disable_irq(struct jz_ipu *ipu)
{
	unsigned int tmp;

	tmp = reg_read(ipu, IPU_FM_CTRL);
	tmp &= ~FM_IRQ_EN;
	reg_write(ipu, IPU_FM_CTRL, tmp);

	tmp = reg_read(ipu, IPU_GLB_CTRL);
	tmp &= ~(IRQ_EN | DMA_OPT_ENA);
	reg_write(ipu, IPU_GLB_CTRL, tmp);

}

static void start_ipu(struct jz_ipu *ipu)
{
	unsigned int tmp;

	tmp = reg_read(ipu, IPU_TRIG);
	tmp |= IPU_RUN;
	reg_write(ipu, IPU_TRIG, tmp);
}

static void enable_spage_map(struct jz_ipu *ipu)
{
	unsigned int tmp;

	tmp = reg_read(ipu, IPU_FM_CTRL);
	tmp |= SPAGE_MAP;
	reg_write(ipu, IPU_FM_CTRL, tmp);
}

static void disable_spage_map(struct jz_ipu *ipu)
{
	unsigned int tmp;

	tmp = reg_read(ipu, IPU_FM_CTRL);
	tmp &= ~SPAGE_MAP;
	reg_write(ipu, IPU_FM_CTRL, tmp);
}

static void disable_dpage_map(struct jz_ipu *ipu)
{
	unsigned int tmp;

	tmp = reg_read(ipu, IPU_FM_CTRL);
	tmp &= ~DPAGE_MAP;
	reg_write(ipu, IPU_FM_CTRL, tmp);
}

static void enable_dpage_map(struct jz_ipu *ipu)
{
	unsigned int tmp;
//	struct ipu_img_param *img = &ipu->img;

	tmp = reg_read(ipu, IPU_FM_CTRL);
	tmp |= DPAGE_MAP;
	reg_write(ipu, IPU_FM_CTRL, tmp);
#if 0
	tmp = PHYS((unsigned int) img->out_t_addr) & 0xfff;
	reg_write(ipu, IPU_OUT_ADDR, tmp);
	tmp = PHYS((unsigned int) img->out_t_addr);    
	reg_write(ipu, REG_OUT_PHY_T_ADDR, tmp);			    
#endif
}

static void set_yuv_stride(struct jz_ipu *ipu)
{
	unsigned int tmp;
	struct ipu_img_param *img = &ipu->img;

	reg_write(ipu, IPU_Y_STRIDE,  img->stride.y);
	tmp = U_STRIDE(img->stride.u) | V_STRIDE(img->stride.v);
	reg_write(ipu, IPU_UV_STRIDE, tmp);
}

static void config_osd_regs(struct jz_ipu *ipu)
{
	unsigned int tmp;
	
	tmp = GLB_ALPHA(0xa0) | MOD_OSD(0x3) | OSD_PM;
	reg_write(ipu, IPU_OSD_CTRL, tmp);
}

static void stop_ipu_to_lcd(struct jz_ipu *ipu)
{
	unsigned int tmp;
	
	tmp = reg_read(ipu, IPU_TRIG);
	tmp |= IPU_STOP_LCD;
	reg_write(ipu, IPU_TRIG, tmp);
}

static void dump_img(struct jz_ipu *ipu)
{
	struct ipu_img_param *img;
	if (ipu == NULL) {
		dev_err(ipu->dev, "ipu is NULL\n");
		return;
	}

	img = &ipu->img;
	printk("ipu_cmd = %x\n", img->ipu_cmd);
	printk("lcdc_id = %d\n", img->lcdc_id);
	printk("output_mode[%#x]\r\n", (unsigned int) img->output_mode);
	printk("in_width[%#x]\r\n", (unsigned int) img->in_width);
	printk("in_height[%#x]\r\n", (unsigned int) img->in_height);
	printk("in_bpp[%#x]\r\n", (unsigned int) img->in_bpp);
	printk("in_fmt[%#x]\n", (unsigned int) img->in_fmt);
	printk("out_fmt[%#x]\n",(unsigned int) img->out_fmt);
	printk("out_x[%#x]\n",(unsigned int) img->out_x);
	printk("out_y[%#x]\n",(unsigned int) img->out_y);
	printk("out_width[%#x]\r\n", (unsigned int) img->out_width);
	printk("out_height[%#x]\r\n", (unsigned int) img->out_height);
	printk("y_buf_v[%#x]\r\n", (unsigned int) img->y_buf_v);
	printk("u_buf_v[%#x]\r\n", (unsigned int) img->u_buf_v);
	printk("v_buf_v[%#x]\r\n", (unsigned int) img->v_buf_v);
	printk("y_buf_p[%#x]\r\n", (unsigned int) img->y_buf_p);
	printk("u_buf_p[%#x]\r\n", (unsigned int) img->u_buf_p);
	printk("v_buf_p[%#x]\r\n", (unsigned int) img->v_buf_p);
	printk("out_buf_v[%#x]\r\n", (unsigned int) img->out_buf_v);
	printk("out_buf_p[%#x]\r\n", (unsigned int) img->out_buf_p);
	printk("src_page_mapping[%#x]\r\n", (unsigned int)img->src_page_mapping);
	printk("dst_page_mapping[%#x]\r\n", (unsigned int)img->dst_page_mapping);
	printk("y_t_addr[%#x]\r\n", (unsigned int) img->y_t_addr);
	printk("u_t_addr[%#x]\r\n", (unsigned int) img->u_t_addr);
	printk("v_t_addr[%#x]\r\n", (unsigned int) img->v_t_addr);
	printk("out_t_addr[%#x]\r\n", (unsigned int) img->out_t_addr);
	printk("stride.y[%#x]\r\n", (unsigned int) img->stride.y);
	printk("stride.u[%#x]\r\n", (unsigned int) img->stride.u);
	printk("stride.v[%#x]\r\n", (unsigned int) img->stride.v);
	printk("Wdiff[%#x]\r\n", (unsigned int) img->Wdiff);
	printk("Hdiff[%#x]\r\n", (unsigned int) img->Hdiff);
	printk("zoom_mode[%#x]\r\n", (unsigned int) img->zoom_mode);
	printk("hcoef_real_heiht[%#x]\r\n", (unsigned int) img->hcoef_real_heiht);
	printk("vcoef_real_heiht[%#x]\r\n", (unsigned int) img->vcoef_real_heiht);
	printk("hoft_table[%#x]\r\n", (unsigned int) img->hoft_table);
	printk("voft_table[%#x]\r\n", (unsigned int) img->voft_table);
	printk("hcoef_table[%#x]\r\n", (unsigned int) img->hcoef_table);
	printk("voft_table[%#x]\r\n", (unsigned int) img->voft_table);
	printk("hcoef_table[%#x]\r\n", (unsigned int) img->hcoef_table);
	printk("vcoef_table[%#x]\r\n", (unsigned int) img->vcoef_table);
	printk("cube_hcoef_table[%#x]\r\n", (unsigned int) img->cube_hcoef_table);
	printk("cube_vcoef_table[%#x]\r\n", (unsigned int) img->cube_vcoef_table);
	return ;
}

static int jz47_dump_ipu_regs(struct jz_ipu *ipu, int num)
{
	int i, total;
	int *hoft_table, *voft_table; 
	int *hcoef_table, *vcoef_table;
	int hcoef_real_heiht, vcoef_real_heiht;
	struct ipu_img_param *img;
	struct ipu_table *table = &ipu->table;

	dev_dbg(ipu->dev, "enter jz47_dump_ipu_regs\n");
	if (ipu == NULL) {
		dev_err(ipu->dev, "ipu is NULL!\n");
		return -1;
	}
	img = &ipu->img;
	hoft_table = table->hoft; 
	hcoef_table= table->hcoef;
	hcoef_real_heiht = img->hcoef_real_heiht;
	voft_table = table->voft; 
	vcoef_table= table->vcoef;
	vcoef_real_heiht = img->vcoef_real_heiht;

	if (num == -1) {
		total = sizeof(jz47_ipu_regs_name) / sizeof(struct ipu_reg_struct);
		for (i = 0; i < total; i++) {
			printk("ipu_reg: %s: \t0x%08x\r\n", jz47_ipu_regs_name[i].name,
					reg_read(ipu, jz47_ipu_regs_name[i].addr));
		}
	}
	if (num == -2) {
		printk(" //bi-cube resize\nint cube_hcoef_table[H_OFT_LUT][4] = {");
		for ( i = 0 ; i < hcoef_real_heiht ; i++) 
			printk("\t\t\t{0x%02x,0x%02x, 0x%02x, 0x%02x}, \n", 
					table->cube_hcoef[i][0], table->cube_hcoef[i][1], 
					table->cube_hcoef[i][2], table->cube_hcoef[i][3]); 
		printk("};");

		printk(" int cube_vcoef_table[V_OFT_LUT][4] = {");
		for ( i = 0 ; i < vcoef_real_heiht ; i++) 
			printk("\t\t\t{0x%02x,0x%02x, 0x%02x, 0x%02x}, \n", 
					table->cube_vcoef[i][0], table->cube_vcoef[i][1], 
					table->cube_vcoef[i][2], table->cube_vcoef[i][3]); 
		printk("};");
	} else if (num == -3) {
		printk("hcoef_real_heiht=%d\n", hcoef_real_heiht);
		for (i = 0; i < IPU_LUT_LEN; i++) {
			printk("ipu_H_LUT(%02d): hcoef, hoft: %05d %02d\n",
					i, hcoef_table[i], hoft_table[i]);
		}
		printk("vcoef_real_veiht=%d\n", vcoef_real_heiht);
		for (i = 0; i < IPU_LUT_LEN; i++) {
			printk("ipu_V_LUT(%02d): vcoef, voft: %05d %02d\n",
					i, vcoef_table[i], voft_table[i]);
		}
	}

	return 1;
}

static int ipu_dump_regs(struct jz_ipu *ipu)
{
	int ret = 0;
	if (ipu == NULL) {
		dev_err(ipu->dev, "ipu is NULL\n");
		return -1;
	}

	printk("ipu->base: %p\n", ipu->iomem);
	dump_img(ipu);
	ret = jz47_dump_ipu_regs(ipu, -1);
	ret = jz47_dump_ipu_regs(ipu, -2);
	ret = jz47_dump_ipu_regs(ipu, -3);

	return ret;
}

static int ipu_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	int len = 0;
#define PRINT(ARGS...) len += sprintf (page+len, ##ARGS)
	PRINT("hello world!\n");
	return len;
}

/* pixel format definitions to ipu pixel format */
static unsigned int hal_to_ipu_infmt(int hal_fmt)
{
	unsigned int ipu_fmt = IN_FMT_YUV420;

	/* hardware/libhardware/include/hardware/hardware.h */
	switch ( hal_fmt ) {
		case HAL_PIXEL_FORMAT_YCbCr_422_SP:
			ipu_fmt = IN_FMT_YUV422;
			break;
		case HAL_PIXEL_FORMAT_YCbCr_420_SP:
			ipu_fmt = IN_FMT_YUV420;
			break;
		case HAL_PIXEL_FORMAT_YCbCr_422_P:
			ipu_fmt = IN_FMT_YUV422;
			break;
		case HAL_PIXEL_FORMAT_YCbCr_422_I:
			ipu_fmt = IN_FMT_YUV422;
			break;
		case HAL_PIXEL_FORMAT_YCbCr_420_I:
			ipu_fmt = IN_FMT_YUV420;
			break;
		case HAL_PIXEL_FORMAT_YCbCr_420_B:
		case HAL_PIXEL_FORMAT_JZ_YUV_420_B:
			ipu_fmt = IN_FMT_YUV420_B;
			break;		
		case HAL_PIXEL_FORMAT_YCbCr_420_P:
		case HAL_PIXEL_FORMAT_JZ_YUV_420_P:
		default:
			ipu_fmt = IN_FMT_YUV420;
			break;
	}

	return ipu_fmt;
}

/* pixel format definitions to ipu pixel format */
static unsigned int hal_to_ipu_outfmt(int hal_fmt)
{
	unsigned int ipu_fmt = OUT_FMT_RGB888;

	/* hardware/libhardware/include/hardware/hardware.h */
	switch ( hal_fmt ) {
		case HAL_PIXEL_FORMAT_RGBA_8888:
		case HAL_PIXEL_FORMAT_RGBX_8888:
		case HAL_PIXEL_FORMAT_RGB_888:
		case HAL_PIXEL_FORMAT_BGRA_8888:
		case HAL_PIXEL_FORMAT_BGRX_8888:
			ipu_fmt = OUT_FMT_RGB888;
			break;
		case HAL_PIXEL_FORMAT_RGB_565:
			ipu_fmt = OUT_FMT_RGB565;
			break;
		case HAL_PIXEL_FORMAT_RGBA_5551:
			ipu_fmt = OUT_FMT_RGB555;
			break;
		case HAL_PIXEL_FORMAT_YCbCr_422_I:
			ipu_fmt = OUT_FMT_YUV422;
			break;
	}

	return ipu_fmt;
}

static void copy_ipu_tabel_from_user(struct jz_ipu *ipu, struct ipu_img_param *imgp)
{
	struct ipu_table *table = &ipu->table;

	copy_from_user((void *)table->hoft,
			(void *)imgp->hoft_table,
			sizeof(table->hoft));
	copy_from_user((void *)table->voft,
			(void *)imgp->voft_table,
			sizeof(table->voft));
	copy_from_user((void *)table->pic_enhance,
			(void *)imgp->pic_enhance_table,
			sizeof(table->pic_enhance));

	if (imgp->zoom_mode != ZOOM_MODE_BILINEAR) {
		copy_from_user((void *)table->cube_hcoef,
				(void *)imgp->cube_hcoef_table,
				sizeof(table->cube_hcoef));
		copy_from_user((void *)table->cube_vcoef,
				(void *)imgp->cube_vcoef_table,
				sizeof(table->cube_vcoef));
	} else {
		copy_from_user((void *)table->hcoef,
				(void *)imgp->hcoef_table,
				sizeof(table->hcoef));
		copy_from_user((void *)table->vcoef,
				(void *)imgp->vcoef_table,
				sizeof(table->vcoef));
	}
}

static int jz47_set_ipu_resize(struct jz_ipu *ipu)
{
	unsigned int tmp;
	struct ipu_img_param *img;

	dev_dbg(ipu->dev, "enter jz47_set_ipu_resize\n");
	if (ipu == NULL) {
		dev_err(ipu->dev, "ipu is NULL\n");
		return -1;
	}

	img = &ipu->img;

	clear_hvsz_vrsz_bits(ipu);

	if (img->out_width != img->in_width) {
		enable_hrsz(ipu);
	}
	if (img->out_height != img->in_height) {
		enable_vrsz(ipu);
	}

	tmp = ((img->vcoef_real_heiht - 1) << VE_IDX_SFT) | 
		((img->hcoef_real_heiht - 1) << HE_IDX_SFT);
	reg_write(ipu, IPU_RSZ_COEF_INDEX, tmp);

	if (img->zoom_mode != ZOOM_MODE_BILINEAR) {
		sel_zoom_mode(ipu);

		/* set_hrsz_lut_weigth_cube */
		set_hrsz_lut_weigth_cube(ipu);

		/* set_vrsz_lut_weigth_cube */
		set_vrsz_lut_weigth_cube(ipu);
	} else {
		disable_zoom_mode(ipu);

		/* set_vrsz_lut_coef_line */
		set_vrsz_lut_coef_line(ipu);

		/* set_hrsz_lut_coef_line */
		set_hrsz_lut_coef_line(ipu);
	}

	return 0;
}

/* get RGB order */
static unsigned int get_out_fmt_rgb_order(int hal_out_fmt) 
{
	unsigned int order = RGB_OUT_OFT_RGB;

	switch (hal_out_fmt) {
		case HAL_PIXEL_FORMAT_RGBA_8888:
		case HAL_PIXEL_FORMAT_RGBX_8888:
			order = RGB_OUT_OFT_BGR;
			break;
		case HAL_PIXEL_FORMAT_RGB_888:
		case HAL_PIXEL_FORMAT_RGB_565:
		case HAL_PIXEL_FORMAT_BGRA_8888:
		case HAL_PIXEL_FORMAT_BGRX_8888:
		case HAL_PIXEL_FORMAT_RGBA_5551:
		default :
			order = RGB_OUT_OFT_RGB;
			break;
	}

	return order;
}

static int jz47_set_ipu_csc_cfg(struct jz_ipu *ipu, int outW, 
								int outH, int Wdiff, int Hdiff)
{
	struct ipu_img_param *img;
	unsigned int in_fmt, out_fmt, tmp;
	unsigned int in_fmt_tmp, out_fmt_tmp;
	unsigned int out_rgb_order;

	dev_dbg(ipu->dev, "enter jz47_set_ipu_csc_cfg\n");
	if (ipu == NULL) {
		dev_err(ipu->dev, "ipu is NULL\n");
		return -1;
	}

	img = &ipu->img;

	in_fmt = hal_to_ipu_infmt(img->in_fmt);
	out_fmt = hal_to_ipu_outfmt(img->out_fmt);
	/* set RGB order */
	out_rgb_order = get_out_fmt_rgb_order(img->out_fmt);

	switch (in_fmt) {
		case IN_FMT_YUV420:
		case IN_FMT_YUV420_B:
			Hdiff = (Hdiff + 1) & ~1;
			Wdiff = (Wdiff + 1) & ~1;
			break;
		case IN_FMT_YUV422:
			Wdiff = (Wdiff + 1) & ~1;
			break;
		case IN_FMT_YUV444:
		case IN_FMT_YUV411:
			break;
		default:
			printk("Error: 111 Input data format isn't support\n");
			return -1;
	}

	switch (out_fmt) {
		case OUT_FMT_RGB888:
			outW = outW << 2;
			break;
		case OUT_FMT_RGB555:
			outW = outW << 1;
			break;
		case OUT_FMT_RGB565:
			outW = outW << 1;
			break;
	}

	// Set GS register
	set_gs_regs(ipu,Wdiff,Hdiff,outW,outH);

	// Set out stride
	if (img->stride.out != 0) {
		reg_write(ipu, IPU_OUT_STRIDE, img->stride.out);
	} else {
		switch (img->output_mode & IPU_OUTPUT_MODE_MASK) {
			case IPU_OUTPUT_TO_LCD_FG1:
				break;
			case IPU_OUTPUT_TO_LCD_FB0:
			case IPU_OUTPUT_TO_LCD_FB1:
				tmp = img->fb_w * img->in_bpp >> 3;
				reg_write(ipu, IPU_OUT_STRIDE, tmp);
				break;
			case IPU_OUTPUT_TO_FRAMEBUFFER:
			default:
				outW = img->out_width;
				switch (out_fmt) {
					default:
					case OUT_FMT_RGB888:
						outW = outW << 2;
						break;
					case OUT_FMT_RGB555:
					case OUT_FMT_RGB565:
						outW = outW << 1;
						break;
				}
				reg_write(ipu, IPU_OUT_STRIDE, outW);
				break;
		}
	}

	in_fmt_tmp = in_fmt;
	out_fmt_tmp = out_fmt;
	if ( in_fmt == IN_FMT_YUV422 ) {
		printk("*** jz47xx ipu driver: IN_FMT_YUV422 use IN_OFT_Y1VY0U\n");
		in_fmt_tmp |= IN_OFT_VY1UY0;
	}
	if (out_fmt == OUT_FMT_YUV422) {
		printk("*** outformat use YUV_PKG_OUT_OFT_Y0UY1V\n");
		out_fmt_tmp |= YUV_PKG_OUT_OFT_VY1UY0;
	}

	// set Format
	tmp = in_fmt_tmp | out_fmt_tmp | out_rgb_order;
	reg_write(ipu, IPU_D_FMT, tmp);

	// set CSC parameter
	set_csc_param(ipu, in_fmt, out_fmt);

	return 0;
}

static int jz47_set_ipu_stride(struct jz_ipu *ipu)
{
	int in_fmt;
	int out_fmt;
	unsigned int tmp;
	struct ipu_img_param *img;

	dev_dbg(ipu->dev, "Enter jz47_set_ipu_stride\n");
	if (ipu == NULL) {
		dev_err(ipu->dev, "ipu is NULL\n");
		return -1;
	}

	img = &ipu->img;

	in_fmt = hal_to_ipu_infmt(img->in_fmt);
	out_fmt = hal_to_ipu_outfmt(img->out_fmt);

	if (img->stride.y == 0) {	/* set default stride */
		if (in_fmt == IN_FMT_YUV420_B) {
			reg_write(ipu, IPU_Y_STRIDE, img->in_width*16);
		} else if (reg_read(ipu, IPU_FM_CTRL) & SPKG_SEL) {
			reg_write(ipu, IPU_Y_STRIDE, img->in_width*2);
		} else {
			reg_write(ipu, IPU_Y_STRIDE, img->in_width);
		}

		switch (in_fmt) {
			case IN_FMT_YUV420:
			case IN_FMT_YUV422:
				tmp = U_STRIDE(img->in_width/2) | V_STRIDE(img->in_width/2);
				reg_write(ipu, IPU_UV_STRIDE, tmp);
				break;
			case IN_FMT_YUV420_B:
				tmp = U_STRIDE(8*img->in_width) | V_STRIDE(8*img->in_width);
				reg_write(ipu, IPU_UV_STRIDE, tmp);
				break;
			case IN_FMT_YUV444:
				tmp = U_STRIDE(img->in_width) | V_STRIDE(img->in_width);
				reg_write(ipu, IPU_UV_STRIDE, tmp);
				break;
			case IN_FMT_YUV411:
				tmp = U_STRIDE(img->in_width/4) | V_STRIDE(img->in_width/4);
				reg_write(ipu, IPU_UV_STRIDE, tmp);
				break;
			default:
				dev_err(ipu->dev, "Error: 222 Input data format isn't support\n");
				return -1;
		}
	} else {
		reg_write(ipu, IPU_Y_STRIDE, img->stride.y);
		tmp = U_STRIDE(img->stride.u) | V_STRIDE(img->stride.v);
		reg_write(ipu, IPU_UV_STRIDE, tmp);
	}

	return 0;
}

static int jz47_ipu_init(struct jz_ipu *ipu, struct ipu_img_param *imgp)
{
	int ret, in_fmt, out_fmt;
	int outW, outH, Wdiff, Hdiff;
	unsigned int tmp;

	dev_dbg(ipu->dev, "enter jz47_ipu_init\n");

	if (imgp->output_mode & IPU_OUTPUT_TO_LCD_FG1) {
		tmp = reg_read(ipu, IPU_FM_CTRL);			
		tmp |= LCDC_SEL;
		reg_write(ipu, IPU_FM_CTRL, tmp);
	} 

	dev_dbg(ipu->dev, "<-----outW: %d, outH: %d\n", imgp->out_width, imgp->out_height);
	outW = imgp->out_width;
	outH = imgp->out_height;
	Wdiff = imgp->Wdiff;
	Hdiff = imgp->Hdiff;

	dev_dbg(ipu->dev, "outW=%d, outH=%d, Wdiff=%d, Hdiff=%d", outW, outH, Wdiff, Hdiff);

	/* set src and dst format */
	in_fmt = hal_to_ipu_infmt(imgp->in_fmt);
	out_fmt = hal_to_ipu_outfmt(imgp->out_fmt);

	if ((in_fmt == IN_FMT_YUV444) && (out_fmt != OUT_FMT_YUV422)) {
		disable_pkg_mode(ipu);
	}

	if ((in_fmt == IN_FMT_YUV422)) { 
		enable_pkg_mode(ipu);
	}

	ret = jz47_set_ipu_resize(ipu);
	if (ret != 0) {
		dev_err(ipu->dev, "jz47_set_ipu_resize error : out!\n");
		return ret;
	}
	ret = jz47_set_ipu_csc_cfg(ipu, outW, outH, Wdiff, Hdiff);
	if (ret != 0) {
		dev_err(ipu->dev, "jz47_set_ipu_csc_cfg error : out!\n");
		return ret;
	}
	ret = jz47_set_ipu_stride(ipu);
	if (ret != 0) {
		dev_err(ipu->dev, "jz47_set_ipu_stride error : out!\n");
		return ret;
	}

	if (out_fmt == OUT_FMT_YUV422) {
		enable_csc_mode(ipu);
	}
	if (imgp->stlb_base) {
		reg_write(ipu, IPU_SRC_TLB_ADDR, imgp->stlb_base);
	}
	if (imgp->dtlb_base) {
		reg_write(ipu, IPU_DEST_TLB_ADDR, imgp->dtlb_base);
	}

	// set the ctrl
	tmp = reg_read(ipu, IPU_FM_CTRL);
	tmp &= ~(0x300);
	tmp |= IPU_EN;
	reg_write(ipu, IPU_FM_CTRL, tmp);

	if (in_fmt == IN_FMT_YUV420_B) {
		enable_blk_mode(ipu);
	}
	config_osd_regs(ipu);

	return ret;
}

static int ipu_init(struct jz_ipu *ipu, struct ipu_img_param *imgp)
{
	int ret = 0;
	struct ipu_img_param *img;

	if (!ipu || !imgp) {
		dev_err(ipu->dev, "ipu is NULL\n");
		return -1;
	}
	dev_dbg(ipu->dev, "enter ipu_init\n");

	img = &ipu->img;
	dev_dbg(ipu->dev, "--->outW: %d, outH: %d\n", imgp->out_width, imgp->out_height);
	memcpy(img, imgp, sizeof(struct ipu_img_param));
	//*img = *imgp; /* use the new parameter */

	dev_dbg(ipu->dev, "ipu->inited = %d", ipu->inited);
	if (!ipu->inited) {
   		clk_enable(ipu->clk);
		enable_ipu(ipu);
	}

	reset_ipu(ipu);
	ret = jz47_ipu_init(ipu, img);
	if (ret < 0) {
		dev_err(ipu->dev, "jz47_ipu_init failed\n");
		return ret;
	}

	ipu->inited = 1;

	return ret;
}

static int ipu_start(struct jz_ipu *ipu)
{
	unsigned long irq_flags;
	struct ipu_img_param *img;

	dev_dbg(ipu->dev, "enter ipu_start\n");
	if (ipu == NULL) {
		dev_err(ipu->dev, "ipu is NULL\n");
		return -1;
	}

	img = &ipu->img;
	//ipu_dump_regs(ipu);

	if (img->output_mode & IPU_OUTPUT_BLOCK_MODE) {
		/* Wait for current frame to finished */
	  //		dev_info(ipu->dev, "IPU_OUTPUT_BLOCK_MODE\n");
		spin_lock_irqsave(&ipu->update_lock, irq_flags);
		ipu->frame_requested++;
		spin_unlock_irqrestore(&ipu->update_lock, irq_flags);
	}

	clear_ipu_out_end(ipu);
	enable_ctrl_regs(ipu);
	if (img->output_mode & IPU_OUTPUT_BLOCK_MODE) {
		ipu_enable_irq(ipu);
	}

	/* start ipu */
	start_ipu(ipu);
#if 0
	unsigned int tmp;
	int i;
	for (i = 0; i < 10; i++) {
	  tmp = reg_read(ipu, IPU_STATUS);
	  if (tmp & 0x1) {
	    //	    printk("out end ~~~~~~~~~~~~~~~>\n");
	  } else {
	    //	    printk("not end ~~~~~~\n");
	  } 
	}
#endif
	//ipu_dump_regs(ipu);

	if (img->output_mode & IPU_OUTPUT_BLOCK_MODE) {
		/* Wait for current frame to finished */
		if (ipu->frame_requested != ipu->frame_done)
			wait_event_interruptible_timeout(
					ipu->frame_wq, ipu->frame_done == ipu->frame_requested, HZ/10 + 1); /* HZ = 100 */
	}

	return 0;
}

static int ipu_setbuffer(struct jz_ipu *ipu, struct ipu_img_param *imgp)
{
	unsigned int py_buf, pu_buf, pv_buf;
	unsigned int py_buf_v, pu_buf_v, pv_buf_v;
	unsigned int out_buf;
	unsigned int spage_map, dpage_map;
	unsigned int lcdc_sel;
	unsigned int in_fmt;
	unsigned int tmp;

	struct ipu_img_param *img;

	dev_dbg(ipu->dev, "enter ipu_setbuffer\n");
	if (ipu == NULL) {
		dev_err(ipu->dev, "ipu is NULL\n");
		return -1;
	}

	img = &ipu->img;
	if (imgp) {
		unsigned int old_bpp = ipu->img.in_bpp;
		*img = *imgp;
		img->in_bpp = old_bpp;
	}

	spage_map = img->src_page_mapping;
	dpage_map = img->dst_page_mapping;
	lcdc_sel = reg_read(ipu, IPU_FM_CTRL);
	lcdc_sel &= LCDC_SEL;

	py_buf = ((unsigned int) img->y_buf_p);
	pu_buf = ((unsigned int) img->u_buf_p);
	pv_buf = ((unsigned int) img->v_buf_p);

	py_buf_v = (unsigned int) img->y_buf_v;
	pu_buf_v = (unsigned int) img->u_buf_v;
	pv_buf_v = (unsigned int) img->v_buf_v;

	in_fmt = hal_to_ipu_infmt(img->in_fmt);

	dev_dbg(ipu->dev, "py_buf=0x%08x, pu_buf=0x%08x, pv_buf=0x%08x, py_t_buf=0x%08x, pu_t_buf=0x%08x, pv_t_buf=0x%08x", 
			py_buf, pu_buf, pv_buf, py_buf_v, pu_buf_v, pv_buf_v);
	dev_dbg(ipu->dev, "reg_read(IPU_V_BASE + IPU_FM_CTRL)=%08x, spage_map=%x, dpage_map=%x, lcdc_sel=%x",
			reg_read(ipu, IPU_FM_CTRL), spage_map, dpage_map, lcdc_sel);

	if (spage_map != 0) {
		dev_dbg(ipu->dev, "spage_map != 0\n");

		if ((py_buf_v == 0) || (pu_buf_v == 0) || (pv_buf_v == 0)) {
			printk("Can not found source map table, use no map now!\r\n");
			spage_map = 0;
			disable_spage_map(ipu);
		} else {
			dev_dbg(ipu->dev, "we force spage_map to 0\n");

			py_buf = py_buf_v;
			pu_buf = pu_buf_v;
			pv_buf = pv_buf_v;

			enable_spage_map(ipu);
		}
	}

	reg_write(ipu, IPU_Y_ADDR, py_buf);
	reg_write(ipu, IPU_U_ADDR, pu_buf);
	reg_write(ipu, IPU_V_ADDR, pv_buf);

	set_yuv_stride(ipu);

	//	printk("dpage_map = %d) && (lcdc_sel = %d\n", dpage_map, lcdc_sel);
	//	printk("img->out_buf_v = %x, img->out_buf_p = %x\n", img->out_buf_v, img->out_buf_p);
	/* set out put */
	if ((dpage_map != 0) && (lcdc_sel == 0)) {
		if (PHYS((unsigned int) img->out_buf_v) == 0) {
			dev_err(ipu->dev, " Can not found destination map table, use no map now!\r\n");
			dpage_map = 0;
			disable_dpage_map(ipu);

			if (PHYS((unsigned int) img->out_buf_p) == 0) {
				dev_err(ipu->dev, "Can not found the destination buf[%#x]\r\n",
						(unsigned int)img->out_buf_p);
				return (-1);
			} else {
				tmp = PHYS((unsigned int)img->out_buf_p);
				reg_write(ipu, IPU_OUT_ADDR, tmp);  
			}
		} else {
			tmp = PHYS((unsigned int)img->out_buf_v); /* for test */
			reg_write(ipu, IPU_OUT_ADDR, tmp);  /* for test */
			enable_dpage_map(ipu);
		}
	} else {
		dpage_map = 0;
		disable_dpage_map(ipu);
		if (lcdc_sel == 0) {
			if (PHYS((unsigned int)img->out_buf_p) == 0) {
				dev_err(ipu->dev, "Can not found the destination buf[%#x]\r\n",
						(unsigned int)img->out_buf_p);
				return (-1);
			} else {
				dev_dbg(ipu->dev, "img->out_buf_p=0x%x",img->out_buf_p);
				out_buf = img->out_buf_p;
				reg_write(ipu, IPU_OUT_ADDR, PHYS(out_buf));
			}
		}
	}
	reg_write(ipu, IPU_ADDR_CTRL, 0xf); /* enable address reset */

	return 0;
}

static int ipu_stop(struct jz_ipu *ipu)
{
	unsigned int tmp;
	struct ipu_img_param *img;

	dev_dbg(ipu->dev, "enter ipu_stop\n");

	if (!ipu) {
		dev_err(ipu->dev, "ipu is NULL!\n");
		return -1;
	}

	img = &ipu->img;

	if (!(img->output_mode & IPU_OUTPUT_TO_LCD_FG1)) {
		stop_ipu_to_lcd(ipu);
	} else {
		tmp = IPU_STOP;
		reg_write(ipu, IPU_TRIG, tmp);
	}

	return 0;
}

static int ipu_shut(struct jz_ipu *ipu)
{
	struct ipu_img_param *img;

	dev_dbg(ipu->dev, "enter ipu_shut\n");
	if (ipu == NULL) {
		dev_err(ipu->dev, "ipu is NULL");
		return -1;
	}

	img = &ipu->img;

	ipu->inited = 0;

	return 0;
}

static int ipu_set_bypass(struct jz_ipu *ipu)
{
	mutex_lock(&ipu->lock);
	if (!strcmp(ipu->name, "ipu0") && !ipu0_direct) {
		ipu_id = 0;
	} else if (!strcmp(ipu->name, "ipu1") && !ipu1_direct) {
		ipu_id = 1;
	} else {
		dev_err(ipu->dev, "%s nodirect unavailable!\n", ipu->name);
		return -EFAULT;
	}
	bypass += 1;
	mutex_unlock(&ipu->lock);

	return 0;
}

static int ipu_get_bypass_state(struct jz_ipu *ipu)
{
	if (!strcmp(ipu->name, "ipu0") && (ipu_id == 0)) {
		return 0;
	}
	if (!strcmp(ipu->name, "ipu1") && (ipu_id == 1)) {
		return 0;
	}

	return bypass ? -1 : 0;
}

static int ipu_clr_bypass(struct jz_ipu *ipu)
{
	mutex_lock(&ipu->lock);
	if (!strcmp(ipu->name, "ipu0") || !strcmp(ipu->name, "ipu1")) {
		bypass -= 1;
	}
	if (!bypass) {
		ipu_id = 0;
	}
	mutex_unlock(&ipu->lock);

	return 0;
}

static long ipu_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct ipu_img_param img;
	void __user *argp = (void __user *)arg;
	struct miscdevice *dev = file->private_data;
	struct jz_ipu *ipu= container_of(dev, struct jz_ipu, misc_dev);


	if (_IOC_TYPE(cmd) != JZIPU_IOC_MAGIC) {
		dev_err(ipu->dev, "invalid cmd!\n");
		return -EFAULT;
	}

	switch (cmd) {
	case IOCTL_IPU_SHUT:
		ret = ipu_shut(ipu);
		break;
	case IOCTL_IPU_INIT:
		if (copy_from_user(&img, argp, sizeof(struct ipu_img_param))) {	
			dev_err(ipu->dev, "copy_from_user error!!!\n");
			return -EFAULT;
		}		
	//	if (img.version != ipu->img.version) {
	//		dev_err(ipu->dev, "<---Warning, ipu_img_param.version wrong--->\n");
	//		return -EFAULT;
	//	}
		copy_ipu_tabel_from_user(ipu, &img);
		ret = ipu_init(ipu, &img);
		break;
	case IOCTL_IPU_SET_BUFF:
		if (copy_from_user(&img, argp, sizeof(struct ipu_img_param))) {	
			dev_err(ipu->dev, "copy_from_user error!!!\n");
			return -EFAULT;
		}		
		ret = ipu_setbuffer(ipu, &img);
		break;
	case IOCTL_IPU_START:
		ret = ipu_start(ipu);
		break;
	case IOCTL_IPU_STOP:
		ret = ipu_stop(ipu);
		break;
	case IOCTL_IPU_DUMP_REGS:
		ret = ipu_dump_regs(ipu);
		break;
	case IOCTL_IPU_SET_BYPASS:
		ret = ipu_set_bypass(ipu);
		break;
	case IOCTL_IPU_GET_BYPASS_STATE:
		ret = ipu_get_bypass_state(ipu);
		if (copy_to_user(argp, &ret, sizeof(int))) {	
			dev_err(ipu->dev, "copy_to_user error!!!\n");
			return -EFAULT;
		}
		break;
	case IOCTL_IPU_CLR_BYPASS:
		ret = ipu_clr_bypass(ipu);
		break;
	default:
		dev_err(ipu->dev, "invalid command: 0x%08x\n", cmd);
		return -EINVAL;
	}

	return ret;
}

static int ipu_open(struct inode *inode, struct file *filp)
{
	struct miscdevice *dev = filp->private_data;
	struct jz_ipu *ipu = container_of(dev, struct jz_ipu, misc_dev);

	ipu->open_cnt++;
	dev_info(ipu->dev,"Enter ipu_open open_cnt: %d\n", ipu->open_cnt);

	return 0;
}

static int ipu_release(struct inode *inode, struct file *filp)
{
	struct miscdevice *dev = filp->private_data;
	struct jz_ipu *ipu = container_of(dev,struct jz_ipu,misc_dev);

	ipu->open_cnt--;

	dev_info(ipu->dev,"Enter ipu_release open_cnt: %d\n", ipu->open_cnt);

	return 0;
}

static struct file_operations ipu_ops = {
	.owner     	=    THIS_MODULE,
	.open      	=    ipu_open,
	.release   	=    ipu_release,
	.unlocked_ioctl =    ipu_ioctl,
};

static irqreturn_t ipu_irq_handler(int irq, void *data)
{
	struct ipu_img_param *img;
	struct jz_ipu *ipu = (struct jz_ipu *)data;
	unsigned long irq_flags;
	unsigned int dummy_read;

	dummy_read = reg_read(ipu, IPU_STATUS); /* avoid irq looping or disable_irq*/
  	ipu_disable_irq(ipu); // failed
	dev_dbg(ipu->dev, "ipu_irq_handler---------->2");
	img = &ipu->img;

	if (img->output_mode & IPU_OUTPUT_BLOCK_MODE) {
		spin_lock_irqsave(&ipu->update_lock, irq_flags);
	}

	if (img->output_mode & IPU_OUTPUT_BLOCK_MODE) {
		ipu->frame_done = ipu->frame_requested;
		spin_unlock_irqrestore(&ipu->update_lock, irq_flags);
		wake_up(&ipu->frame_wq);
	}

	return IRQ_HANDLED;
}

static int ipu_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct jz_ipu *ipu;

	dev_info(&pdev->dev, "%s %d\n",__func__,__LINE__);

	ipu = (struct jz_ipu *)kzalloc(sizeof(struct jz_ipu), GFP_KERNEL);
	if (!ipu) {
		dev_err(&pdev->dev, "alloc ipu mem_region failed!\n");
		return -ENOMEM;
	}

	if (pdev->id == 0) 
		sprintf(ipu->name,"ipu%d", 1);
	else 
		sprintf(ipu->name,"ipu%d", 0);
		
	//sprintf(ipu->name,"ipu%d",pdev->id);
	ipu->misc_dev.minor     = MISC_DYNAMIC_MINOR;
	ipu->misc_dev.name      = ipu->name;
	ipu->misc_dev.fops      = &ipu_ops;
	ipu->dev = &pdev->dev;

	mutex_init(&ipu->lock);
	spin_lock_init(&ipu->update_lock);
	init_waitqueue_head(&ipu->frame_wq);

	ipu->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!ipu->res) {
		dev_err(&pdev->dev, "failed to get dev resources: %d\n", ret);
		ret = -EINVAL;
		goto err_get_res;
	}
	ipu->res = request_mem_region(ipu->res->start, 
			ipu->res->end - ipu->res->start + 1, pdev->name);
	if (!ipu->res) {
		dev_err(&pdev->dev, "failed to request regs memory region");
		ret = -EINVAL;
		goto err_get_res;	
	}
	ipu->iomem = ioremap(ipu->res->start, resource_size(ipu->res));
	if (!ipu->iomem) {
		dev_err(&pdev->dev, "failed to remap regs memory region: %d\n", ret);
		ret = -EINVAL;
		goto err_ioremap;
	}		

	ipu->irq = platform_get_irq(pdev, 0);
	if (request_irq(ipu->irq, ipu_irq_handler, IRQF_SHARED,
				ipu->name, ipu)) {
		dev_err(&pdev->dev, "request irq failed\n");
		ret = -EINVAL;
		goto err_req_irq;
	}

	ipu->clk = clk_get(ipu->dev,ipu->name);
	if(IS_ERR(ipu->clk)) {
		ret = dev_err(&pdev->dev, "ipu clk get failed!\n");
		goto err_get_clk;
	}

	dev_set_drvdata(&pdev->dev, ipu);
	dev_info(&pdev->dev, "----------%p\n", ipu);

	ret = misc_register(&ipu->misc_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "register misc device failed!\n");
		goto err_set_drvdata;
	}

	/* for test */
	ipu->pde = create_proc_entry(ipu->name, 0444, (void *)ipu);
	if (ipu->pde) ipu->pde->read_proc = ipu_read_proc;

	return 0;

err_set_drvdata:
	clk_put(ipu->clk);
err_get_clk:
	free_irq(ipu->irq, ipu);
err_req_irq:
	iounmap(ipu->iomem);
err_ioremap:
err_get_res:
	return ret;
}

static int ipu_remove(struct platform_device *pdev)
{
	struct jz_ipu *ipu = dev_get_drvdata(&pdev->dev);
	struct resource *res = ipu->res;

	ipu = dev_get_drvdata(&pdev->dev);

	free_irq(ipu->irq, ipu);
	iounmap(ipu->iomem);	

	release_mem_region(res->start, res->end - res->start + 1);

	free_irq(ipu->irq, ipu);

	return 0;
}

static struct platform_driver jz_ipu_driver = {
	.probe 	= ipu_probe,
	.remove = ipu_remove,
	.driver = {
		.name = "jz-ipu",
	},
};

static int __init ipu_setup(void)
{
	platform_driver_register(&jz_ipu_driver);

	return 0;
}

static void __exit ipu_cleanup(void)
{
	platform_driver_unregister(&jz_ipu_driver);
}

module_init(ipu_setup);
module_exit(ipu_cleanup);


MODULE_DESCRIPTION("Jz4780 IPU driver");
MODULE_AUTHOR("Sean Tang <ctang@ingenic.cn>");
MODULE_LICENSE("GPL");
