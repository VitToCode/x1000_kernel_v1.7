/* 
 * kernel/drivers/video/jz4780/jz4780_fb.c
 *
 * Copyright (c) 2012 Ingenic Semiconductor Co., Ltd.
 *              http://www.ingenic.com/
 *
 * Core file for Ingenic Display Controller driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

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
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/memory.h>
#include <linux/earlysuspend.h>
#include <linux/suspend.h>
#include <linux/string.h>
#include <linux/kthread.h>

#include <mach/jzfb.h>

#include "jz4780_fb.h"
#include "regs.h"

static void dump_lcdc_registers(struct jzfb *jzfb);
static void jzfb_enable(struct fb_info *info);
static int jzfb_set_par(struct fb_info *info);

static const struct fb_fix_screeninfo jzfb_fix __devinitdata = {
	.id		= "jzfb",
	.type		= FB_TYPE_PACKED_PIXELS,
	.visual		= FB_VISUAL_TRUECOLOR,
	.xpanstep	= 0,
	.ypanstep	= 1,
	.ywrapstep	= 0,
	.accel		= FB_ACCEL_NONE,
};

static int jzfb_open(struct fb_info *info, int user)
{
	unsigned int tmp;
	struct jzfb *jzfb = info->par;

	dev_info(info->dev,"open count : %d\n",++jzfb->open_cnt);
	dump_lcdc_registers(jzfb);

	/* check the state of FG1 */
	tmp = reg_read(jzfb, LCDC_OSDC);
	if (tmp & LCDC_OSDC_F1EN) {
		dev_info(info->dev, "jzfb_open() OSDC_F1EN: state is wrong\n");
	}

	if(!jzfb->is_enabled && jzfb->vidmem_phys) {
		jzfb_set_par(info);
		jzfb_enable(info);
	}

	return 0;
}

static int jzfb_release(struct fb_info *info, int user)
{
	return 0;
}

static void jzfb_videomode_to_var(struct fb_var_screeninfo *var,
				  const struct fb_videomode *mode)
{
	var->xres = mode->xres;
	var->yres = mode->yres;
	var->xres_virtual = mode->xres;
	var->yres_virtual = mode->yres * NUM_FRAME_BUFFERS;
	var->xoffset = 0;
	var->yoffset = 0;
	var->pixclock = mode->pixclock;
	var->left_margin = mode->left_margin;
	var->right_margin = mode->right_margin;
	var->upper_margin = mode->upper_margin;
	var->lower_margin = mode->lower_margin;
	var->hsync_len = mode->hsync_len;
	var->vsync_len = mode->vsync_len;
	var->sync = mode->sync;
	var->vmode = mode->vmode & FB_VMODE_MASK;
}

static int jzfb_get_controller_bpp(struct jzfb *jzfb)
{
	switch (jzfb->pdata->bpp) {
	case 18:
	case 24:
		return 32;
	case 15:
		return 16;
	default:
		return jzfb->pdata->bpp;
	}
}

static struct fb_videomode *jzfb_get_mode(struct fb_var_screeninfo *var,
					  struct fb_info *info)
{
	size_t i;
	struct jzfb *jzfb = info->par;
	struct fb_videomode *mode = jzfb->pdata->modes;

	for (i = 0; i < jzfb->pdata->num_modes; ++i, ++mode) {
		if (mode->xres == var->xres && mode->yres == var->yres &&
		    mode->vmode == var->vmode && mode->pixclock == var->
		    pixclock && mode->right_margin == var->right_margin)
			return mode;
	}

	return NULL;
}

static void jzfb_config_fg0(struct fb_info *info)
{
	unsigned int rgb_ctrl, cfg ,ctrl = 0;
	struct jzfb *jzfb = info->par;
	struct jzfb_osd_t *osd = &jzfb->osd;
	struct fb_videomode *mode = info->mode;

	osd->fg0.fg = 0;
	osd->fg0.bpp = 32;
	osd->fg0.x = osd->fg0.y = 0;
	osd->fg0.w = mode->xres;
	osd->fg0.h = mode->yres;

	/* OSD mode enable and alpha blending is enabled */
	cfg = LCDC_OSDC_OSDEN | LCDC_OSDC_ALPHAEN;
	cfg |= 1 << 16; /* once transfer two pixels */
	/* OSD control register is read only */

	if (jzfb->fmt_order == FORMAT_X8B8G8R8) {
		rgb_ctrl = LCDC_RGBC_RGBFMT | LCDC_RGBC_ODD_BGR |
			LCDC_RGBC_EVEN_BGR;
	} else {
		/* default: FORMAT_X8R8G8B8*/
		rgb_ctrl = LCDC_RGBC_RGBFMT | LCDC_RGBC_ODD_RGB |
			LCDC_RGBC_EVEN_RGB;
	}

	reg_write(jzfb, LCDC_OSDC, cfg);
	reg_write(jzfb, LCDC_OSDCTRL, ctrl);
	reg_write(jzfb, LCDC_RGBC, rgb_ctrl);
}

static int jzfb_calculate_size(struct fb_info *info,
				   struct jzfb_display_size *size)
{
	struct jzfb *jzfb = info->par;
	struct fb_videomode *mode = info->mode;

	/*
	 * The rules of f0, f1's position:
	 * f0.x + f0.w <= panel.w;
	 * f0.y + f0.h <= panel.h;
	 */
	if ((jzfb->osd.fg0.x + jzfb->osd.fg0.w > mode->xres) |
	    (jzfb->osd.fg0.y + jzfb->osd.fg0.h > mode->yres) |
	    (jzfb->osd.fg0.x >= mode->xres) |
	    (jzfb->osd.fg0.y >= mode->yres)) {
		dev_info(info->dev, "Invalid foreground size or position");
		return -EINVAL;
	}

	/* lcd display area */
	size->fg0_line_size = jzfb->osd.fg0.w * jzfb->osd.fg0.bpp >> 3;
	/* word aligned and in word */
	size->fg0_line_size = ALIGN(size->fg0_line_size, 4) >> 2;
	size->fg0_frm_size = size->fg0_line_size * jzfb->osd.fg0.h;

	/* panel PIXEL_ALIGN stride buffer area */
	size->panel_line_size = ALIGN(mode->xres, PIXEL_ALIGN) *
		(jzfb->osd.fg0.bpp >> 3);
	/* word aligned and in word */
	size->panel_line_size = ALIGN(size->panel_line_size, 4) >> 2;
	jzfb->frm_size = size->panel_line_size * mode->yres << 2;

	size->height_width = (jzfb->osd.fg0.h - 1) << LCDC_DESSIZE_HEIGHT_BIT
		& LCDC_DESSIZE_HEIGHT_MASK;
	size->height_width |=((jzfb->osd.fg0.w -1) << LCDC_DESSIZE_WIDTH_BIT
			      & LCDC_DESSIZE_WIDTH_MASK);

	return 0;
}

static void jzfb_config_tft_lcd_dma(struct fb_info *info,
				    struct jzfb_display_size *size,
				    struct jzfb_framedesc *framedesc)
{
	struct jzfb *jzfb = info->par;

	framedesc->next = jzfb->framedesc_phys;
	framedesc->databuf = jzfb->vidmem_phys;
	framedesc->id = 0xda0;

	framedesc->cmd = LCDC_CMD_SOFINT | LCDC_CMD_FRM_EN;
	if (!jzfb->osd.decompress && !jzfb->osd.block) {
		framedesc->cmd |= size->fg0_frm_size;
		framedesc->offsize = (size->panel_line_size
				      - size->fg0_line_size);
	} else if (jzfb->osd.decompress) {
		framedesc->cmd |= LCDC_CMD_COMPEN;
		framedesc->cmd |= (jzfb->osd.fg0.h & LCDC_CMD_LEN_MASK);
		framedesc->offsize = (jzfb->aosd.dst_stride) >> 2;
	} else {
		framedesc->cmd |= LCDC_CMD_16X16BLOCK;
		framedesc->cmd |= (jzfb->osd.fg0.h & LCDC_CMD_LEN_MASK);
		/* block size */
		/* framedesc->offsize = size->fg0_frm_size; */
	}

	if (framedesc->offsize == 0 || jzfb->osd.decompress) {
		framedesc->page_width = 0;
	} else {
		framedesc->page_width = size->fg0_line_size;
	}

	switch (jzfb->osd.fg0.bpp) {
	case 16:
		framedesc->cpos = LCDC_CPOS_RGB_RGB565
			| LCDC_CPOS_BPP_16;
		break;
	case 30:
		framedesc->cpos = LCDC_CPOS_BPP_30;
		break;
	default:
		if (!jzfb->osd.decompress) {
			framedesc->cpos = LCDC_CPOS_BPP_18_24;
		} else {
			framedesc->cpos = LCDC_CPOS_BPP_CMPS_24;
		}
		break;
	}
	/* global alpha mode */
	framedesc->cpos |= 0;
	/* data has not been premultied */
	framedesc->cpos |= LCDC_CPOS_PREMULTI;
	/* coef_sle 0 use 1 */
	framedesc->cpos |= LCDC_CPOS_COEF_SLE_1;
	framedesc->cpos |= (jzfb->osd.fg0.y << LCDC_CPOS_YPOS_BIT
			    & LCDC_CPOS_YPOS_MASK);
	framedesc->cpos |= (jzfb->osd.fg0.x << LCDC_CPOS_XPOS_BIT
			    & LCDC_CPOS_XPOS_MASK);

	/* fg0 alpha value */
	framedesc->desc_size = 0xa0 << LCDC_DESSIZE_ALPHA_BIT;
	framedesc->desc_size |= size->height_width;
}

static void jzfb_config_smart_lcd_dma(struct fb_info *info,
				      struct jzfb_display_size *size,
				      struct jzfb_framedesc *framedesc)
{
	//struct jzfb *jzfb = info->par;

}

static void jzfb_config_fg1_dma(struct fb_info *info,
				struct jzfb_display_size *size)
{
	struct jzfb *jzfb = info->par;

	/*
	 * the descriptor of DMA 1 just init once
	 * and generally no need to use it
	 */
	if (jzfb->fg1_framedesc)
		return;

	jzfb->fg1_framedesc = jzfb->framedesc[0] + sizeof(struct jzfb_framedesc)
		* (jzfb->desc_num - 1);

	jzfb->fg1_framedesc->next = (uint32_t)virt_to_phys(jzfb->fg1_framedesc);
	jzfb->fg1_framedesc->databuf = 0;
	jzfb->fg1_framedesc->id = 0xda1;
	jzfb->fg1_framedesc->cmd = (LCDC_CMD_SOFINT & ~LCDC_CMD_FRM_EN)
		| size->fg0_frm_size;
	jzfb->fg1_framedesc->offsize = 0;
	jzfb->fg1_framedesc->page_width = 0;

	/* global alpha mode, data has not been premultied, COEF_SLE is 11 */
	jzfb->fg1_framedesc->cpos = LCDC_CPOS_BPP_18_24 | jzfb->osd.fg0.y <<
		LCDC_CPOS_YPOS_BIT | jzfb->osd.fg0.x | LCDC_CPOS_PREMULTI
		| LCDC_CPOS_COEF_SLE_3;

	jzfb->fg1_framedesc->desc_size = size->height_width | 0xa0 <<
		LCDC_DESSIZE_ALPHA_BIT;

	reg_write(jzfb, LCDC_DA1, jzfb->fg1_framedesc->next);
}

static int jzfb_prepare_dma_desc(struct fb_info *info)
{
	int i;
	struct jzfb *jzfb = info->par;
	struct jzfb_display_size *display_size;
	struct jzfb_framedesc (*framedesc)[sizeof(struct jzfb_framedesc)];

	display_size = kzalloc(sizeof(struct jzfb_display_size), GFP_KERNEL);
	framedesc = kzalloc(sizeof(struct jzfb_framedesc) *
			    (jzfb->desc_num - 1), GFP_KERNEL);

	jzfb_calculate_size(info, display_size);

	if (jzfb->pdata->lcd_type != LCD_TYPE_LCM) {
		jzfb_config_tft_lcd_dma(info, display_size, framedesc[0]);
	} else {
		jzfb_config_smart_lcd_dma(info, display_size, framedesc[0]);
	}

	for (i = 0; i < jzfb->desc_num - 1; i++) {
		jzfb->framedesc[i]->next = framedesc[i]->next;
		jzfb->framedesc[i]->databuf = framedesc[i]->databuf;
		jzfb->framedesc[i]->id = framedesc[i]->id;
		jzfb->framedesc[i]->cmd = framedesc[i]->cmd;
		jzfb->framedesc[i]->offsize = framedesc[i]->offsize;
		jzfb->framedesc[i]->page_width = framedesc[i]->page_width;
		jzfb->framedesc[i]->cpos = framedesc[i]->cpos;
		jzfb->framedesc[i]->desc_size = framedesc[i]->desc_size;
	}


	jzfb_config_fg1_dma(info, display_size);
	kzfree(framedesc);
	kzfree(display_size);

	return 0;
}

static int jzfb_check_var(struct fb_var_screeninfo *var, struct fb_info *info)
{
	struct jzfb *jzfb = info->par;
	struct fb_videomode *mode;

	if (var->bits_per_pixel != jzfb_get_controller_bpp(jzfb) &&
	    var->bits_per_pixel != jzfb->pdata->bpp)
		return -EINVAL;
	mode = jzfb_get_mode(var, info);
	if (mode == NULL) {
		dev_err(info->dev, "%s get video mode faild\n", __func__);
		return -EINVAL;
	}

	jzfb_videomode_to_var(var, mode);

	switch (jzfb->pdata->bpp) {
	case 16:
		var->red.offset = 11;
		var->red.length = 5;
		var->green.offset = 5;
		var->green.length = 6;
		var->blue.offset = 0;
		var->blue.length = 5;
		break;
	case 17 ... 32:
		if (jzfb->fmt_order == FORMAT_X8B8G8R8) {
			var->red.offset	= 0;
			var->green.offset = 8;
			var->blue.offset = 16;
		} else {
			/* default: FORMAT_X8R8G8B8*/
			var->red.offset	= 16;
			var->green.offset = 8;
			var->blue.offset = 0;
		}

		var->transp.offset = 24;
		var->transp.length = 8;
		var->red.length = 8;
		var->green.length = 8;
		var->blue.length = 8;
		var->bits_per_pixel = 32;
		break;
	default:
		dev_err(jzfb->dev, "Not support for %d bpp\n",
			jzfb->pdata->bpp);
		break;
	}

	return 0;
}

static void jzfb_lvds_txctrl_is_reset(struct fb_info *info, int reset)
{
	struct jzfb *jzfb = info->par;
	unsigned int tmp;

	tmp = reg_read(jzfb, LVDS_TXCTRL);
	if (reset) {
		/* 0:reset */
		tmp &= ~LVDS_TX_RSTB;
		reg_write(jzfb, LVDS_TXCTRL, tmp);
	} else {
		tmp |= LVDS_TX_RSTB;
		reg_write(jzfb, LVDS_TXCTRL, tmp);
	}
}

static void jzfb_lvds_txpll0_is_pll_en(struct fb_info *info, int pll_en)
{
	struct jzfb *jzfb = info->par;
	unsigned int tmp;

	tmp = reg_read(jzfb, LVDS_TXPLL0);
        if (pll_en) {
		/* 1: enable */
		tmp |= LVDS_PLL_EN;
		reg_write(jzfb, LVDS_TXPLL0, tmp);
	} else {
		tmp &= ~LVDS_PLL_EN;
		reg_write(jzfb, LVDS_TXPLL0, tmp);
	}
}

static void jzfb_lvds_txpll0_is_bg_pwd(struct fb_info *info, int bg)
{
	struct jzfb *jzfb = info->par;
	unsigned int tmp;

	tmp = reg_read(jzfb, LVDS_TXPLL0);
        if (bg) {
		/* 1: power down */
		tmp |= LVDS_BG_PWD;
		reg_write(jzfb, LVDS_TXPLL0, tmp);
	} else {
		tmp &= ~LVDS_BG_PWD;
		reg_write(jzfb, LVDS_TXPLL0, tmp);
	}
}

/* check LVDS_PLL_LOCK lock */
static void jzfb_lvds_check_pll_lock(struct fb_info *info)
{
	struct jzfb *jzfb = info->par;
	unsigned int tmp;
	int count = 0;

	do {
		mdelay(10);
		tmp = reg_read(jzfb, LVDS_TXPLL0);
		if (count++ > 100) {
			dev_info(info->dev, "Wait LVDS PLL LOCK timeout\n");
			break;
		}
	} while (!(tmp & LVDS_PLL_LOCK));
	if (count <= 100) {
		dev_info(info->dev, "LVDS PLL LOCK OK\n");
	}
}

static void jzfb_lvds_txctrl_config(struct fb_info *info)
{
	struct jzfb *jzfb = info->par;
	struct lvds_txctrl *txctrl = &jzfb->pdata->txctrl;
	unsigned int ctrl;

	if (txctrl->data_format) {
		ctrl = LVDS_MODEL_SEL;
	}
	ctrl |= LVDS_TX_RSTB; /* TXCTRL disable reset */
	if (txctrl->clk_edge_falling_7x)
		ctrl |= LVDS_TX_CKBIT_PHA_SEL;
	if (txctrl->clk_edge_falling_1x)
		ctrl |= LVDS_TX_CKBYTE_PHA_SEL;

	 /* 1x clock coarse tuning. TXCTRL: 15-13 bit */
	ctrl |= (txctrl->data_start_edge << LVDS_TX_CKOUT_PHA_S_BIT &
		 LVDS_TX_CKOUT_PHA_S_MASK);
	ctrl |= txctrl->operate_mode; /* TXCTRL: 30, 29, 12, 11, 1, 0 bit */
	/* 1x clock fine tuning. TXCTRL: 10-8 bit */
	ctrl |= ((txctrl->edge_delay << LVDS_TX_DLY_SEL_BIT) &
		 LVDS_TX_DLY_SEL_MASK);

	/* output amplitude control. TXCTRL: 7 6; 5-3 2 bit */
	switch (txctrl->output_amplitude) {
	case VOD_FIX_200MV:
		ctrl &= ~LVDS_TX_AMP_ADJ;
		ctrl &= ~LVDS_TX_LVDS;
		break;
	case VOD_FIX_350MV:
		ctrl &= ~LVDS_TX_AMP_ADJ;
		ctrl |= LVDS_TX_LVDS;
		break;
	default:
		ctrl |= LVDS_TX_AMP_ADJ;
		ctrl &= ~(0xf << 2);
		ctrl |= (txctrl->output_amplitude << 2);
		break;
	}

	reg_write(jzfb, LVDS_TXCTRL, ctrl);
}

static void jzfb_lvds_txpll0_config(struct fb_info *info)
{
	struct jzfb *jzfb = info->par;
	struct lvds_txpll0 *txpll0 = &jzfb->pdata->txpll0;
	unsigned int cfg = 0;
        int indiv = 0, fbdiv = 0;

	if (txpll0->ssc_enable) {
		cfg = LVDS_PLL_SSC_EN;
	} else {
		cfg &= ~LVDS_PLL_SSC_EN;
	}
	if (txpll0->ssc_mode_center_spread)
		cfg |= LVDS_PLL_SSC_MODE;

	/* post divider */
	cfg |= (txpll0->post_divider << LVDS_PLL_POST_DIVA_BIT &
		LVDS_PLL_POST_DIVA_MASK);
       /* feedback_divider */
        if (txpll0->feedback_divider == 260) {
               fbdiv = 0;
        } else if (txpll0->feedback_divider >= 8 && txpll0->feedback_divider
		 <= 259) {
               fbdiv = txpll0->feedback_divider/2 - 2;
        } else {
                dev_info(info->dev, "LVDS PLL feedback divider is error\n");
        }
	cfg |= (fbdiv << LVDS_PLL_PLLN_BIT & LVDS_PLL_PLLN_MASK);

        /*input_divider*/
        if (txpll0->input_divider == 2) {
                indiv = 0;
        } else if (txpll0->input_divider >= 3 && txpll0->input_divider <= 17) {
                indiv = txpll0->input_divider - 2;
        } else if (txpll0->input_divider >= 18 && txpll0->input_divider <= 34) {
                indiv = (txpll0->input_divider) / 2 - 2;
                indiv += 32;
        } else {
                dev_info(info->dev, "LVDS PLL input divider is error\n");
        }
	cfg |= ((indiv << LVDS_PLL_INDIV_BIT) & LVDS_PLL_INDIV_MASK);

	reg_write(jzfb, LVDS_TXPLL0, cfg);
}

static void jzfb_lvds_txpll1_config(struct fb_info *info)
{
	struct jzfb *jzfb = info->par;
	struct lvds_txpll1 *txpll1 = &jzfb->pdata->txpll1;
	unsigned int cfg;

	cfg = (txpll1->charge_pump << LVDS_PLL_ICP_SEL_BIT &
	       LVDS_PLL_ICP_SEL_MASK);
	cfg |= (txpll1->vco_gain << LVDS_PLL_KVCO_BIT & LVDS_PLL_KVCO_MASK);
	cfg |= (txpll1->vco_biasing_current << LVDS_PLL_IVCO_SEL_BIT &
	       LVDS_PLL_IVCO_SEL_MASK);

        if (txpll1->sscn == 130) {
		cfg |= 0;
        }
	if (txpll1->sscn >=3 && txpll1->sscn <=129) {
                cfg |= ((txpll1->sscn - 2) << LVDS_PLL_SSCN_BIT &
			LVDS_PLL_SSCN_MASK);
	}

        if (txpll1->ssc_counter >= 0 && txpll1->ssc_counter <= 15) {
		cfg |= (txpll1->ssc_counter << LVDS_PLL_GAIN_BIT &
			LVDS_PLL_GAIN_MASK);
        }
	if (txpll1->ssc_counter >= 16 && txpll1->ssc_counter <= 8191) {
		cfg |= (txpll1->ssc_counter << LVDS_PLL_COUNT_BIT &
			LVDS_PLL_COUNT_MASK);
        }

	reg_write(jzfb, LVDS_TXPLL1, cfg);
}

static void jzfb_lvds_txectrl_config(struct fb_info *info)
{
	struct jzfb *jzfb = info->par;
	struct lvds_txectrl *txectrl = &jzfb->pdata->txectrl;
	unsigned int cfg;

	cfg = (txectrl->emphasis_level << LVDS_TX_EM_S_BIT &
	       LVDS_TX_EM_S_MASK);
	if (txectrl->emphasis_enable) {
		cfg |= LVDS_TX_EM_EN;
	}
	cfg |= (txectrl->ldo_output_voltage << LVDS_TX_LDO_VO_S_BIT &
		LVDS_TX_LDO_VO_S_MASK);
	cfg |= (txectrl->fine_tuning_7x_clk << LVDS_TX_CK_PHA_FINE_BIT &
		LVDS_TX_CK_PHA_FINE_MASK);
	cfg |= (txectrl->coarse_tuning_7x_clk << LVDS_TX_CK_PHA_COAR_BIT &
		LVDS_TX_CK_PHA_COAR_MASK);

	reg_write(jzfb, LVDS_TXECTRL, cfg);
}

static void jzfb_config_lvds_controller(struct fb_info *info)
{
	jzfb_lvds_txctrl_is_reset(info, 1); /* TXCTRL enable reset */
	jzfb_lvds_txpll0_is_bg_pwd(info, 0); /* band-gap power on */

	mdelay(5);

	jzfb_lvds_txpll0_is_pll_en(info, 1); /* pll enable */
	udelay(20);
	jzfb_lvds_txctrl_is_reset(info, 1); /* TXCTRL disable reset */

	jzfb_lvds_txctrl_config(info);
	jzfb_lvds_txpll0_config(info);
	jzfb_lvds_txpll1_config(info);
	jzfb_lvds_txectrl_config(info);

	jzfb_lvds_check_pll_lock(info);
}

static void jzfb_enable(struct fb_info *info)
{
	uint32_t ctrl;
	struct jzfb *jzfb = info->par;

	mutex_lock(&jzfb->lock);
	if(jzfb->is_enabled) {
		mutex_unlock(&jzfb->lock);
		return;
	}

	clk_enable(jzfb->ldclk);

	reg_write(jzfb, LCDC_STATE, 0);

	reg_write(jzfb, LCDC_DA0, jzfb->framedesc[0]->next);

	ctrl = reg_read(jzfb, LCDC_CTRL);
	ctrl |= LCDC_CTRL_ENA;
	ctrl &= ~LCDC_CTRL_DIS;
	reg_write(jzfb, LCDC_CTRL, ctrl);
	jzfb->is_enabled = 1;
	mutex_unlock(&jzfb->lock);
}

static void jzfb_disable(struct fb_info *info)
{
	uint32_t ctrl, state;
	struct jzfb *jzfb = info->par;
	int count = 1000;

	mutex_lock(&jzfb->lock);
	if(!jzfb->is_enabled) {
		mutex_unlock(&jzfb->lock);
		return;
	}
	ctrl = reg_read(jzfb, LCDC_CTRL);
	ctrl |= LCDC_CTRL_DIS;
	reg_write(jzfb, LCDC_CTRL,ctrl);
	do {
		udelay(100);
		state = reg_read(jzfb, LCDC_STATE);
	} while (!(state & LCDC_STATE_LDD) && count--);
	if (!count) {
		dev_err(jzfb->dev, "LCDC normal disable state wrong");
	}

	clk_disable(jzfb->ldclk);
	jzfb->is_enabled = 0;
	mutex_unlock(&jzfb->lock);
}

static int jzfb_set_par(struct fb_info *info)
{
	struct jzfb *jzfb = info->par;
	struct jzfb_platform_data *pdata = jzfb->pdata;
	struct fb_var_screeninfo *var = &info->var;
	struct fb_videomode *mode;
	int is_enabled;
	uint16_t hds, vds;
	uint16_t hde, vde;
	uint16_t ht, vt;
	uint32_t cfg, ctrl;
	uint32_t smart_cfg = 0;
	uint32_t pcfg;
	unsigned long rate;

	is_enabled = jzfb->is_enabled;

	if(is_enabled)
		jzfb_disable(info);

	mode = jzfb_get_mode(var, info);
	if (mode == NULL) {
		dev_err(info->dev, "%s get video mode faild\n", __func__);
		return -EINVAL;
	}

	if (mode == info->mode)
		return 0;

	info->mode = mode;

	hds = mode->hsync_len + mode->left_margin;
	hde = hds + mode->xres;
	ht = hde + mode->right_margin;

	vds = mode->vsync_len + mode->upper_margin;
	vde = vds + mode->yres;
	vt = vde + mode->lower_margin;

	/*
	 * configure LCDC config register
	 * use 8words descriptor, not use palette
	 */
	cfg = LCDC_CFG_NEWDES | LCDC_CFG_PALBP | LCDC_CFG_RECOVER;
	cfg |= pdata->lcd_type;

	if (!(mode->sync & FB_SYNC_HOR_HIGH_ACT))
		cfg |= LCDC_CFG_HSP;

	if (!(mode->sync & FB_SYNC_VERT_HIGH_ACT))
		cfg |= LCDC_CFG_VSP;

	if (pdata->pixclk_falling_edge)
		cfg |= LCDC_CFG_PCP;

	if (pdata->date_enable_active_low)
		cfg |= LCDC_CFG_DEP;

	/* configure LCDC control register */
	ctrl = LCDC_CTRL_BST_64;
	if (pdata->pinmd)
		ctrl |= LCDC_CTRL_PINMD;

	/* configure smart LCDC registers */
	if(pdata->lcd_type == LCD_TYPE_LCM) {
		smart_cfg = pdata->smart_config.smart_type |
			pdata->smart_config.cmd_width |
			pdata->smart_config.data_width;

		if (pdata->smart_config.clkply_active_rising)
			smart_cfg |= SLCDC_CFG_CLK_ACTIVE_RISING;
		if (pdata->smart_config.rsply_cmd_high)
			smart_cfg |= SLCDC_CFG_RS_CMD_HIGH;
		if (pdata->smart_config.csply_active_high)
			smart_cfg |= SLCDC_CFG_CS_ACTIVE_HIGH;
	}

	if (mode->pixclock) {
		rate = PICOS2KHZ(mode->pixclock) * 1000;
		mode->refresh = rate / vt / ht;
	} else {
		if (pdata->lcd_type == LCD_TYPE_8BIT_SERIAL) {
			rate = mode->refresh * (vt + 2 * mode->xres) * ht;
		} else {
			rate = mode->refresh * vt * ht;
		}
		mode->pixclock = KHZ2PICOS(rate / 1000);

		var->pixclock = mode->pixclock;
	}

	mutex_lock(&jzfb->lock);
	if (!jzfb->is_enabled) {
		clk_enable(jzfb->ldclk);
	}
#ifndef CONFIG_FPGA_TEST
	switch (pdata->lcd_type) {
	case LCD_TYPE_SPECIAL_TFT_1:
	case LCD_TYPE_SPECIAL_TFT_2:
	case LCD_TYPE_SPECIAL_TFT_3:
		reg_write(jzfb, LCDC_SPL, pdata->special_tft_config.spl);
		reg_write(jzfb, LCDC_CLS, pdata->special_tft_config.cls);
		reg_write(jzfb, LCDC_PS, pdata->special_tft_config.ps);
		reg_write(jzfb, LCDC_REV, pdata->special_tft_config.ps);
		break;
	default:
		cfg |= LCDC_CFG_PSM;
		cfg |= LCDC_CFG_CLSM;
		cfg |= LCDC_CFG_SPLM;
		cfg |= LCDC_CFG_REVM;
		break;
	}
#endif

	if(pdata->lcd_type != LCD_TYPE_LCM) {
		reg_write(jzfb, LCDC_VAT, (ht << 16) | vt);
		reg_write(jzfb, LCDC_DAH, (hds << 16) | hde);
		reg_write(jzfb, LCDC_DAV, (vds << 16) | vde);

		reg_write(jzfb, LCDC_HSYNC, mode->hsync_len);
		reg_write(jzfb, LCDC_VSYNC, mode->vsync_len);
	} else {
		reg_write(jzfb, LCDC_VAT, (mode->xres << 16) | mode->yres);
		reg_write(jzfb, LCDC_DAH, mode->xres);
		reg_write(jzfb, LCDC_DAV, mode->yres);

		reg_write(jzfb, LCDC_HSYNC, 0);
		reg_write(jzfb, LCDC_VSYNC, 0);

		reg_write(jzfb, SLCDC_CFG, smart_cfg);
	}

	reg_write(jzfb, LCDC_CFG, cfg);

	reg_write(jzfb, LCDC_CTRL, ctrl);

	pcfg = 0xC0000000 | (511<<18) | (400<<9) | (256<<0) ;
	reg_write(jzfb, LCDC_PCFG, pcfg);

	jzfb_config_fg0(info);

	jzfb_prepare_dma_desc(info);

	clk_set_rate(jzfb->lpclk, rate);
	//clk_set_rate(jzfb->ldclk, rate * 3);

	dev_info(info->dev,"LCDC: PixClock:%lu\n", rate);
	dev_info(info->dev,"LCDC: PixClock:%lu(real)\n",
		 clk_get_rate(jzfb->lpclk));

	/* panel'type is TFT LVDS, need to configure LVDS controller */
	if (pdata->lvds && jzfb->id) {
		jzfb_config_lvds_controller(info);
	}

	/* if dither_en is 1, then set it */
	jzfb_config_image_enh(info);

	if (!jzfb->is_enabled)
		clk_disable(jzfb->ldclk);

	mutex_unlock(&jzfb->lock);

	if(is_enabled)
		jzfb_enable(info);

	return 0;
}

static int jzfb_blank(int blank_mode, struct fb_info *info)
{
	switch (blank_mode) {
	case FB_BLANK_UNBLANK:
		jzfb_enable(info);
		break;
	default:
		jzfb_disable(info);
		break;
	}

	return 0;
}

static int jzfb_alloc_devmem(struct jzfb *jzfb)
{
	unsigned int max_videosize = 0;
	struct fb_videomode *mode = jzfb->pdata->modes;
	void *page;
	int i;

	jzfb->framedesc = dma_alloc_coherent(jzfb->dev, sizeof(*jzfb->framedesc)
					     * jzfb->desc_num,
					     &jzfb->framedesc_phys, GFP_KERNEL);
	if (!jzfb->framedesc)
	return -ENOMEM;

	if (!jzfb->pdata->alloc_vidmem) {
		dev_info(jzfb->dev, "Not allocate frame buffer\n");
		return 0;
	}

	for (i = 0; i < jzfb->pdata->num_modes; ++mode, ++i) {
		if (max_videosize < mode->xres * mode->yres)
			max_videosize = mode->xres * mode->yres;
	}

	max_videosize *= jzfb_get_controller_bpp(jzfb) >> 3;
	max_videosize *= NUM_FRAME_BUFFERS;

	jzfb->vidmem_size = PAGE_ALIGN(max_videosize);
	jzfb->vidmem = dma_alloc_coherent(jzfb->dev,
					  jzfb->vidmem_size,
					  &jzfb->vidmem_phys, GFP_KERNEL);
	if (!jzfb->vidmem)
		return -ENOMEM;

	for (page = jzfb->vidmem;
	     page < jzfb->vidmem + PAGE_ALIGN(jzfb->vidmem_size);
	     page += PAGE_SIZE) {
		SetPageReserved(virt_to_page(page));
	}

	dev_info(jzfb->dev, "Frame buffer size: %d bytes\n",
		 jzfb->vidmem_size);

	return 0;
}

static void jzfb_free_devmem(struct jzfb *jzfb)
{
	dma_free_coherent(jzfb->dev, jzfb->vidmem_size,
			  jzfb->vidmem, jzfb->vidmem_phys);
	dma_free_coherent(jzfb->dev, sizeof(*jzfb->framedesc) * jzfb->desc_num,
			  jzfb->framedesc, jzfb->framedesc_phys);
}

static int jzfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	int next_frm;
	struct jzfb *jzfb = info->par;
	//struct timeval tv1, tv2;

	if (var->xoffset - info->var.xoffset) {
		dev_err(info->dev,"No support for X panning for now\n");
		return -EINVAL;
	}

	next_frm = var->yoffset / var->yres;

	if (jzfb->pdata->lcd_type != LCD_TYPE_INTERLACED_TV ||
	    jzfb->pdata->lcd_type != LCD_TYPE_LCM) {
		jzfb->framedesc[0]->databuf = jzfb->vidmem_phys
			+ jzfb->frm_size * next_frm;
	} else {
		//smart tft spec code here.
	}

	return 0;
}

static void jzfb_set_alpha(struct fb_info *info, struct jzfb_fg_alpha *fg_alpha)
{
	int i;
	int desc_num;
	uint32_t cfg;
	struct jzfb *jzfb = info->par;
	struct jzfb_framedesc *framedesc[jzfb->desc_num];

	if (!fg_alpha->fg) {
		desc_num = jzfb->desc_num -1;
		for (i = 0; i < desc_num; i++) {
			framedesc[i] = jzfb->framedesc[0] + sizeof(
				struct jzfb_framedesc) * i;
		}
	} else {
		desc_num = 1;
		framedesc[0] = jzfb->fg1_framedesc;
	}

	cfg = reg_read(jzfb, LCDC_OSDC);
	if (fg_alpha->enable) {
		cfg |= LCDC_OSDC_ALPHAEN;
		for ( i = 0; i < desc_num; i++) {
			if (!fg_alpha->mode) {
				framedesc[i]->cpos &= ~LCDC_CPOS_ALPHAMD1;
			} else {
				framedesc[i]->cpos |= LCDC_CPOS_ALPHAMD1;
			}
			framedesc[i]->desc_size |= (fg_alpha->value <<
						    LCDC_DESSIZE_ALPHA_BIT
						    & LCDC_DESSIZE_ALPHA_MASK);
		}
	} else {
		cfg &= ~LCDC_OSDC_ALPHAEN;
	}
	reg_write(jzfb, LCDC_OSDC, cfg);
}

static void jzfb_set_background(struct fb_info *info, struct jzfb_bg *background)
{
	struct jzfb *jzfb = info->par;
	uint32_t bg_value;

	bg_value = background->red << LCDC_BGC_RED_OFFSET & LCDC_BGC_RED_MASK;
	bg_value |= (background->green << LCDC_BGC_GREEN_OFFSET
		     & LCDC_BGC_GREEN_MASK);
	bg_value |= (background->blue << LCDC_BGC_BLUE_OFFSET
		     & LCDC_BGC_BLUE_MASK);

	if (!background->fg)
		reg_write(jzfb, LCDC_BGC0, bg_value);
	else
		reg_write(jzfb, LCDC_BGC1, bg_value);
}

static void jzfb_set_colorkey(struct fb_info *info, struct jzfb_color_key *color_key)
{
	struct jzfb *jzfb = info->par;
	uint32_t tmp = 0;

	if (color_key->mode == 1) {
		tmp |= LCDC_KEY_KEYMD;
	} else {
		tmp &= ~LCDC_KEY_KEYMD;
	}

	tmp |= (color_key->red << LCDC_KEY_RED_OFFSET & LCDC_KEY_RED_MASK);
	tmp |= (color_key->green << LCDC_KEY_GREEN_OFFSET
		&  LCDC_KEY_GREEN_MASK);
	tmp |= (color_key->blue << LCDC_KEY_BLUE_OFFSET
		& LCDC_KEY_BLUE_MASK);
	tmp |= LCDC_KEY_KEYEN;

	if (!color_key->fg) {
		reg_write(jzfb, LCDC_KEY0, tmp);
		tmp = reg_read(jzfb, LCDC_KEY0);
	} else {
		reg_write(jzfb, LCDC_KEY1, tmp);
		tmp = reg_read(jzfb, LCDC_KEY1);
	}

	if (color_key->enable == 1) {
		tmp |= LCDC_KEY_KEYEN;
	} else {
		tmp &= ~LCDC_KEY_KEYEN;
	}
	if (!color_key->fg) {
		reg_write(jzfb, LCDC_KEY0, tmp);
	} else {
		reg_write(jzfb, LCDC_KEY1, tmp);
	}
}

static int jzfb_set_foreground_position(struct fb_info *info,
				    struct jzfb_fg_pos *fg_pos)
{
	int i;
	int desc_num;
	struct jzfb *jzfb = info->par;
	struct jzfb_framedesc *framedesc[jzfb->desc_num];
	struct fb_videomode *mode = info->mode;

	/*
	 * The rules of f0, f1's position:
	 * f0.x + f0.w <= panel.w;
	 * f0.y + f0.h <= panel.h;
	 */
	if ((fg_pos->x + jzfb->osd.fg0.w > mode->xres) |
	    (fg_pos->y + jzfb->osd.fg0.h > mode->yres) |
	    (fg_pos->x >= mode->xres) |
	    (fg_pos->y >= mode->yres)) {
		dev_info(info->dev, "Invalid foreground position");
		return -EINVAL;
	}
	jzfb->osd.fg0.x = fg_pos->x;
	jzfb->osd.fg0.y = fg_pos->y;

	if (!fg_pos->fg) {
		desc_num = jzfb->desc_num -1;
		for ( i = 0; i < desc_num; i++) {
			framedesc[i] = jzfb->framedesc[0] + sizeof(
				struct jzfb_framedesc) * i;
		}
	} else {
		framedesc[0] = jzfb->fg1_framedesc;
		desc_num = 1;
	}

	for ( i = 0; i < desc_num; i++) {
		framedesc[i]->cpos |= (((fg_pos->y << LCDC_CPOS_YPOS_BIT) &
					LCDC_CPOS_YPOS_MASK) |
				       (fg_pos->x & LCDC_CPOS_XPOS_MASK));
	}

	return 0;
}

static int jzfb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	int i;
	unsigned int value;
	unsigned int tmp;
	void __user *argp = (void __user *)arg;
	struct jzfb *jzfb = info->par;
	struct jzfb_platform_data *pdata = jzfb->pdata;
	struct fb_videomode *mode = info->mode;
	char (*buf)[MODE_NAME_LEN];
	char name[MODE_NAME_LEN];

	union {
		struct jzfb_fg_pos fg_pos;
		struct jzfb_fg_size fg_size;
		struct jzfb_fg_alpha fg_alpha;
		struct jzfb_bg background;
		struct jzfb_color_key color_key;
	} osd;

	switch (cmd) {
	case JZFB_GET_MODENUM:
		copy_to_user(argp, &pdata->num_modes, sizeof(int));
		break;
	case JZFB_GET_MODELIST:
		buf = kzalloc(sizeof(char) * MODE_NAME_LEN * pdata->num_modes,
			      GFP_KERNEL);
		for (i = 0; i < pdata->num_modes; i++) {
			if (!pdata->modes[i].name)
				continue;
			memcpy(buf[i], pdata->modes[i].name, strlen(
				       pdata->modes[i].name) + 1);
		}

		copy_to_user(argp, buf, sizeof(char) * MODE_NAME_LEN
			     * pdata->num_modes);
		kzfree(buf);
		break;
	case JZFB_SET_VIDMEM:
		if (copy_from_user(&jzfb->vidmem_phys, argp,
				   sizeof(unsigned int)))
			return -EFAULT;
		break;
	case JZFB_SET_MODE:
		if (copy_from_user(name, argp, sizeof(char) * MODE_NAME_LEN))
			return -EFAULT;

		for (i = 0; i < pdata->num_modes; i++) {
			if (!pdata->modes[i].name)
				continue;
			if (!strcmp(pdata->modes[i].name, name)) {
				jzfb_videomode_to_var(&info->var,
						      &pdata->modes[i]);
				return jzfb_set_par(info);
			}
		}
		dev_info(info->dev, "Not find equal video mode at pdata");
		return -EFAULT;
		break;
	case JZFB_ENABLE:
		if (copy_from_user(&value, argp, sizeof(int))) {
			dev_info(info->dev, "copy FB enable value failed\n");
			return -EFAULT;
		}
		if(!jzfb->vidmem_phys) {
			return -EFAULT;
		}
		if (value) {
			jzfb_enable(info);
		} else {
			jzfb_disable(info);
		}
		break;
	case JZFB_SET_FG_SIZE:
		if (copy_from_user(&osd.fg_size, argp, sizeof(
					   struct jzfb_fg_size))) {
			dev_info(info->dev, "copy FG size from user failed\n");
			return -EFAULT;
		} else {
			if ((jzfb->osd.fg0.x + osd.fg_size.w > mode->xres) |
			    (jzfb->osd.fg0.y + osd.fg_size.h > mode->yres)) {
				dev_info(info->dev, "Invalid foreground size");
				return -EINVAL;
			}
			if (!osd.fg_size.fg) {
				jzfb->osd.fg0.w = osd.fg_size.w;
				jzfb->osd.fg0.h = osd.fg_size.h;
				return jzfb_prepare_dma_desc(info);
			} else {
				/* LCDC DMA 1 is not used for now */
			}
		}
		break;
	case JZFB_GET_FG_SIZE:
		if (copy_from_user(&osd.fg_size, argp, sizeof(
					   struct jzfb_fg_size))) {
			dev_info(info->dev, "copy FG size from user failed\n");
			return -EFAULT;
		}

		if (!osd.fg_size.fg) {
			value = reg_read(jzfb, LCDC_SIZE0);
		} else {
			value = reg_read(jzfb, LCDC_SIZE1);
		}
		osd.fg_size.w = value & LCDC_SIZE_WIDTH_MASK;
		osd.fg_size.h = (value & LCDC_SIZE_HEIGHT_MASK) >>
			LCDC_SIZE_HEIGHT_BIT;
		if (copy_to_user(argp, &osd.fg_size, sizeof(
					 struct jzfb_fg_size))) {
			dev_info(info->dev, "copy FG size to user failed\n");
			return -EFAULT;
		}
		break;
	case JZFB_SET_FG_POS:
		if (copy_from_user(&osd.fg_pos, argp, sizeof(
					   struct jzfb_fg_pos))) {
			dev_info(info->dev, "copy FG pos from user failed\n");
			return -EFAULT;
		} else {
			return jzfb_set_foreground_position(info, &osd.fg_pos);
		}
		break;
	case JZFB_GET_FG_POS:
		if (copy_from_user(&osd.fg_pos, argp, sizeof(
					   struct jzfb_fg_pos))) {
			dev_info(info->dev, "copy FG pos from user failed\n");
			return -EFAULT;
		}
		if (!osd.fg_size.fg) {
			value = reg_read(jzfb, LCDC_XYP0);
		} else {
			value = reg_read(jzfb, LCDC_XYP1);
		}
		osd.fg_pos.x = value & LCDC_XYP_XPOS_MASK;
		osd.fg_pos.y = (value & LCDC_XYP_YPOS_MASK) >>
			LCDC_XYP_YPOS_BIT;
		if (copy_to_user(argp, &osd.fg_pos, sizeof(
					 struct jzfb_fg_pos))) {
			dev_info(info->dev, "copy FG pos to user failed\n");
			return -EFAULT;
		}
		break;
	case JZFB_SET_ALPHA:
		if (copy_from_user(&osd.fg_alpha, argp, sizeof(
					   struct jzfb_fg_alpha))) {
			dev_info(info->dev, "copy alpha from user failed\n");
			return -EFAULT;
		} else {
			jzfb_set_alpha(info, &osd.fg_alpha);
		}
		break;
	case JZFB_SET_VSYNCINT:
		if (copy_from_user(&value, argp, sizeof(int)))
			return -EFAULT;
		if (value) {
			/* clear previous EOF flag */
			tmp = reg_read(jzfb, LCDC_STATE);
			reg_write(jzfb, LCDC_STATE, tmp & ~LCDC_STATE_EOF);
			/* enable end of frame interrupt */
			tmp = reg_read(jzfb, LCDC_CTRL);
			reg_write(jzfb, LCDC_CTRL, tmp | LCDC_CTRL_EOFM);
			dev_info(info->dev, "LCDC DMA enable EOF interrupt");
		} else {
			tmp = reg_read(jzfb, LCDC_CTRL);
			reg_write(jzfb, LCDC_CTRL, tmp & ~LCDC_CTRL_EOFM);
			dev_info(info->dev, "LCDC DMA disable EOF interrupt");
		}
		break;
	case JZFB_SET_BACKGROUND:
		if (copy_from_user(&osd.background, argp, sizeof(
					   struct jzfb_bg))) {
			dev_info(info->dev, "copy colorkey from user failed\n");
			return -EFAULT;
		} else {
			jzfb_set_background(info, &osd.background);
		}
		break;
	case JZFB_SET_COLORKEY:
		if (copy_from_user(&osd.color_key, argp, sizeof(
					   struct jzfb_color_key))) {
			dev_info(info->dev, "copy colorkey from user failed\n");
			return -EFAULT;
		}
		jzfb_set_colorkey(info, &osd.color_key);
		break;
	case JZFB_COMPRESS_EN:
		if (copy_from_user(&value, argp, sizeof(int)))
			return -EFAULT;
		if (value) {
			dev_info(info->dev, "LCDC DMA enable decompress mode");
			jzfb->osd.decompress = 1;
			jzfb_prepare_dma_desc(info);
		} else {
			dev_info(info->dev, "LCDC DMA disable decompress mode");
			jzfb->osd.decompress = 0;
			jzfb_prepare_dma_desc(info);
		}
		break;
	case JZFB_16X16_BLOCK_EN:
		if (copy_from_user(&value, argp, sizeof(int)))
			return -EFAULT;
		if (value) {
			dev_info(info->dev, "LCDC DMA enable block mode");
			jzfb->osd.block = 1;
			jzfb_prepare_dma_desc(info);
		} else {
			dev_info(info->dev, "LCDC DMA disable block mode");
			jzfb->osd.block = 0;
			jzfb_prepare_dma_desc(info);
		}
		break;
	case JZFB_IPU0_TO_BUF:
		if (copy_from_user(&value, argp, sizeof(int)))
			return -EFAULT;
		if (jzfb->id != 1) {
			dev_err(info->dev, "LCDC 0 can't accessing 0x130a02c8");
			return -EFAULT;
		}
		tmp = inl(LCDC_CTRL_OUTPUT);
		if (value) {
			tmp |= LCDC_CTRL_OUTPUT_IPU02BUF;
			outl(tmp, LCDC_CTRL_OUTPUT);
		} else {
			tmp &= ~LCDC_CTRL_OUTPUT_IPU02BUF;
			outl(tmp, LCDC_CTRL_OUTPUT);
		}
		break;
	case JZFB_ENABLE_FG1:
		if (copy_from_user(&value, argp, sizeof(int)))
			return -EFAULT;
		if (value) {
			jzfb->fg1_framedesc->cmd |= LCDC_CMD_FRM_EN;
		} else {
			jzfb->fg1_framedesc->cmd &= ~LCDC_CMD_FRM_EN;
		}
		break;
	default:
		jzfb_image_enh_ioctl(info, cmd, arg);
		break;
	}

	return 0;
}

static int jzfb_vsync_timestamp_changed(struct jzfb *jzfb,
					ktime_t prev_timestamp)
{
	rmb();
	return !ktime_equal(prev_timestamp, jzfb->vsync_timestamp);
}

static int jzfb_wait_for_vsync_thread(void *data)
{
	struct jzfb *jzfb = (struct jzfb *)data;

	while (!kthread_should_stop()) {
		ktime_t prev_timestamp = jzfb->vsync_timestamp;
		int ret = wait_event_interruptible_timeout(
			jzfb->vsync_wq,
			jzfb_vsync_timestamp_changed(jzfb, prev_timestamp),
			msecs_to_jiffies(100));
		if (ret > 0) {
			char *envp[2];
			char buf[64];
			snprintf(buf, sizeof(buf), "VSYNC=%llu", ktime_to_ns(
					 jzfb->vsync_timestamp));
			envp[0] = buf;
			envp[1] = NULL;
			kobject_uevent_env(&jzfb->dev->kobj, KOBJ_CHANGE, envp);
		}
	}

	return 0;
}

static irqreturn_t jzfb_irq_handler(int irq, void *data)
{
	unsigned int state;
	struct jzfb *jzfb = (struct jzfb *)data;

	state = reg_read(jzfb, LCDC_STATE);
	if (state & LCDC_STATE_SOF) {
		reg_write(jzfb, LCDC_STATE, state & ~LCDC_STATE_SOF);
		jzfb->vsync_timestamp = ktime_get();
		wmb();
		wake_up_interruptible(&jzfb->vsync_wq);
	}

	return IRQ_HANDLED;
#if 0
	if (state & LCDC_STATE_OFU) {
		reg_write(jzfb, LCDC_STATE, state & ~LCDC_STATE_OFU);
		if (jzfb->irq_cnt++ > 100) {
			unsigned int tmp;
			tmp = reg_read(jzfb, LCDC_CTRL);
			reg_write(jzfb, LCDC_CTRL, tmp & ~LCDC_CTRL_OFUM);
			dev_err(jzfb->dev, "disable OFU irq\n");
		}
		dev_err(jzfb->dev, "%s, Out FiFo underrun\n", __func__);
	}

	return IRQ_HANDLED;
#endif
}

static struct fb_ops jzfb_ops = {
	.owner = THIS_MODULE,
	.fb_open = jzfb_open,
	.fb_release = jzfb_release,
	.fb_check_var = jzfb_check_var,
	.fb_set_par = jzfb_set_par,
	.fb_blank = jzfb_blank,
	.fb_pan_display = jzfb_pan_display,
	.fb_fillrect = cfb_fillrect,
	.fb_copyarea = cfb_copyarea,
	.fb_imageblit = cfb_imageblit,
	.fb_ioctl = jzfb_ioctl,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void jzfb_early_suspend(struct early_suspend *h)
{
	struct jzfb *jzfb = container_of(h, struct jzfb, early_suspend);

	fb_blank(jzfb->fb, FB_BLANK_POWERDOWN);
	fb_set_suspend(jzfb->fb, 1);
}

static void jzfb_late_resume(struct early_suspend *h)
{
	struct jzfb *jzfb = container_of(h, struct jzfb, early_suspend);

	fb_set_suspend(jzfb->fb, 0);
	fb_blank(jzfb->fb, FB_BLANK_UNBLANK);
}
#endif

static void jzfb_display_v_color_bar(struct fb_info *info)
{
	int i,j;
	int w, h;
	int bpp;
	unsigned short *p16;
	unsigned int *p32;
	struct jzfb *jzfb = info->par;
	struct fb_videomode *mode = jzfb->pdata->modes;

	p16 = (unsigned short *)jzfb->vidmem;
	p32 = (unsigned int *)jzfb->vidmem;
	w = jzfb->osd.fg0.w;
	h = jzfb->osd.fg0.h;
	bpp = jzfb->osd.fg0.bpp;

	dev_info(info->dev, "LCD COLOR BAR w,h,bpp(%d,%d,%d)\n", w, h, bpp);

	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++) {
			short c16;
			int c32;
			switch ((j / 10) % 4) {
			case 0:
				c16 = 0xF800;
				c32 = 0x00FF0000;
				break;
			case 1:
				c16 = 0x07C0;
				c32 = 0x0000FF00;
				break;
			case 2:
				c16 = 0x001F;
				c32 = 0x000000FF;
				break;
			default:
				c16 = 0xFFFF;
				c32 = 0xFFFFFFFF;
				break;
			}
			switch (bpp) {
			case 18:
			case 24:
			case 32:
				*p32++ = c32;
				break;
			default:
				*p16 ++ = c16;
			}
		}
		if (w % PIXEL_ALIGN) {
			switch (bpp) {
			case 18:
			case 24:
			case 32:
				p32 += (ALIGN(mode->xres, PIXEL_ALIGN) - w);
				break;
			default:
				p16 += (ALIGN(mode->yres, PIXEL_ALIGN) - w);
				break;
			}
		}
	}
}

static void dump_lcdc_registers(struct jzfb *jzfb)
{
	int i;
	struct device *dev = jzfb->fb->dev;

	/* LCD Controller Resgisters */
	dev_info(dev, "LCDC_CFG: \t0x%08lx\n", reg_read(jzfb, LCDC_CFG));
	dev_info(dev, "LCDC_CTRL:\t0x%08lx\n", reg_read(jzfb, LCDC_CTRL));
	dev_info(dev, "LCDC_STATE:\t0x%08lx\n", reg_read(jzfb, LCDC_STATE));
	dev_info(dev, "LCDC_OSDC:\t0x%08lx\n", reg_read(jzfb, LCDC_OSDC));
	dev_info(dev, "LCDC_OSDCTRL:\t0x%08lx\n", reg_read(jzfb, LCDC_OSDCTRL));
	dev_info(dev, "LCDC_OSDS:\t0x%08lx\n", reg_read(jzfb, LCDC_OSDS));
	dev_info(dev, "LCDC_BGC0:\t0x%08lx\n", reg_read(jzfb, LCDC_BGC0));
	dev_info(dev, "LCDC_BGC1:\t0x%08lx\n", reg_read(jzfb, LCDC_BGC1));
	dev_info(dev, "LCDC_KEY0:\t0x%08lx\n", reg_read(jzfb, LCDC_KEY0));
	dev_info(dev, "LCDC_KEY1:\t0x%08lx\n", reg_read(jzfb, LCDC_KEY1));
	dev_info(dev, "LCDC_ALPHA:\t0x%08lx\n", reg_read(jzfb, LCDC_ALPHA));
	dev_info(dev, "LCDC_IPUR:\t0x%08lx\n", reg_read(jzfb, LCDC_IPUR));
	dev_info(dev, "==================================\n");
	dev_info(dev, "LCDC_VAT: \t0x%08lx\n",reg_read(jzfb, LCDC_VAT));
	dev_info(dev, "LCDC_DAH: \t0x%08lx\n", reg_read(jzfb, LCDC_DAH));
	dev_info(dev, "LCDC_DAV: \t0x%08lx\n", reg_read(jzfb, LCDC_DAV));
	dev_info(dev, "LCDC_VSYNC:\t0x%08lx\n", reg_read(jzfb, LCDC_VSYNC));
	dev_info(dev, "LCDC_HSYNC:\t0x%08lx\n", reg_read(jzfb, LCDC_HSYNC));
	dev_info(dev, "==================================\n");
	dev_info(dev, "LCDC_XYP0:\t0x%08lx\n", reg_read(jzfb, LCDC_XYP0));
	dev_info(dev, "LCDC_XYP1:\t0x%08lx\n", reg_read(jzfb, LCDC_XYP1));
	dev_info(dev, "LCDC_SIZE0:\t0x%08lx\n", reg_read(jzfb, LCDC_SIZE0));
	dev_info(dev, "LCDC_SIZE1:\t0x%08lx\n", reg_read(jzfb, LCDC_SIZE1));
	dev_info(dev, "LCDC_RGBC \t0x%08lx\n", reg_read(jzfb, LCDC_RGBC));
	dev_info(dev, "LCDC_PS:  \t0x%08lx\n", reg_read(jzfb, LCDC_PS));
	dev_info(dev, "LCDC_CLS: \t0x%08lx\n", reg_read(jzfb, LCDC_CLS));
	dev_info(dev, "LCDC_SPL: \t0x%08lx\n", reg_read(jzfb, LCDC_SPL));
	dev_info(dev, "LCDC_REV: \t0x%08lx\n", reg_read(jzfb, LCDC_REV));
	dev_info(dev, "LCDC_IID: \t0x%08lx\n", reg_read(jzfb, LCDC_IID));
	dev_info(dev, "==================================\n");
	dev_info(dev, "LCDC_DA0: \t0x%08lx\n", reg_read(jzfb, LCDC_DA0));
	dev_info(dev, "LCDC_SA0: \t0x%08lx\n", reg_read(jzfb, LCDC_SA0));
	dev_info(dev, "LCDC_FID0:\t0x%08lx\n", reg_read(jzfb, LCDC_FID0));
	dev_info(dev, "LCDC_CMD0:\t0x%08lx\n", reg_read(jzfb, LCDC_CMD0));
	dev_info(dev, "LCDC_OFFS0:\t0x%08lx\n", reg_read(jzfb, LCDC_OFFS0));
	dev_info(dev, "LCDC_PW0: \t0x%08lx\n", reg_read(jzfb, LCDC_PW0));
	dev_info(dev, "LCDC_CNUM0:\t0x%08lx\n", reg_read(jzfb, LCDC_CNUM0));
	dev_info(dev, "LCDC_DESSIZE0:\t0x%08lx\n",
		 reg_read(jzfb, LCDC_DESSIZE0));
	dev_info(dev, "==================================\n");
	dev_info(dev, "LCDC_DA1: \t0x%08lx\n", reg_read(jzfb, LCDC_DA1));
	dev_info(dev, "LCDC_SA1: \t0x%08lx\n", reg_read(jzfb, LCDC_SA1));
	dev_info(dev, "LCDC_FID1:\t0x%08lx\n", reg_read(jzfb, LCDC_FID1));
	dev_info(dev, "LCDC_CMD1:\t0x%08lx\n", reg_read(jzfb, LCDC_CMD1));
	dev_info(dev, "LCDC_OFFS1:\t0x%08lx\n", reg_read(jzfb, LCDC_OFFS1));
	dev_info(dev, "LCDC_PW1: \t0x%08lx\n", reg_read(jzfb, LCDC_PW1));
	dev_info(dev, "LCDC_CNUM1:\t0x%08lx\n", reg_read(jzfb, LCDC_CNUM1));
	dev_info(dev, "LCDC_DESSIZE1:\t0x%08lx\n",
		 reg_read(jzfb, LCDC_DESSIZE1));
	dev_info(dev, "==================================\n");
	dev_info(dev, "LCDC_PCFG:\t0x%08lx\n", reg_read(jzfb, LCDC_PCFG));
	dev_info(dev, "Next is DMA 0 descriptor value in memory\n");
	for (i = 0; i < jzfb->desc_num -1; i++) {
		dev_info(dev, "framedesc[%d]->next: \t0x%08x\n", i,
			 jzfb->framedesc[i]->next);
		dev_info(dev, "framedesc[%d]->databuf:  \t0x%08x\n", i,
			 jzfb->framedesc[i]->databuf);
		dev_info(dev, "framedesc[%d]->id: \t0x%08x\n", i,
			 jzfb->framedesc[i]->id);
		dev_info(dev, "framedesc[%d]->cmd:\t0x%08x\n", i,
			 jzfb->framedesc[i]->cmd);
		dev_info(dev, "framedesc[%d]->offsize:\t0x%08x\n", i,
			 jzfb->framedesc[i]->offsize);
		dev_info(dev, "framedesc[%d]->page_width:\t0x%08x\n", i,
			 jzfb->framedesc[i]->page_width);
		dev_info(dev, "framedesc[%d]->cpos:\t0x%08x\n", i,
			 jzfb->framedesc[i]->cpos);
		dev_info(dev, "framedesc[%d]->desc_size:\t0x%08x\n", i,
			 jzfb->framedesc[i]->desc_size);
	}

	return;
}

static ssize_t dump_lcd(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jzfb *jzfb = dev_get_drvdata(dev);
	dump_lcdc_registers(jzfb);

	return 0;
}

static ssize_t dump_aosd(struct device *dev, struct device_attribute *attr, char *buf)
{
	return 0;
}

static struct device_attribute lcd_sysfs_attrs[] = {
	__ATTR(dump_lcd, S_IRUGO|S_IWUSR, dump_lcd, NULL),
	__ATTR(dump_aosd, S_IRUGO|S_IWUSR, dump_aosd, NULL),
};

static int __devinit jzfb_probe(struct platform_device *pdev)
{
	int ret;
	int i;
	unsigned int lco; /* lcd control output */
	struct jzfb *jzfb;
	struct fb_info *fb;
	struct jzfb_platform_data *pdata = pdev->dev.platform_data;
	struct resource *mem;

	lco = inl(LCDC_CTRL_OUTPUT);
#ifdef CONFIG_MAP_DATABUS_TO_LCDC0
	lco |= LCDC_CTRL_OUTPUT_LCDC02TFT;
#else
	lco &= ~LCDC_CTRL_OUTPUT_LCDC02TFT;
#endif
	outl(lco, LCDC_CTRL_OUTPUT);

	if (!pdata) {
		dev_err(&pdev->dev, "Missing platform data\n");
		return -ENXIO;
	}

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "Failed to get register memory resource\n");
		return -ENXIO;
	}

	mem = request_mem_region(mem->start, resource_size(mem), pdev->name);
	if (!mem) {
		dev_err(&pdev->dev, "Failed to request register memory region\n");
		return -EBUSY;
	}

	fb = framebuffer_alloc(sizeof(struct jzfb), &pdev->dev);
	if (!fb) {
		dev_err(&pdev->dev, "Failed to allocate framebuffer device\n");
		ret = -ENOMEM;
		goto err_release_mem_region;
	}

	fb->fbops = &jzfb_ops;
	fb->flags = FBINFO_DEFAULT;

	jzfb = fb->par;
	jzfb->fb = fb;
	jzfb->dev = &pdev->dev;
	jzfb->pdata = pdata;
	jzfb->id = pdev->id;
	jzfb->mem = mem;

	if (pdata->lcd_type != LCD_TYPE_INTERLACED_TV ||
	    pdata->lcd_type != LCD_TYPE_LCM) {
		jzfb->desc_num = 2;
	} else {
		jzfb->desc_num = 3;
	}

	sprintf(jzfb->clk_name, "lcd%d",pdev->id);
	sprintf(jzfb->pclk_name, "lcd_pclk%d",pdev->id);

	jzfb->ldclk = clk_get(&pdev->dev, jzfb->clk_name);
	if (IS_ERR(jzfb->ldclk)) {
		ret = PTR_ERR(jzfb->ldclk);
		dev_err(&pdev->dev, "Failed to get lcdc clock: %d\n", ret);
		goto err_framebuffer_release;
	}

	jzfb->lpclk = clk_get(&pdev->dev, jzfb->pclk_name);
	if (IS_ERR(jzfb->lpclk)) {
		ret = PTR_ERR(jzfb->lpclk);
		dev_err(&pdev->dev, "Failed to get lcd pixel clock: %d\n", ret);
		goto err_put_ldclk;
	}

	jzfb->base = ioremap(mem->start, resource_size(mem));
	if (!jzfb->base) {
		dev_err(&pdev->dev, "Failed to ioremap register memory region\n");
		ret = -EBUSY;
		goto err_put_lpclk;
	}

	platform_set_drvdata(pdev, jzfb);

	mutex_init(&jzfb->lock);

	fb_videomode_to_modelist(pdata->modes, pdata->num_modes,
				 &fb->modelist);
	jzfb_videomode_to_var(&fb->var, pdata->modes);
	fb->var.width = pdata->width;
	fb->var.height = pdata->height;
	fb->var.bits_per_pixel = pdata->bpp;

	jzfb->fmt_order = FORMAT_X8B8G8R8;

	jzfb_check_var(&fb->var, fb);

	ret = jzfb_alloc_devmem(jzfb);
	if (ret) {
		dev_err(&pdev->dev, "Failed to allocate video memory\n");
		goto err_iounmap;
	}

	fb->fix = jzfb_fix;
	fb->fix.line_length = fb->var.bits_per_pixel * ALIGN(fb->var.xres,
							     PIXEL_ALIGN) >> 3;
	fb->fix.mmio_start = mem->start;
	fb->fix.mmio_len = resource_size(mem);
	fb->fix.smem_start = jzfb->vidmem_phys;
	fb->fix.smem_len =  jzfb->vidmem_size;
	fb->screen_base = jzfb->vidmem;

	jzfb->irq = platform_get_irq(pdev, 0);
	if (request_irq(jzfb->irq, jzfb_irq_handler, IRQF_DISABLED,
			pdev->name, jzfb)) {
		dev_err(&pdev->dev,"request irq failed\n");
		ret = -EINVAL;
		goto err_free_devmem;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	jzfb->early_suspend.suspend = jzfb_early_suspend;
	jzfb->early_suspend.resume = jzfb_late_resume;
	jzfb->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&jzfb->early_suspend);
#endif

	for (i = 0; i < ARRAY_SIZE(lcd_sysfs_attrs); i++) {
		ret = device_create_file(&pdev->dev, &lcd_sysfs_attrs[i]);
		if (ret)
			break;
	}

	if (ret) {
		dev_err(&pdev->dev, "device create file failed\n");
		ret = -EINVAL;
		goto err_free_irq;
	}

	init_waitqueue_head(&jzfb->vsync_wq);
	jzfb->vsync_thread = kthread_run(jzfb_wait_for_vsync_thread,
					 jzfb, "jzfb-vsync");
	if (jzfb->vsync_thread == ERR_PTR(-ENOMEM)) {
		dev_err(&pdev->dev, "Failed to run vsync thread");
		goto err_free_file;
	}

	ret = register_framebuffer(fb);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register framebuffer: %d\n", ret);
		goto err_kthread_stop;
	}

#ifdef CONFIG_FPGA_TEST
	if (jzfb->vidmem_phys) {
		jzfb_set_par(jzfb->fb);
		jzfb_enable(jzfb->fb);
		if (jzfb->id) {
			/* set pixel clock for lcd devide */
			reg_write(jzfb, LCDC_REV, 1 << 16);
		}
		jzfb_display_v_color_bar(jzfb->fb);
	}

	if (!jzfb->id){
		/* set pixel clock for hdmi 480p test */
		reg_write(jzfb, LCDC_REV, 0 << 16);
	}
	//dump_lcdc_registers(jzfb);
#endif

	return 0;

err_kthread_stop:
	kthread_stop(jzfb->vsync_thread);
err_free_file:
	for (i = 0; i < ARRAY_SIZE(lcd_sysfs_attrs); i++) {
		device_remove_file(&pdev->dev, &lcd_sysfs_attrs[i]);
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&jzfb->early_suspend);
#endif
err_free_irq:
	free_irq(jzfb->irq, jzfb);
err_free_devmem:
	jzfb_free_devmem(jzfb);
err_iounmap:
	iounmap(jzfb->base);
err_put_lpclk:
	clk_put(jzfb->lpclk);
err_put_ldclk:
	clk_put(jzfb->ldclk);
err_framebuffer_release:
	framebuffer_release(fb);
err_release_mem_region:
	release_mem_region(mem->start, resource_size(mem));
	return ret;
}

static int __devexit jzfb_remove(struct platform_device *pdev)
{
	struct jzfb *jzfb = platform_get_drvdata(pdev);
	int i;

	kthread_stop(jzfb->vsync_thread);
	jzfb_free_devmem(jzfb);
	platform_set_drvdata(pdev, NULL);

	clk_put(jzfb->lpclk);
	clk_put(jzfb->ldclk);

	for (i = 0; i < ARRAY_SIZE(lcd_sysfs_attrs); i++) {
		device_remove_file(&pdev->dev, &lcd_sysfs_attrs[i]);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&jzfb->early_suspend);
#endif
	iounmap(jzfb->base);
	release_mem_region(jzfb->mem->start, resource_size(jzfb->mem));

	framebuffer_release(jzfb->fb);

	return 0;
}

static void jzfb_shutdown(struct platform_device *pdev)
{
	struct jzfb *jzfb = platform_get_drvdata(pdev);

	fb_blank(jzfb->fb, FB_BLANK_POWERDOWN);
};

static struct platform_driver jzfb_driver = {
	.probe 	= jzfb_probe,
	.remove = jzfb_remove,
	.shutdown = jzfb_shutdown,
	.driver = {
		.name = "jz-fb",
	},
};

static int __init jzfb_init(void)
{
	platform_driver_register(&jzfb_driver);
	return 0;
}

static void __exit jzfb_cleanup(void)
{
	platform_driver_unregister(&jzfb_driver);
}

module_init(jzfb_init);
module_exit(jzfb_cleanup);

MODULE_DESCRIPTION("Jz4780 LCD Controller driver");
MODULE_AUTHOR("Sean Tang <ctang@ingenic.cn>");
MODULE_LICENSE("GPL");
