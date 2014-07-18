/*
 * kernel/drivers/video/jz_fb_v1_2/jz_fb.c
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
#include <linux/gpio.h>
#include <asm/cacheflush.h>
#include <mach/platform.h>
#include <soc/gpio.h>
#include <mach/jzfb.h>
#ifdef CONFIG_JZ_MIPI_DSI
#include "./jz_mipi_dsi/jz_mipi_dsih_hal.h"
#include "./jz_mipi_dsi/jz_mipi_dsi_regs.h"
#endif

#include "jz_fb.h"
#include "regs.h"

extern void dump_dsi_reg(struct dsi_device *dsi);
extern struct platform_device jz_dsi_device;
extern struct platform_driver jz_dsi_driver;

static void dump_lcdc_registers(struct jzfb *jzfb);
static void jzfb_enable(struct fb_info *info);
static void jzfb_disable(struct fb_info *info);
static int jzfb_set_par(struct fb_info *info);

static int showFPS = 0;
static struct jzfb *jzfb;
static const struct fb_fix_screeninfo jzfb_fix __devinitdata = {
	.id = "jzfb",
	.type = FB_TYPE_PACKED_PIXELS,
	.visual = FB_VISUAL_TRUECOLOR,
	.xpanstep = 0,
	.ypanstep = 1,
	.ywrapstep = 0,
	.accel = FB_ACCEL_NONE,
};

void jzfb_clk_enable(struct jzfb *jzfb);
void jzfb_clk_disable(struct jzfb *jzfb);
static int jzfb_open(struct fb_info *info, int user)
{
	struct jzfb *jzfb = info->par;

	dev_info(info->dev, "open count : %d\n", ++jzfb->open_cnt);

	if (!jzfb->is_lcd_en && jzfb->vidmem_phys) {
		jzfb_set_par(info);
		jzfb_enable(info);
	}

	return 0;
}

static int jzfb_release(struct fb_info *info, int user)
{
	return 0;
}

static void
jzfb_videomode_to_var(struct fb_var_screeninfo *var,
		      const struct fb_videomode *mode, int lcd_type)
{
	var->xres = mode->xres;
	var->yres = mode->yres;
	var->xres_virtual = mode->xres;
	var->yres_virtual = mode->yres * NUM_FRAME_BUFFERS;
	var->xoffset = 0;
	var->yoffset = 0;
	var->left_margin = mode->left_margin;
	var->right_margin = mode->right_margin;
	var->upper_margin = mode->upper_margin;
	var->lower_margin = mode->lower_margin;
	var->hsync_len = mode->hsync_len;
	var->vsync_len = mode->vsync_len;
	var->sync = mode->sync;
	var->vmode = mode->vmode & FB_VMODE_MASK;
	if (lcd_type == LCD_TYPE_LCM) {
		uint64_t pixclk =
		    KHZ2PICOS((var->xres + var->left_margin +
			       var->hsync_len) * (var->yres +
						  var->upper_margin +
						  var->lower_margin +
						  var->vsync_len) * 60 / 1000);
		var->pixclock =
		    (mode->pixclock < pixclk) ? pixclk : mode->pixclock;
	} else {
		var->pixclock = mode->pixclock;
	}
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
		if (mode->flag & FB_MODE_IS_VGA) {
			if (mode->xres == var->xres &&
			    mode->yres == var->yres
			    && mode->pixclock == var->pixclock)
				return mode;
		} else {
			if (mode->xres == var->xres && mode->yres == var->yres
			    && mode->vmode == var->vmode
			    && mode->right_margin == var->right_margin) {
				if (jzfb->pdata->lcd_type != LCD_TYPE_LCM) {
					if (mode->pixclock == var->pixclock)
						return mode;
				} else {
					return mode;
				}
			}
		}
	}

	return NULL;
}

static struct fb_videomode *jzfb_checkout_max_vga_videomode(struct fb_info
							    *info)
{
	struct jzfb *jzfb = info->par;
	struct jzfb_platform_data *pdata = jzfb->pdata;
	int i, flag;
	int pix_size = 0;
	flag = 0;
	for (i = 0; i < pdata->num_modes; i++) {
		if ((pdata->modes[i].xres * pdata->modes[i].yres) > pix_size) {
			flag = pdata->modes[i].flag;
			pix_size = pdata->modes[i].xres * pdata->modes[i].yres;
		}
	}

	for (i = 0; i < pdata->num_modes; i++) {
		if (pdata->modes[i].flag != flag)
			continue;
		return &pdata->modes[i];
	}

	if (i > pdata->num_modes) {
		dev_err(jzfb->dev, "Find video mode fail\n");
		return NULL;
	}

	return NULL;
}

static void jzfb_config_fg0(struct fb_info *info)
{
	unsigned int rgb_ctrl, cfg, ctrl = 0;
	struct jzfb *jzfb = info->par;
	struct jzfb_osd_t *osd = &jzfb->osd;
	struct fb_videomode *mode = info->mode;

	if (!mode) {
		dev_err(jzfb->dev, "%s, video mode is NULL\n", __func__);
		return;
	}
	osd->fg0.fg = 0;
	osd->fg0.bpp = jzfb_get_controller_bpp(jzfb) == 32 ? 32 : 16;
	osd->fg0.x = osd->fg0.y = 0;
	osd->fg0.w = mode->xres;
	osd->fg0.h = mode->yres;

	/* OSD mode enable and alpha blending is enabled */
	cfg = LCDC_OSDC_OSDEN | LCDC_OSDC_ALPHAEN;
	cfg |= 1 << 16;		/* once transfer two pixels */

	if (jzfb->fmt_order == FORMAT_X8B8G8R8) {
		rgb_ctrl =
		    LCDC_RGBC_RGBFMT | LCDC_RGBC_ODD_BGR | LCDC_RGBC_EVEN_BGR;
	} else {
		/* default: FORMAT_X8R8G8B8 */
		rgb_ctrl =
		    LCDC_RGBC_RGBFMT | LCDC_RGBC_ODD_RGB | LCDC_RGBC_EVEN_RGB;
	}

	reg_write(jzfb, LCDC_OSDC, cfg);
	reg_write(jzfb, LCDC_OSDCTRL, ctrl);
	reg_write(jzfb, LCDC_RGBC, rgb_ctrl);
}

static int
jzfb_calculate_size(struct fb_info *info, struct jzfb_display_size *size)
{
	struct jzfb *jzfb = info->par;
	struct fb_videomode *mode = info->mode;

	if (!mode) {
		dev_err(jzfb->dev, "%s, video mode is NULL\n", __func__);
		return -EINVAL;
	}
	/*
	 * The rules of f0, f1's position:
	 * f0.x + f0.w <= panel.w;
	 * f0.y + f0.h <= panel.h;
	 */
	if ((jzfb->osd.fg0.x + jzfb->osd.fg0.w > mode->xres) |
	    (jzfb->osd.fg0.y + jzfb->osd.fg0.h > mode->yres) |
	    (jzfb->osd.fg0.x >= mode->xres) | (jzfb->osd.fg0.y >= mode->yres)) {
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
	size->height_width |= ((jzfb->osd.fg0.w - 1) << LCDC_DESSIZE_WIDTH_BIT
			       & LCDC_DESSIZE_WIDTH_MASK);

	return 0;
}

static void
jzfb_config_tft_lcd_dma(struct fb_info *info,
			struct jzfb_display_size *size,
			struct jzfb_framedesc *framedesc)
{
	struct jzfb *jzfb = info->par;

	framedesc->next = jzfb->framedesc_phys;
	framedesc->databuf = jzfb->vidmem_phys;
	framedesc->id = 0xda0;

	framedesc->cmd = LCDC_CMD_EOFINT | LCDC_CMD_FRM_EN;
	if (!jzfb->osd.block) {
		framedesc->cmd |= size->fg0_frm_size;
		framedesc->offsize =
		    (size->panel_line_size - size->fg0_line_size);
	} else {
		framedesc->cmd |= LCDC_CMD_16X16BLOCK;
		framedesc->cmd |= (jzfb->osd.fg0.h & LCDC_CMD_LEN_MASK);
		/* block size */
		/* framedesc->offsize = size->fg0_frm_size; */
	}

	if (framedesc->offsize == 0) {
		framedesc->page_width = 0;
	} else {
		framedesc->page_width = size->fg0_line_size;
	}

	if (jzfb->framedesc[0]->cpos & LCDC_CPOS_ALPHAMD1)
		/* per pixel alpha mode */
		framedesc->cpos = LCDC_CPOS_ALPHAMD1;
	else
		framedesc->cpos = 0;

	switch (jzfb->osd.fg0.bpp) {
	case 16:
		framedesc->cpos |= LCDC_CPOS_RGB_RGB565 | LCDC_CPOS_BPP_16;
		break;
	case 30:
		framedesc->cpos |= LCDC_CPOS_BPP_30;
		break;
	default:
		framedesc->cpos |= LCDC_CPOS_BPP_18_24;
		break;
	}

	/* data has not been premultied */
	framedesc->cpos |= LCDC_CPOS_PREMULTI;
	/* coef_sle 0 use 1 */
	framedesc->cpos |= LCDC_CPOS_COEF_SLE_1;
	framedesc->cpos |= (jzfb->osd.fg0.y << LCDC_CPOS_YPOS_BIT
			    & LCDC_CPOS_YPOS_MASK);
	framedesc->cpos |= (jzfb->osd.fg0.x << LCDC_CPOS_XPOS_BIT
			    & LCDC_CPOS_XPOS_MASK);

	/* fg0 alpha value */
	framedesc->desc_size = 0xff << LCDC_DESSIZE_ALPHA_BIT;
	framedesc->desc_size |= size->height_width;
}

static void
jzfb_config_smart_lcd_dma(struct fb_info *info,
			  struct jzfb_display_size *size,
			  struct jzfb_framedesc *framedesc)
{
	struct jzfb *jzfb = info->par;

	framedesc->next =
	    jzfb->framedesc_phys +
	    sizeof(struct jzfb_framedesc) * (jzfb->desc_num - 2);
	framedesc->databuf = jzfb->vidmem_phys;
	framedesc->id = 0xda0da0;

	framedesc->cmd = LCDC_CMD_EOFINT | LCDC_CMD_FRM_EN;
	framedesc->cmd |= size->fg0_frm_size;

	if (jzfb->framedesc[0]->cpos & LCDC_CPOS_ALPHAMD1)
		/* per pixel alpha mode */
		framedesc->cpos = LCDC_CPOS_ALPHAMD1;
	else
		framedesc->cpos = 0;
	framedesc->offsize = (size->panel_line_size - size->fg0_line_size);
	if (framedesc->offsize == 0) {
		framedesc->page_width = 0;
	} else {
		framedesc->page_width = size->fg0_line_size;
	}

	switch (jzfb->osd.fg0.bpp) {
	case 16:
		framedesc->cpos |= LCDC_CPOS_RGB_RGB565 | LCDC_CPOS_BPP_16;
		break;
	case 30:
		framedesc->cpos |= LCDC_CPOS_BPP_30;
		break;
	default:
		framedesc->cpos |= LCDC_CPOS_BPP_18_24;
		break;
	}
	/* data has not been premultied */
	framedesc->cpos |= LCDC_CPOS_PREMULTI;
	/* coef_sle 0 use 1 */
	framedesc->cpos |= LCDC_CPOS_COEF_SLE_1;
	framedesc->cpos |= (jzfb->osd.fg0.y << LCDC_CPOS_YPOS_BIT
			    & LCDC_CPOS_YPOS_MASK);
	framedesc->cpos |= (jzfb->osd.fg0.x << LCDC_CPOS_XPOS_BIT
			    & LCDC_CPOS_XPOS_MASK);

	/* fg0 alpha value */
	framedesc->desc_size = 0xff << LCDC_DESSIZE_ALPHA_BIT;
	framedesc->desc_size |= size->height_width;

	framedesc[1].next = jzfb->framedesc_phys;
	framedesc[1].databuf = 0;
	framedesc[1].id = 0xda0da1;
	framedesc[1].cmd = LCDC_CMD_CMD | LCDC_CMD_FRM_EN | 0;
	framedesc[1].offsize = 0;
	framedesc[1].page_width = 0;
	framedesc[1].cpos = 0;
	framedesc[1].desc_size = 0;

	framedesc[2].next = jzfb->framedesc_phys;
	framedesc[2].databuf = jzfb->desc_cmd_phys;
	framedesc[2].id = 0xda0da2;
	framedesc[2].offsize = 0;
	framedesc[2].page_width = 0;
	framedesc[2].desc_size = 0;

	/*must to optimize*/
	switch (jzfb->pdata->smart_config.bus_width) {
		case 8:
			framedesc[2].cmd = LCDC_CMD_CMD | LCDC_CMD_FRM_EN | 1;
			framedesc[2].cpos = 4;
			break;
		default:
			framedesc[2].cmd = LCDC_CMD_CMD | LCDC_CMD_FRM_EN | 1;
			framedesc[2].cpos = 1;
			break;
	}
}

static void
jzfb_config_fg1_dma(struct fb_info *info, struct jzfb_display_size *size)
{
	struct jzfb *jzfb = info->par;

	/*
	 * the descriptor of DMA 1 just init once
	 * and generally no need to use it
	 */
	if (jzfb->fg1_framedesc){
		reg_write(jzfb, LCDC_DA1, jzfb->fg1_framedesc->next);
		return;
	}

	jzfb->fg1_framedesc = jzfb->framedesc[0] + (jzfb->desc_num - 1);
	jzfb->fg1_framedesc->next =
	    jzfb->framedesc_phys +
	    sizeof(struct jzfb_framedesc) * (jzfb->desc_num - 1);

	jzfb->fg1_framedesc->databuf = 0;
	jzfb->fg1_framedesc->id = 0xda1;
	jzfb->fg1_framedesc->cmd = (LCDC_CMD_EOFINT & ~LCDC_CMD_FRM_EN)
	    | size->fg0_frm_size;
	jzfb->fg1_framedesc->offsize = 0;
	jzfb->fg1_framedesc->page_width = 0;

	/* global alpha mode, data has not been premultied, COEF_SLE is 11 */
	jzfb->fg1_framedesc->cpos = LCDC_CPOS_BPP_18_24 | jzfb->osd.fg0.y <<
	    LCDC_CPOS_YPOS_BIT | jzfb->osd.fg0.x | LCDC_CPOS_PREMULTI
	    | LCDC_CPOS_COEF_SLE_3;

	jzfb->fg1_framedesc->desc_size = size->height_width | 0xff <<
	    LCDC_DESSIZE_ALPHA_BIT;

	reg_write(jzfb, LCDC_DA1, jzfb->fg1_framedesc->next);
}

static int jzfb_prepare_dma_desc(struct fb_info *info)
{
	int i;
	struct jzfb *jzfb = info->par;
	struct jzfb_display_size *display_size;
	struct jzfb_framedesc *framedesc[MAX_DESC_NUM];

	display_size = kzalloc(sizeof(struct jzfb_display_size), GFP_KERNEL);
	framedesc[0] = kzalloc(sizeof(struct jzfb_framedesc) *
			       (jzfb->desc_num - 1), GFP_KERNEL);
	for (i = 1; i < jzfb->desc_num - 1; i++)
		framedesc[i] = framedesc[0] + i;

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

	if (jzfb->pdata->lcd_type != LCD_TYPE_LCM) {
		reg_write(jzfb, LCDC_DA0, jzfb->framedesc[0]->next);
	} else {
		reg_write(jzfb, LCDC_DA0, (unsigned int)virt_to_phys((void *)
								     jzfb->framedesc[2]));
	}

	jzfb_config_fg1_dma(info, display_size);
	kzfree(framedesc[0]);
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
		dev_err(info->dev, "%s get video mode failed\n", __func__);
		return -EINVAL;
	}

	jzfb_videomode_to_var(var, mode, jzfb->pdata->lcd_type);

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
			var->red.offset = 0;
			var->green.offset = 8;
			var->blue.offset = 16;
		} else {
			/* default: FORMAT_X8R8G8B8 */
			var->red.offset = 16;
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

/* Sent a command without data (18-bit bus, 16-bit index) */
static void slcd_send_mcu_command(struct jzfb *jzfb, unsigned long cmd)
{
	int count = 10000;
	while ((reg_read(jzfb, SLCDC_STATE) & SLCDC_STATE_BUSY) && count--) {
		udelay(10);
	}
	if (count < 0) {
		dev_err(jzfb->dev, "SLCDC wait busy state wrong");
	}
	reg_write(jzfb, SLCDC_DATA, SLCDC_DATA_RS_COMMAND | cmd);
}

static void slcd_send_mcu_data(struct jzfb *jzfb, unsigned long data)
{
	int count = 10000;

	while ((reg_read(jzfb, SLCDC_STATE) & SLCDC_STATE_BUSY) && count--) {
		udelay(10);
	}
	if (count < 0) {
		dev_err(jzfb->dev, "SLCDC wait busy state wrong");
	}

	reg_write(jzfb, SLCDC_DATA, SLCDC_DATA_RS_DATA | data);
}

/* Sent a command with data (18-bit bus, 16-bit index, 16-bit register value) */
static void
slcd_set_mcu_register(struct jzfb *jzfb, unsigned long cmd, unsigned long data)
{
	slcd_send_mcu_command(jzfb, cmd);
	slcd_send_mcu_data(jzfb, data);
}

static void jzfb_slcd_mcu_init(struct fb_info *info)
{
	unsigned int is_lcd_en, i;
	struct jzfb *jzfb = info->par;
	struct jzfb_platform_data *pdata = jzfb->pdata;

	if (pdata->lcd_type != LCD_TYPE_LCM)
		return;

	is_lcd_en = jzfb->is_lcd_en;
	jzfb_enable(info);

#ifndef CONFIG_GPIO_SIMULATE
	if (pdata->smart_config.gpio_for_slcd) {
		pdata->smart_config.gpio_for_slcd();
	}
#endif
	/*
	 *set cmd_width and data_width
	 * */
	if (pdata->smart_config.length_data_table
	    && pdata->smart_config.data_table) {
		for (i = 0; i < pdata->smart_config.length_data_table; i++) {
			switch (pdata->smart_config.data_table[i].type) {
			case 0:
				slcd_set_mcu_register(jzfb,
						      pdata->
						      smart_config.data_table
						      [i].reg,
						      pdata->
						      smart_config.data_table
						      [i].value);
				break;
			case 1:
				slcd_send_mcu_command(jzfb,
						      pdata->
						      smart_config.data_table
						      [i].value);
				break;
			case 2:
				slcd_send_mcu_data(jzfb,
						   pdata->
						   smart_config.data_table[i].
						   value);
				break;
			default:
				dev_err(jzfb->dev, "Unknow SLCD data type\n");
				break;
			}
			udelay(pdata->smart_config.data_table[i].udelay);
			if (pdata->smart_config.newcfg_datatx_type
			    && pdata->smart_config.newcfg_cmdtx_type) {
				gpio_direction_output(GPIO_PC(21), 1);
				udelay(5);
				gpio_direction_output(GPIO_PC(21), 0);
			}
		}
		{
			int count = 10000;
			while ((reg_read(jzfb, SLCDC_STATE) & SLCDC_STATE_BUSY)
			       && count--) {
				udelay(10);
			}
			if (count < 0) {
				dev_err(jzfb->dev,
					"SLCDC wait busy state wrong");
			}

		}

		if (pdata->smart_config.newcfg_datatx_type
		    && pdata->smart_config.newcfg_cmdtx_type) {
			gpio_direction_output(GPIO_PC(21), 1);
			udelay(5);
			gpio_direction_output(GPIO_PC(21), 0);

		}
	}


	if (pdata->smart_config.data_width2) {
		int tmp = reg_read(jzfb, SLCDC_CFG);
		tmp &= ~SMART_LCD_DWIDTH_MASK;
		tmp |= pdata->smart_config.data_width2;
		reg_write(jzfb, SLCDC_CFG, tmp);
		dev_info(jzfb->dev, "mcu init over.the cfg is %08x\n", tmp);
	}
	if (pdata->smart_config.data_new_times2) {
		int tmp = reg_read(jzfb, SLCDC_CFG_NEW);
		tmp &= ~(3<<8); //mask the 8~9bit
		tmp |= pdata->smart_config.data_new_times2;
		reg_write(jzfb, SLCDC_CFG_NEW, tmp);
		dev_info(jzfb->dev, "the slcd  slcd_cfg_new is %08x\n", tmp);
	}

#ifdef CONFIG_FB_JZ_DEBUG
	/*for register mode test,
	 * write test code according to the lcd panel
	 **/
#endif

		if (pdata->smart_config.newcfg_datatx_type
		    && pdata->smart_config.newcfg_cmdtx_type) {
			gpio_direction_output(GPIO_PC(21), 1);
			udelay(5);
			gpio_direction_output(GPIO_PC(21), 0);

		}

	/*recovery ori status*/
	if (!is_lcd_en) {
		jzfb_disable(info);
	}

}

void jzfb_clk_enable(struct jzfb *jzfb)
{
	if(jzfb->is_clk_en){
		return;
	}
	clk_enable(jzfb->clk);
	jzfb->is_clk_en = 1;
}
void jzfb_clk_disable(struct jzfb *jzfb)
{
	if(!jzfb->is_clk_en){
		return;
	}
	jzfb->is_clk_en = 0;
	clk_disable(jzfb->clk);
}

static void jzfb_enable(struct fb_info *info)
{
	uint32_t ctrl;
	struct jzfb *jzfb = info->par;
	mutex_lock(&jzfb->lock);
	if (jzfb->is_lcd_en) {
		mutex_unlock(&jzfb->lock);
		return;
	}

	reg_write(jzfb, LCDC_STATE, 0);
	reg_write(jzfb, LCDC_OSDS, 0);
	ctrl = reg_read(jzfb, LCDC_CTRL);
	ctrl |= LCDC_CTRL_ENA;
	ctrl &= ~LCDC_CTRL_DIS;
	reg_write(jzfb, LCDC_CTRL, ctrl);

	jzfb->is_lcd_en = 1;
	mutex_unlock(&jzfb->lock);
}

static void jzfb_disable(struct fb_info *info)
{
	uint32_t ctrl;
	struct jzfb *jzfb = info->par;
	int count = 10000;

	mutex_lock(&jzfb->lock);
	if (!jzfb->is_lcd_en) {
		mutex_unlock(&jzfb->lock);
		return;
	}

	if (jzfb->pdata->lcd_type != LCD_TYPE_LCM) {
		ctrl = reg_read(jzfb, LCDC_CTRL);
		ctrl |= LCDC_CTRL_DIS;
		reg_write(jzfb, LCDC_CTRL, ctrl);
		while (!(reg_read(jzfb, LCDC_STATE) & LCDC_STATE_LDD)
		       && count--) {
			udelay(10);
		}
		if (count >= 0) {
			ctrl = reg_read(jzfb, LCDC_STATE);
			ctrl &= ~LCDC_STATE_LDD;
			reg_write(jzfb, LCDC_STATE, ctrl);
		} else {
			dev_err(jzfb->dev, "LCDC normal disable state wrong");
		}
	} else {
		/* SLCD and TVE only support quick disable */
		ctrl = reg_read(jzfb, LCDC_CTRL);
		ctrl &= ~LCDC_CTRL_ENA;
		reg_write(jzfb, LCDC_CTRL, ctrl);
	}

	jzfb->is_lcd_en = 0;
	mutex_unlock(&jzfb->lock);

}

static int jzfb_set_par(struct fb_info *info)
{
	struct jzfb *jzfb = info->par;
	struct jzfb_platform_data *pdata = jzfb->pdata;
	struct fb_var_screeninfo *var = &info->var;
	struct fb_videomode *mode;
	int is_lcd_en;
	int is_pclk_en;
	uint16_t hds, vds;
	uint16_t hde, vde;
	uint16_t ht, vt;
	uint32_t cfg, ctrl;
	uint32_t smart_cfg = 0, smart_ctrl = 0;
	uint32_t smart_new_cfg = 0;
	uint32_t smart_wtime = 0, smart_tas = 0;
	uint32_t pcfg;
	unsigned long rate;

	mode = jzfb_get_mode(var, info);
	if (mode == NULL) {
		dev_err(info->dev, "%s get video mode failed\n", __func__);
		return -EINVAL;
	}
#if 0
	if (mode == info->mode){
		printk("+++++mode=%d  info->mode=%d\n",mode,info->mode);
		return 0;
	}
#endif
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
	 * ! M200 NOT SUPPORT PALETTE FUNCTION, DO NOT SET LCDC_CFG_PALBP(BIT27), IT CAUGHT BPP16 COLOR ERROR.
	 */
	/*SET PALBP TO AVOID FORMAT TRANSFER */

	cfg = LCDC_CFG_NEWDES | LCDC_CFG_RECOVER;
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
	ctrl = LCDC_CTRL_BST_64 | LCDC_CTRL_OFUM;
	if (pdata->pinmd)
		ctrl |= LCDC_CTRL_PINMD;

	pcfg = 0xC0000000 | (511 << 18) | (400 << 9) | (256 << 0);

	/* configure smart LCDC registers */
	if (pdata->lcd_type == LCD_TYPE_LCM) {
		smart_cfg = pdata->smart_config.smart_type |
		    pdata->smart_config.cmd_width | pdata->
		    smart_config.data_width;

		if (pdata->smart_config.clkply_active_rising)
			smart_cfg |= SLCDC_CFG_CLK_ACTIVE_RISING;
		if (pdata->smart_config.rsply_cmd_high)
			smart_cfg |= SLCDC_CFG_RS_CMD_HIGH;
		if (pdata->smart_config.csply_active_high)
			smart_cfg |= SLCDC_CFG_CS_ACTIVE_HIGH;

		smart_ctrl = SLCDC_CTRL_DMA_MODE;
		//smart_ctrl |= SLCDC_CTRL_GATE_MASK; //for saving power
		smart_ctrl &= ~SLCDC_CTRL_GATE_MASK;

		smart_ctrl |= (SLCDC_CTRL_NEW_MODE | SLCDC_CTRL_NOT_USE_TE); //new slcd mode
		smart_ctrl &= ~SLCDC_CTRL_MIPI_MODE;
		smart_new_cfg |= pdata->smart_config.data_new_width |
		    pdata->smart_config.data_new_times;

		if (pdata->smart_config.newcfg_6800_md)
			smart_new_cfg |= SLCDC_NEW_CFG_6800_MD;
		if (pdata->smart_config.newcfg_datatx_type
		    && pdata->smart_config.newcfg_cmdtx_type)
			smart_new_cfg |=
			    SLCDC_NEW_CFG_DTYPE_SERIAL |
			    SLCDC_NEW_CFG_CTYPE_SERIAL;
		if (pdata->smart_config.newcfg_cmd_9bit)
			smart_new_cfg |= SLCDC_NEW_CFG_CMD_9BIT;

		smart_wtime = 0;
		smart_tas = 0;
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

	/*set reg,and enable lcd after set all reg*/
	is_lcd_en = jzfb->is_lcd_en;
	jzfb_disable(info);

	mutex_lock(&jzfb->lock);

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

	if (pdata->lcd_type != LCD_TYPE_LCM) {
		reg_write(jzfb, LCDC_VAT, (ht << 16) | vt);

		/*
		 * If you are using a VGA output,
		 * then you need to last a pix of the value is set to 0,
		 * you can add the background size widened, that is,
		 * the increase in the value of the VDE, plus at least 1,
		 * the maximum can not exceed the value of VT.
		 * Example:
		 * LCD monitor manufacturers: ViewSonic
		 * Model: VA926.
		 * Set the resolution: 1280 * 1024
		 * To set the parameters in xxx_lcd.c need to pay attention to:
		 * 1. The timing need to use the standard timing
		 * 2. If you will be using a VGA display, .flag = FB_MODE_IS_VGA
		 * 3. sync = FB_SYNC_HOR_HIGH_ACT | FB_SYNC_VERT_HIGH_ACT,
		 *        The sync trigger for the high level active
		 * 4. pixclk_falling_edge = 1, PIX clock trigger high level
		 * 5. date_enable_active_low = 1, DATA enable is high level
		 */

		if (mode->flag & FB_MODE_IS_VGA) {
			if ((hde + 8) <= ht)
				hde += 8;
			else if ((hde + 1) <= ht)
				hde += 1;
			/*
			   if(vds > 2 && (vde + 2) <= vt){
			   vds -= 2;
			   vde += 2;
			   }
			 */
			info->fix.line_length = info->var.bits_per_pixel *
			    ALIGN(mode->xres, PIXEL_ALIGN) >> 3;
		}

		reg_write(jzfb, LCDC_DAH, (hds << 16) | hde);
		reg_write(jzfb, LCDC_DAV, (vds << 16) | vde);

		reg_write(jzfb, LCDC_HSYNC, mode->hsync_len);
		reg_write(jzfb, LCDC_VSYNC, mode->vsync_len);
	} else {
#ifdef CONFIG_JZ_MIPI_DSI
		smart_cfg |= 1 << 16;
		smart_new_cfg |= 4 << 13;
		smart_ctrl |= 1 << 7 | 1 << 6;

		mipi_dsih_hal_gen_set_mode(jzfb->dsi, 1);
		mipi_dsih_hal_dpi_color_coding(jzfb->dsi,
			jzfb->dsi->video_config->color_coding);

#endif
		reg_write(jzfb, LCDC_VAT, (mode->xres << 16) | mode->yres);
		reg_write(jzfb, LCDC_DAH, mode->xres);
		reg_write(jzfb, LCDC_DAV, mode->yres);

		reg_write(jzfb, LCDC_HSYNC, 0);
		reg_write(jzfb, LCDC_VSYNC, 0);

		reg_write(jzfb, SLCDC_CFG, smart_cfg);
		reg_write(jzfb, SLCDC_CTRL, smart_ctrl);

		reg_write(jzfb, SLCDC_CFG_NEW, smart_new_cfg);
		reg_write(jzfb, SLCDC_WTIME, smart_wtime);
		reg_write(jzfb, SLCDC_TAS, smart_tas);

	}

	reg_write(jzfb, LCDC_CFG, cfg);
	ctrl |= reg_read(jzfb, LCDC_CTRL);
	reg_write(jzfb, LCDC_CTRL, ctrl);
	reg_write(jzfb, LCDC_PCFG, pcfg);

	jzfb_config_fg0(info);
	jzfb_prepare_dma_desc(info);

	mutex_unlock(&jzfb->lock);

	is_pclk_en = clk_is_enabled(jzfb->pclk);
	if(is_pclk_en)
		clk_disable(jzfb->pclk);
	clk_set_rate(jzfb->pclk, rate);
	clk_enable(jzfb->pclk);

	if (!jzfb->is_suspend) {
		/*avoid printk after every wake up */
		dev_info(jzfb->dev, "LCDC: PixClock:%lu\n", rate);
		dev_info(jzfb->dev, "LCDC: PixClock:%lu(real)\n", clk_get_rate(jzfb->pclk));
	}

	jzfb_config_image_enh(info);
	if (pdata->lcd_type == LCD_TYPE_LCM) {
		jzfb_slcd_mcu_init(info);

#ifdef CONFIG_SLCDC_CONTINUA
		smart_ctrl &= ~SLCDC_CTRL_DMA_MODE;
#else
		smart_ctrl |= SLCDC_CTRL_DMA_START;
#endif
		smart_ctrl |= SLCDC_CTRL_DMA_EN;

#ifdef CONFIG_SLCDC_USE_TE
		smart_ctrl &= ~SLCDC_CTRL_NOT_USE_TE;
#endif

		if (pdata->smart_config.newcfg_fmt_conv) {
			smart_new_cfg = reg_read(jzfb, SLCDC_CFG_NEW);
			smart_new_cfg |= SLCDC_NEW_CFG_FMT_CONV_EN;
			reg_write(jzfb, SLCDC_CFG_NEW, smart_new_cfg);
		}
		reg_write(jzfb, SLCDC_CTRL, smart_ctrl);
	}
#ifdef CONFIG_JZ_MIPI_DSI
	else {
		cfg |= 1 << 24;
		reg_write(jzfb, LCDC_CFG, cfg);
		jzfb->dsi->master_ops->video_cfg(jzfb->dsi);
	}

#endif

	if (is_lcd_en) {
		jzfb_enable(info);
	}

	return 0;
}

static int jzfb_blank(int blank_mode, struct fb_info *info)
{
	int count = 10000;
	unsigned long ctrl;
	struct jzfb *jzfb = info->par;

	switch (blank_mode) {
	case FB_BLANK_UNBLANK:
		reg_write(jzfb, LCDC_STATE, 0);
		reg_write(jzfb, LCDC_OSDS, 0);
		ctrl = reg_read(jzfb, LCDC_CTRL);
		ctrl |= LCDC_CTRL_ENA;
		ctrl &= ~LCDC_CTRL_DIS;
		reg_write(jzfb, LCDC_CTRL, ctrl);

		mutex_lock(&jzfb->suspend_lock);
		if (jzfb->is_suspend) {
			jzfb->is_suspend = 0;
			mutex_unlock(&jzfb->suspend_lock);
			if (jzfb->pdata->lcd_type == LCD_TYPE_LCM) {
				jzfb_slcd_mcu_init(jzfb->fb);
				msleep(50);
				ctrl = reg_read(jzfb, SLCDC_CTRL);
				ctrl &= ~SLCDC_CTRL_DMA_MODE;
				ctrl |= SLCDC_CTRL_DMA_EN;
				reg_write(jzfb, SLCDC_CTRL, ctrl);
			}
		} else {
			mutex_unlock(&jzfb->suspend_lock);
		}
		jzfb->is_lcd_en = 1;
		break;
	default:
		if (jzfb->pdata->lcd_type != LCD_TYPE_LCM) {
			ctrl = reg_read(jzfb, LCDC_CTRL);
			ctrl |= LCDC_CTRL_DIS;
			reg_write(jzfb, LCDC_CTRL, ctrl);
			while (!(reg_read(jzfb, LCDC_STATE) & LCDC_STATE_LDD)
			       && count--) {
				udelay(10);
			}
			if (count >= 0) {
				ctrl = reg_read(jzfb, LCDC_STATE);
				ctrl &= ~LCDC_STATE_LDD;
				reg_write(jzfb, LCDC_STATE, ctrl);
			} else {
				dev_err(jzfb->dev, "LCDC disable state wrong");
			}
		} else {
			ctrl = reg_read(jzfb, LCDC_CTRL);
			ctrl &= ~LCDC_CTRL_ENA;
			reg_write(jzfb, LCDC_CTRL, ctrl);

			ctrl = reg_read(jzfb, SLCDC_CTRL);
			ctrl &= ~SLCDC_CTRL_DMA_EN;
			reg_write(jzfb, SLCDC_CTRL, ctrl);
		}
		jzfb->is_lcd_en = 0;
	}

	return 0;
}

static int jzfb_alloc_devmem(struct jzfb *jzfb)
{
	int i;
	unsigned int videosize = 0;
	struct fb_videomode *mode;
	void *page;

	jzfb->framedesc[0] =
	    dma_alloc_coherent(jzfb->dev,
			       sizeof(struct jzfb_framedesc) * jzfb->desc_num,
			       &jzfb->framedesc_phys, GFP_KERNEL);
	if (!jzfb->framedesc[0])
		return -ENOMEM;
	for (i = 1; i < jzfb->desc_num; i++)
		jzfb->framedesc[i] = jzfb->framedesc[0] + i;

	if (!jzfb->pdata->alloc_vidmem) {
		dev_info(jzfb->dev, "Not allocate frame buffer\n");
		return 0;
	}

	mode = jzfb->pdata->modes;
	if (!mode) {
		dev_err(jzfb->dev, "Checkout video mode fail\n");
		return -EINVAL;
	}

	if (mode->flag & FB_MODE_IS_VGA) {
		mode = jzfb_checkout_max_vga_videomode(jzfb->fb);
		if (!mode) {
			dev_err(jzfb->dev,
				"Checkout VGA max pix video mode fail\n");
			return -EINVAL;
		}
	}

	videosize = ALIGN(mode->xres, PIXEL_ALIGN) * mode->yres;
	videosize *= jzfb_get_controller_bpp(jzfb) >> 3;
	videosize *= NUM_FRAME_BUFFERS;

	jzfb->vidmem_size = PAGE_ALIGN(videosize);

	/**
	 * Use the dma alloc coherent has waste some space,
	 * If you need to alloc buffer for dma, open it,
	 * else close it and use the Kmalloc.
	 * And in jzfb_free_devmem() function is also set.
	 */
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

	if (jzfb->pdata->lcd_type == LCD_TYPE_LCM) {
		int i;
		unsigned long cmd[2] = { 0 }, *ptr;
		jzfb->desc_cmd_vidmem = dma_alloc_coherent(jzfb->dev, PAGE_SIZE,
							   &jzfb->desc_cmd_phys,
							   GFP_KERNEL);
		ptr = (unsigned long *)jzfb->desc_cmd_vidmem;
		cmd[0] = jzfb->pdata->smart_config.write_gram_cmd;
		switch (jzfb->pdata->smart_config.bus_width) {
		case 18:
			cmd[0] &= 0xffff;
			cmd[0] = cmd[0] << 16 | cmd[0];
			cmd[1] = cmd[0];
			break;
		case 16:
			cmd[0] &= 0xffff;
			cmd[0] = cmd[0] << 16 | cmd[0];
			cmd[1] = cmd[0];
			break;
		case 9:
			{
				unsigned long c = cmd[0];
				c &= 0x0ff;
				c &= 0x01ff;
				cmd[0] = (c << 16) | (c << 0);
				cmd[1] = cmd[0];
			}
			break;
		case 8:
			{
#ifdef CONFIG_LCD_LH155
				unsigned long c = cmd[0];
				c &= 0xffff;
				cmd[0] = (c << 16) | (c << 0);
				cmd[1] = cmd[0];
#else
				cmd[1] = cmd[0]; //for mirasol panel ok
#endif
			}
			break;
		default:
			dev_err(jzfb->dev, "Don't support %d bit bus\n",
				jzfb->pdata->smart_config.bus_width);
			break;
		}
		for (i = 0; i < 4; i += 2) {
			ptr[i] = cmd[0];
			ptr[i + 1] = cmd[1];
		}
	}

	dev_info(jzfb->dev, "Frame buffer size: %d bytes\n", jzfb->vidmem_size);

	return 0;
}

static void jzfb_free_devmem(struct jzfb *jzfb)
{
	dma_free_coherent(jzfb->dev, jzfb->vidmem_size,
			  jzfb->vidmem, jzfb->vidmem_phys);
	dma_free_coherent(jzfb->dev,
			  sizeof(struct jzfb_framedesc) * jzfb->desc_num,
			  jzfb->framedesc, jzfb->framedesc_phys);
	if (jzfb->pdata->lcd_type == LCD_TYPE_LCM) {
		dma_free_coherent(jzfb->dev, PAGE_SIZE,
				  jzfb->desc_cmd_vidmem, jzfb->desc_cmd_phys);
	}
}

#define SPEC_TIME_IN_NS (1000*1000000)  /* 1s */
static int jzfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	int next_frm;
	unsigned int tmp = 0;
	struct jzfb *jzfb = info->par;

	{/*debug*/
		static struct timespec time_now, time_last;
		struct timespec time_interval;
		long long  interval_in_ns;
		unsigned int interval_in_ms;
		static unsigned int fpsCount = 0;

		jzfb->pan_display_count++;
		if(showFPS){
			switch(showFPS){
				case 1:
					fpsCount++;
					time_now = current_kernel_time();
					time_interval = timespec_sub(time_now, time_last);
					interval_in_ns = timespec_to_ns(&time_interval);
					if ( interval_in_ns > SPEC_TIME_IN_NS ) {
						printk(KERN_DEBUG " Pan display FPS: %d\n",fpsCount);
						fpsCount = 0;
						time_last = time_now;
					}
					break;
				case 2:
					time_now = current_kernel_time();
					time_interval = timespec_sub(time_now, time_last);
					interval_in_ns = timespec_to_ns(&time_interval);
					interval_in_ms = (unsigned long)interval_in_ns/1000000;
					printk(KERN_DEBUG " Pan display interval: %d\n",interval_in_ms);
					time_last = time_now;
					break;
				default:
					break;
			}
		}
	}/*end debug*/
	if (var->xoffset - info->var.xoffset) {
		dev_err(info->dev, "No support for X panning for now\n");
		return -EINVAL;
	}

	if (var->yres == 720 || var->yres == 1080) {	/* work around for HDMI device */
		switch (var->yoffset) {
		case 1440:
		case (1080 * 2):
			next_frm = 2;
			break;
		case 720:
		case (1080 * 1):
			next_frm = 1;
			break;
		default:
			next_frm = 0;
			break;
		}
	} else
		next_frm = var->yoffset / var->yres;

	jzfb->current_buffer = next_frm;

	if (jzfb->pdata->lcd_type != LCD_TYPE_INTERLACED_TV &&
	    jzfb->pdata->lcd_type != LCD_TYPE_LCM) {
		if (!jzfb->osd.block) {
			jzfb->framedesc[0]->databuf = jzfb->vidmem_phys
			    + jzfb->frm_size * next_frm;
		} else {
			/* 16x16 block mode */
		}
	} else if (jzfb->pdata->lcd_type == LCD_TYPE_LCM) {
		/* smart tft spec code here */
		jzfb->framedesc[0]->databuf = jzfb->vidmem_phys
		    + jzfb->frm_size * next_frm;
		if (!jzfb->is_lcd_en)
			return -EINVAL;;

#ifndef CONFIG_SLCDC_CONTINUA
		if (jzfb->pdata->lcd_callback_ops.dma_transfer_begin){
			jzfb->pdata->lcd_callback_ops.dma_transfer_begin(jzfb);
			tmp = reg_read(jzfb, SLCDC_CTRL);
			tmp |= SLCDC_CTRL_DMA_START | SLCDC_CTRL_DMA_MODE;
			reg_write(jzfb, SLCDC_CTRL, tmp);
		}else{
			tmp = reg_read(jzfb, SLCDC_CTRL);
			tmp |= SLCDC_CTRL_DMA_START | SLCDC_CTRL_DMA_MODE;
			reg_write(jzfb, SLCDC_CTRL, tmp);
		}
#endif
	} else {
		/* LCD_TYPE_INTERLACED_TV */
	}

	return 0;
}

static void jzfb_set_alpha(struct fb_info *info, struct jzfb_fg_alpha *fg_alpha)
{
	int i;
	int desc_num;
	uint32_t cfg;
	struct jzfb *jzfb = info->par;
	struct jzfb_framedesc *framedesc;

	if (!fg_alpha->fg) {
		desc_num = jzfb->desc_num - 1;
		framedesc = jzfb->framedesc[0];
	} else {
		desc_num = 1;
		framedesc = jzfb->fg1_framedesc;
	}

	cfg = reg_read(jzfb, LCDC_OSDC);
	if (fg_alpha->enable) {
		cfg |= LCDC_OSDC_ALPHAEN;
		for (i = 0; i < desc_num; i++) {
			if (!fg_alpha->mode) {
				(framedesc + i)->cpos &= ~LCDC_CPOS_ALPHAMD1;
			} else {
				(framedesc + i)->cpos |= LCDC_CPOS_ALPHAMD1;
			}
			(framedesc + i)->desc_size &= ~LCDC_DESSIZE_ALPHA_MASK;
			(framedesc + i)->desc_size |= (fg_alpha->value <<
						       LCDC_DESSIZE_ALPHA_BIT
						       &
						       LCDC_DESSIZE_ALPHA_MASK);
		}
	} else {
		dev_info(info->dev, "Failed to set alpha\n");
	}
	reg_write(jzfb, LCDC_OSDC, cfg);
}

static void
jzfb_set_background(struct fb_info *info, struct jzfb_bg *background)
{
	struct jzfb *jzfb = info->par;
	uint32_t bg_value;

	bg_value = background->red << LCDC_BGC_RED_OFFSET & LCDC_BGC_RED_MASK;
	bg_value |= (background->green << LCDC_BGC_GREEN_OFFSET
		     & LCDC_BGC_GREEN_MASK);
	bg_value |=
	    (background->blue << LCDC_BGC_BLUE_OFFSET & LCDC_BGC_BLUE_MASK);

	if (!background->fg)
		reg_write(jzfb, LCDC_BGC0, bg_value);
	else
		reg_write(jzfb, LCDC_BGC1, bg_value);
}

static void
jzfb_set_colorkey(struct fb_info *info, struct jzfb_color_key *color_key)
{
	struct jzfb *jzfb = info->par;
	uint32_t tmp = 0;

	if (color_key->mode == 1) {
		tmp |= LCDC_KEY_KEYMD;
	} else {
		tmp &= ~LCDC_KEY_KEYMD;
	}

	tmp |= (color_key->red << LCDC_KEY_RED_OFFSET & LCDC_KEY_RED_MASK);
	tmp |=
	    (color_key->green << LCDC_KEY_GREEN_OFFSET & LCDC_KEY_GREEN_MASK);
	tmp |= (color_key->blue << LCDC_KEY_BLUE_OFFSET & LCDC_KEY_BLUE_MASK);
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

static int
jzfb_set_foreground_position(struct fb_info *info, struct jzfb_fg_pos *fg_pos)
{
	int i;
	int desc_num;
	struct jzfb *jzfb = info->par;
	struct jzfb_framedesc *framedesc;
	struct fb_videomode *mode = info->mode;

	if (!mode) {
		dev_err(jzfb->dev, "%s, video mode is NULL\n", __func__);
		return -EINVAL;
	}
	/*
	 * The rules of f0, f1's position:
	 * f0.x + f0.w <= panel.w;
	 * f0.y + f0.h <= panel.h;
	 */
	if ((fg_pos->x + jzfb->osd.fg0.w > mode->xres) |
	    (fg_pos->y + jzfb->osd.fg0.h > mode->yres) |
	    (fg_pos->x >= mode->xres) | (fg_pos->y >= mode->yres)) {
		dev_info(info->dev, "Invalid foreground position");
		return -EINVAL;
	}
	jzfb->osd.fg0.x = fg_pos->x;
	jzfb->osd.fg0.y = fg_pos->y;

	if (!fg_pos->fg) {
		desc_num = jzfb->desc_num - 1;
		framedesc = jzfb->framedesc[0];
	} else {
		desc_num = 1;
		framedesc = jzfb->fg1_framedesc;
	}

	for (i = 0; i < desc_num; i++) {
		(framedesc + i)->cpos |= (((fg_pos->y << LCDC_CPOS_YPOS_BIT) &
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
	int *buf;

	union {
		struct jzfb_fg_pos fg_pos;
		struct jzfb_fg_size fg_size;
		struct jzfb_fg_alpha fg_alpha;
		struct jzfb_bg background;
		struct jzfb_color_key color_key;
		struct jzfb_mode_res res;
	} osd;

	switch (cmd) {
	case JZFB_GET_MODENUM:
		copy_to_user(argp, &pdata->num_modes, sizeof(int));
		break;
	case JZFB_GET_MODELIST:
		buf = kzalloc(sizeof(int) * pdata->num_modes, GFP_KERNEL);
		for (i = 0; i < pdata->num_modes; i++) {
			if (!pdata->modes[i].flag)
				continue;
			buf[i] = pdata->modes[i].flag;
		}
		copy_to_user(argp, buf, sizeof(int) * pdata->num_modes);
		kzfree(buf);
		break;
	case JZFB_SET_VIDMEM:
		if (copy_from_user
		    (&jzfb->vidmem_phys, argp, sizeof(unsigned int)))
			return -EFAULT;
		break;
	case JZFB_SET_MODE:
		if (copy_from_user(&value, argp, sizeof(int)))
			return -EFAULT;

		for (i = 0; i < pdata->num_modes; i++) {
			if (pdata->modes[i].flag == value) {
				jzfb_videomode_to_var(&info->var,
						      &pdata->modes[i],
						      jzfb->pdata->lcd_type);
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

		if (value) {
			jzfb_enable(info);
		} else {
			jzfb_disable(info);
		}
		break;
	case JZFB_SET_FG_SIZE:
		if (copy_from_user
		    (&osd.fg_size, argp, sizeof(struct jzfb_fg_size))) {
			dev_info(info->dev, "copy FG size from user failed\n");
			return -EFAULT;
		} else {
			if (!mode) {
				dev_err(jzfb->dev, "Video mode is NULL\n");
				return -EINVAL;
			}
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
		if (copy_from_user
		    (&osd.fg_size, argp, sizeof(struct jzfb_fg_size))) {
			dev_info(info->dev, "copy FG size from user failed\n");
			return -EFAULT;
		}

		if (!osd.fg_size.fg) {
			value = reg_read(jzfb, LCDC_SIZE0);
		} else {
			value = reg_read(jzfb, LCDC_SIZE1);
		}
		osd.fg_size.w = value & LCDC_SIZE_WIDTH_MASK;
		osd.fg_size.h =
		    (value & LCDC_SIZE_HEIGHT_MASK) >> LCDC_SIZE_HEIGHT_BIT;
		if (copy_to_user
		    (argp, &osd.fg_size, sizeof(struct jzfb_fg_size))) {
			dev_info(info->dev, "copy FG size to user failed\n");
			return -EFAULT;
		}
		break;
	case JZFB_SET_FG_POS:
		if (copy_from_user
		    (&osd.fg_pos, argp, sizeof(struct jzfb_fg_pos))) {
			dev_info(info->dev, "copy FG pos from user failed\n");
			return -EFAULT;
		} else {
			return jzfb_set_foreground_position(info, &osd.fg_pos);
		}
		break;
	case JZFB_GET_FG_POS:
		if (copy_from_user
		    (&osd.fg_pos, argp, sizeof(struct jzfb_fg_pos))) {
			dev_info(info->dev, "copy FG pos from user failed\n");
			return -EFAULT;
		}
		if (!osd.fg_size.fg) {
			value = reg_read(jzfb, LCDC_XYP0);
		} else {
			value = reg_read(jzfb, LCDC_XYP1);
		}
		osd.fg_pos.x = value & LCDC_XYP_XPOS_MASK;
		osd.fg_pos.y =
		    (value & LCDC_XYP_YPOS_MASK) >> LCDC_XYP_YPOS_BIT;
		if (copy_to_user(argp, &osd.fg_pos, sizeof(struct jzfb_fg_pos))) {
			dev_info(info->dev, "copy FG pos to user failed\n");
			return -EFAULT;
		}
		break;
	case JZFB_GET_BUFFER:
		if (copy_to_user(argp, &jzfb->current_buffer, sizeof(int))) {
			dev_info(info->dev, "user get current buffer failed\n");
			return -EFAULT;
		}
		break;
	case JZFB_SET_ALPHA:
		if (copy_from_user
		    (&osd.fg_alpha, argp, sizeof(struct jzfb_fg_alpha))) {
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

			/* is lcdc already in earlysuspend, make trigger a vsync. */
			if (jzfb->is_suspend) {
				dev_err(jzfb->dev,
					"*** JZFB_SET_VSYNCINT but jzfb->is_suspend, trigger a dummy vsync end envent. ***\n");
				jzfb->vsync_timestamp = ktime_get();
				wmb();
				wake_up_interruptible(&jzfb->vsync_wq);
			}

		} else {
			tmp = reg_read(jzfb, LCDC_CTRL);
			reg_write(jzfb, LCDC_CTRL, tmp & ~LCDC_CTRL_EOFM);
		}
		break;
	case JZFB_SET_BACKGROUND:
		if (copy_from_user
		    (&osd.background, argp, sizeof(struct jzfb_bg))) {
			dev_info(info->dev, "copy colorkey from user failed\n");
			return -EFAULT;
		} else {
			jzfb_set_background(info, &osd.background);
		}
		break;
	case JZFB_SET_COLORKEY:
		if (copy_from_user
		    (&osd.color_key, argp, sizeof(struct jzfb_color_key))) {
			dev_info(info->dev, "copy colorkey from user failed\n");
			return -EFAULT;
		}
		jzfb_set_colorkey(info, &osd.color_key);
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
	case JZFB_ENABLE_FG0:
		if (copy_from_user(&value, argp, sizeof(int)))
			return -EFAULT;
		for (i = 0; i < jzfb->desc_num - 1; i++) {
			if (value) {
				jzfb->framedesc[i]->cmd |= LCDC_CMD_FRM_EN;
			} else {
				jzfb->framedesc[i]->cmd &= ~LCDC_CMD_FRM_EN;
			}
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

static int jzfb_mmap(struct fb_info *info, struct vm_area_struct *vma)
{
	struct jzfb *jzfb = info->par;
	unsigned long start;
	unsigned long off;
	u32 len;

	off = vma->vm_pgoff << PAGE_SHIFT;

	/* frame buffer memory */
	start = jzfb->fb->fix.smem_start;
	len = PAGE_ALIGN((start & ~PAGE_MASK) + jzfb->fb->fix.smem_len);
	start &= PAGE_MASK;

	if ((vma->vm_end - vma->vm_start + off) > len)
		return -EINVAL;
	off += start;

	vma->vm_pgoff = off >> PAGE_SHIFT;
	vma->vm_flags |= VM_IO;

	pgprot_val(vma->vm_page_prot) &= ~_CACHE_MASK;
	/* Write-Acceleration */
	pgprot_val(vma->vm_page_prot) |= _CACHE_CACHABLE_WA;

	if (io_remap_pfn_range(vma, vma->vm_start, off >> PAGE_SHIFT,
			       vma->vm_end - vma->vm_start, vma->vm_page_prot))
	{
		return -EAGAIN;
	}

	return 0;
}

static int
jzfb_vsync_timestamp_changed(struct jzfb *jzfb, ktime_t prev_timestamp)
{
	rmb();
	return !ktime_equal(prev_timestamp, jzfb->vsync_timestamp);
}

static int jzfb_wait_for_vsync_thread(void *data)
{
	struct jzfb *jzfb = (struct jzfb *)data;

	while (!kthread_should_stop()) {
		ktime_t prev_timestamp = jzfb->vsync_timestamp;
		int ret = wait_event_interruptible_timeout(jzfb->vsync_wq,
				jzfb_vsync_timestamp_changed(jzfb, prev_timestamp), msecs_to_jiffies(100));
		if (ret > 0) {
			char *envp[2];
			char buf[64];
			int is_suspend;

			mutex_lock(&jzfb->lock);
			is_suspend = jzfb->is_suspend;
			/* rotate right */
			jzfb->vsync_skip_map = (jzfb->vsync_skip_map >> 1 | jzfb->vsync_skip_map << 9) & 0x3ff;
			mutex_unlock(&jzfb->lock);
			if (!is_suspend) {
				if (!(jzfb->vsync_skip_map & 0x1))
					continue;
			}
			snprintf(buf, sizeof(buf), "VSYNC=%llu",
					ktime_to_ns(jzfb->vsync_timestamp));
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

	if (state & LCDC_STATE_EOF) {
		reg_write(jzfb, LCDC_STATE, state & ~LCDC_STATE_EOF);
		jzfb->vsync_timestamp = ktime_get();
		wmb();
		wake_up_interruptible(&jzfb->vsync_wq);
	}

	if (state & LCDC_STATE_OFU) {
		reg_write(jzfb, LCDC_STATE, state & ~LCDC_STATE_OFU);
		if (jzfb->irq_cnt++ > 100) {
			unsigned int tmp;
			jzfb->irq_cnt = 0;
			tmp = reg_read(jzfb, LCDC_CTRL);
			reg_write(jzfb, LCDC_CTRL, tmp & ~LCDC_CTRL_OFUM);
			dev_err(jzfb->dev, "disable OFU irq\n");
		}
		/* dev_err(jzfb->dev, "%s, Out FiFo underrun\n", __func__); */
	}
	return IRQ_HANDLED;
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
	.fb_mmap = jzfb_mmap,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
void dump_cpm_reg(void)
{
	printk("----reg:0x10000020 value=0x%08x  (24bit) Clock Gate Register0\n",
			*(unsigned int *)0xb0000020);
	printk("----reg:0x100000e4 value=0x%08x  (5bit_lcdc 21bit_lcdcs) Power Gate Register: \n",
			*(unsigned int *)0xb00000e4);
	printk("----reg:0x100000b8 value=0x%08x  (10bit) SRAM Power Control Register0 \n",
			*(unsigned int *)0xb00000b8);
	printk("----reg:0x10000064 value=0x%08x  Lcd pixclock \n",
			*(unsigned int *)0xb0000064);
}

static void jzfb_early_suspend(struct early_suspend *h)
{
	struct jzfb *jzfb = container_of(h, struct jzfb, early_suspend);
	mutex_lock(&jzfb->lock);
	if (jzfb->pdata->alloc_vidmem) {
		/* set suspend state and notify panel, backlight client */
		fb_blank(jzfb->fb, FB_BLANK_POWERDOWN);
		fb_set_suspend(jzfb->fb, 1);

	} else {
		/* disable LCDC */
		jzfb_blank(FB_BLANK_POWERDOWN, jzfb->fb);
	}
	mutex_lock(&jzfb->suspend_lock);
	jzfb->is_suspend = 1;
	mutex_unlock(&jzfb->suspend_lock);
	mutex_unlock(&jzfb->lock);

	/*disable clock*/
	jzfb_clk_disable(jzfb);
	clk_disable(jzfb->pclk);
	clk_disable(jzfb->pwcl);
#if 0
	printk("----lcd early suspend:\n");
	dump_cpm_reg()
#endif
}

static void jzfb_late_resume(struct early_suspend *h)
{
	/*enable clock*/
	struct jzfb *jzfb = container_of(h, struct jzfb, early_suspend);
	clk_enable(jzfb->pwcl);
	jzfb_clk_enable(jzfb);
	clk_enable(jzfb->pclk);
	jzfb_set_par(jzfb->fb);

	if (jzfb->pdata->alloc_vidmem) {
		fb_set_suspend(jzfb->fb, 0);
		fb_blank(jzfb->fb, FB_BLANK_UNBLANK);
	} else {
		jzfb_blank(FB_BLANK_UNBLANK, jzfb->fb);
	}
	mutex_lock(&jzfb->suspend_lock);
	jzfb->is_suspend = 0;
	mutex_unlock(&jzfb->suspend_lock);

#if 0
	printk("----lcd early resume:\n");
	dump_cpm_reg()
#endif
}
#endif

static void jzfb_change_dma_desc(struct fb_info *info)
{
	struct jzfb *jzfb = info->par;
	struct fb_videomode *mode;
	int is_pclk_en;
#if 0
	if (!jzfb->is_lcd_en) {
		dev_err(jzfb->dev, "LCDC isn't enabled\n");
		return;
	}
#endif

	mode = jzfb->pdata->modes;
	if (!mode)
		return;
	jzfb->osd.fg0.fg = 0;
	jzfb->osd.fg0.bpp = jzfb_get_controller_bpp(jzfb) == 32 ? 32 : 16;
	jzfb->osd.fg0.x = jzfb->osd.fg0.y = 0;
	jzfb->osd.fg0.w = mode->xres;
	jzfb->osd.fg0.h = mode->yres;

	info->mode = mode;
	jzfb_prepare_dma_desc(info);

	if (mode->pixclock) {
		unsigned long rate = PICOS2KHZ(mode->pixclock) * 1000;
		is_pclk_en = clk_is_enabled(jzfb->pclk);
		if(is_pclk_en)
			clk_disable(jzfb->pclk);
		clk_set_rate(jzfb->pclk, rate);
		clk_enable(jzfb->pclk);
		dev_info(jzfb->dev, "LCDC: PixClock = %lu\n", rate);
		dev_info(jzfb->dev, "LCDC: PixClock = %lu(real)\n",
			 clk_get_rate(jzfb->pclk));
	} else {
		dev_err(jzfb->dev, "Video mode pixclock invalid\n");
	}

	jzfb_config_image_enh(info);

#ifndef CONFIG_SLCDC_CONTINUA
	if (jzfb->pdata->lcd_type == LCD_TYPE_LCM) {
		/* update display */
		unsigned long tmp;
		tmp = reg_read(jzfb, SLCDC_CTRL);
		tmp |= SLCDC_CTRL_DMA_MODE | SLCDC_CTRL_DMA_START;
		reg_write(jzfb, SLCDC_CTRL, tmp);
	}
#endif
}

static int jzfb_copy_logo(struct fb_info *info)
{
	unsigned long src_addr = 0;	/* u-boot logo buffer address */
	unsigned long dst_addr = 0;	/* kernel frame buffer address */
	unsigned long size;
	unsigned long offsize;
	int i = 0, j = 0;
	struct jzfb *jzfb = info->par;

	return -1;

	/* get buffer physical address */
	src_addr = (unsigned long)reg_read(jzfb, LCDC_SA0);
	if (!(reg_read(jzfb, LCDC_CTRL) & LCDC_CTRL_ENA)) {
		/* u-boot is not display logo */
		return -ENOMEM;
	}

	/* jzfb->is_lcd_en = 1; */

	if (src_addr) {
		src_addr = (unsigned long)phys_to_virt(src_addr);
		if (ALIGN(info->var.xres, PIXEL_ALIGN) == info->var.xres) {
			size = info->fix.line_length * info->var.yres;

			for (i = 0; i < NUM_FRAME_BUFFERS; i++) {
				dst_addr = (unsigned long)info->screen_base + i * size;
				memcpy((void *)dst_addr, (void *)src_addr, size);
			}
		} else {
			size = info->var.bits_per_pixel * info->var.xres >> 3;
			offsize = info->var.bits_per_pixel * (ALIGN(info->var.xres, PIXEL_ALIGN) - info->var.xres) >> 3;

			dst_addr = (unsigned long)info->screen_base;
			for (i = 0; i < NUM_FRAME_BUFFERS; i++) {
				unsigned long temp_src_addr = src_addr;
				for (j = 0; j < info->var.yres; j++) {
					memcpy((void *)dst_addr, (void *)temp_src_addr, size);
					dst_addr += (size + offsize);
					temp_src_addr += size;
				}
			}
		}
	}

	return 0;
}

static void jzfb_display_v_color_bar(struct fb_info *info)
{
	int i, j;
	int w, h;
	int bpp;
	unsigned short *p16;
	unsigned int *p32;
	struct jzfb *jzfb = info->par;
	struct fb_videomode *mode = jzfb->pdata->modes;

	if (!mode) {
		dev_err(jzfb->dev, "%s, video mode is NULL\n", __func__);
		return;
	}
	if (!jzfb->vidmem_phys) {
		dev_err(jzfb->dev, "Not allocate frame buffer yet\n");
		return;
	}
	if (!jzfb->vidmem)
		jzfb->vidmem = (void *)phys_to_virt(jzfb->vidmem_phys);
	p16 = (unsigned short *)jzfb->vidmem;
	p32 = (unsigned int *)jzfb->vidmem;
	w = jzfb->osd.fg0.w;
	h = jzfb->osd.fg0.h;
	bpp = jzfb->osd.fg0.bpp;

	dev_info(info->dev,
		 "LCD V COLOR BAR w,h,bpp(%d,%d,%d) jzfb->vidmem=%p\n", w, h,
		 bpp, jzfb->vidmem);

	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++) {
			short c16;
			int c32 = 0;
			switch ((j / 10) % 4) {
			case 0:
				c16 = 0xF800;
				c32 = 0xFFFF0000;
				break;
			case 1:
				c16 = 0x07C0;
				c32 = 0xFF00FF00;
				break;
			case 2:
				c16 = 0x001F;
				c32 = 0xFF0000FF;
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
				*p16++ = c16;
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
				p16 += (ALIGN(mode->xres, PIXEL_ALIGN) - w);
				break;
			}
		}
	}
}

static void jzfb_display_h_color_bar(struct fb_info *info)
{
	int i, j;
	int w, h;
	int bpp;
	unsigned short *p16;
	unsigned int *p32;
	struct jzfb *jzfb = info->par;
	struct fb_videomode *mode = jzfb->pdata->modes;

	if (!mode) {
		dev_err(jzfb->dev, "%s, video mode is NULL\n", __func__);
		return;
	}
	if (!jzfb->vidmem_phys) {
		dev_err(jzfb->dev, "Not allocate frame buffer yet\n");
		return;
	}
	if (!jzfb->vidmem)
		jzfb->vidmem = (void *)phys_to_virt(jzfb->vidmem_phys);
	p16 = (unsigned short *)jzfb->vidmem;
	p32 = (unsigned int *)jzfb->vidmem;
	w = jzfb->osd.fg0.w;
	h = jzfb->osd.fg0.h;
	bpp = jzfb->osd.fg0.bpp;

	dev_info(info->dev,
		 "LCD H COLOR BAR w,h,bpp(%d,%d,%d), jzfb->vidmem=%p\n", w, h,
		 bpp, jzfb->vidmem);

	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++) {
			short c16;
			int c32;
			switch ((i / 10) % 4) {
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
				*p16++ = c16;
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
				p16 += (ALIGN(mode->xres, PIXEL_ALIGN) - w);
				break;
			}
		}
	}
}

static void dump_lcdc_registers(struct jzfb *jzfb)
{
	int i,is_clk_en;
	long unsigned int tmp;
	struct device *dev = jzfb->dev;

	is_clk_en = jzfb->is_clk_en;
	jzfb_clk_enable(jzfb);

	/* LCD Controller Resgisters */
	dev_info(dev, "jzfb->base: \t0x%08x\n", (unsigned int)(jzfb->base));

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
	tmp = reg_read(jzfb, LCDC_VAT);
	dev_info(dev, "LCDC_VAT: \t0x%08lx, HT = %ld, VT = %ld\n", tmp,
		 (tmp & LCDC_VAT_HT_MASK) >> LCDC_VAT_HT_BIT,
		 (tmp & LCDC_VAT_VT_MASK) >> LCDC_VAT_VT_BIT);
	tmp = reg_read(jzfb, LCDC_DAH);
	dev_info(dev, "LCDC_DAH: \t0x%08lx, HDS = %ld, HDE = %ld\n", tmp,
		 (tmp & LCDC_DAH_HDS_MASK) >> LCDC_DAH_HDS_BIT,
		 (tmp & LCDC_DAH_HDE_MASK) >> LCDC_DAH_HDE_BIT);
	tmp = reg_read(jzfb, LCDC_DAV);
	dev_info(dev, "LCDC_DAV: \t0x%08lx, VDS = %ld, VDE = %ld\n", tmp,
		 (tmp & LCDC_DAV_VDS_MASK) >> LCDC_DAV_VDS_BIT,
		 (tmp & LCDC_DAV_VDE_MASK) >> LCDC_DAV_VDE_BIT);
	tmp = reg_read(jzfb, LCDC_HSYNC);
	dev_info(dev, "LCDC_HSYNC:\t0x%08lx, HPS = %ld, HPE = %ld\n", tmp,
		 (tmp & LCDC_HSYNC_HPS_MASK) >> LCDC_HSYNC_HPS_BIT,
		 (tmp & LCDC_HSYNC_HPE_MASK) >> LCDC_HSYNC_HPE_BIT);
	tmp = reg_read(jzfb, LCDC_VSYNC);
	dev_info(dev, "LCDC_VSYNC:\t0x%08lx, VPS = %ld, VPE = %ld\n", tmp,
		 (tmp & LCDC_VSYNC_VPS_MASK) >> LCDC_VSYNC_VPS_BIT,
		 (tmp & LCDC_VSYNC_VPE_MASK) >> LCDC_VSYNC_VPE_BIT);
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
	dev_info(dev, "==================================\n");
	dev_info(dev, "SLCDC_CFG: \t0x%08lx\n", reg_read(jzfb, SLCDC_CFG));
	dev_info(dev, "SLCDC_CTRL: \t0x%08lx\n", reg_read(jzfb, SLCDC_CTRL));
	dev_info(dev, "SLCDC_STATE: \t0x%08lx\n", reg_read(jzfb, SLCDC_STATE));
	dev_info(dev, "SLCDC_DATA: \t0x%08lx\n", reg_read(jzfb, SLCDC_DATA));
	dev_info(dev, "SLCDC_CFG_NEW: \t0x%08lx\n",
		 reg_read(jzfb, SLCDC_CFG_NEW));
	dev_info(dev, "SLCDC_WTIME: \t0x%08lx\n", reg_read(jzfb, SLCDC_WTIME));
	dev_info(dev, "SLCDC_TAS: \t0x%08lx\n", reg_read(jzfb, SLCDC_TAS));
	dev_info(dev, "==================================\n");
	for (i = 0; i < jzfb->desc_num; i++) {
		if (!jzfb->framedesc[i])
			break;
		dev_info(dev, "==================================\n");
		if (i != jzfb->desc_num - 1) {
			dev_info(dev, "jzfb->framedesc[%d]: %p\n", i,
				 jzfb->framedesc[i]);
			dev_info(dev, "DMA 0 descriptor value in memory\n");
		} else {
			dev_info(dev, "jzfb->fg1_framedesc: %p\n",
				 jzfb->framedesc[i]);
			dev_info(dev, "DMA 1 descriptor value in memory\n");
		}
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
	if (!is_clk_en)
		jzfb_clk_disable(jzfb);

	return;
}

static ssize_t
dump_lcd(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jzfb *jzfb = dev_get_drvdata(dev);
	dump_lcdc_registers(jzfb);

	return 0;
}

static ssize_t
dump_h_color_bar(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jzfb *jzfb = dev_get_drvdata(dev);
	jzfb_display_h_color_bar(jzfb->fb);
	return 0;
}

static ssize_t
dump_v_color_bar(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jzfb *jzfb = dev_get_drvdata(dev);
	jzfb_display_v_color_bar(jzfb->fb);
	return 0;
}

static ssize_t
vsync_skip_r(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct jzfb *jzfb = dev_get_drvdata(dev);
	mutex_lock(&jzfb->lock);
	snprintf(buf, 3, "%d\n", jzfb->vsync_skip_ratio);
	dev_info(dev, "vsync_skip_map = 0x%08x\n", jzfb->vsync_skip_map);
	mutex_unlock(&jzfb->lock);
	return 3;		/* sizeof ("%d\n") */
}

static int vsync_skip_set(struct jzfb *jzfb, int vsync_skip)
{
	unsigned int map_wide10 = 0;
	int rate, i, p, n;
	int fake_float_1k;

	if (vsync_skip < 0 || vsync_skip > 9)
		return -EINVAL;

	rate = vsync_skip + 1;
	fake_float_1k = 10000 / rate;	/* 10.0 / rate */

	p = 1;
	n = (fake_float_1k * p + 500) / 1000;	/* +0.5 to int */

	for (i = 1; i <= 10; i++) {
		map_wide10 = map_wide10 << 1;
		if (i == n) {
			map_wide10++;
			p++;
			n = (fake_float_1k * p + 500) / 1000;
		}
	}
	mutex_lock(&jzfb->lock);
	jzfb->vsync_skip_map = map_wide10;
	jzfb->vsync_skip_ratio = rate - 1;	/* 0 ~ 9 */
	mutex_unlock(&jzfb->lock);

	return 0;
}

static ssize_t
vsync_skip_w(struct device *dev, struct device_attribute *attr,
	     const char *buf, size_t count)
{
	struct jzfb *jzfb = dev_get_drvdata(dev);

	if ((count != 1) && (count != 2))
		return -EINVAL;
	if ((*buf < '0') && (*buf > '9'))
		return -EINVAL;

	vsync_skip_set(jzfb, *buf - '0');

	return count;
}

static ssize_t fps_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	printk("\n-----you can choice print way:\n");
	printk("Example: echo NUM > show_fps\n");
	printk("NUM = 0: close fps statistics\n");
	printk("NUM = 1: print recently fps\n");
	printk("NUM = 2: print interval between last and this pan_display\n");
	printk("NUM = 3: print pan_display count\n\n");
	return 0;
}

static ssize_t fps_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t n)
{
	int num = 0;
	num = simple_strtoul(buf, NULL, 0);
	if(num < 0 || num > 3){
		printk("\n--please 'cat show_fps' to view using the method\n\n");
		return n;
	}
	showFPS = num;
	if(showFPS == 3)
		printk(KERN_DEBUG " Pand display count=%d\n",jzfb->pan_display_count);
	return n;
}
/**********************lcd_debug***************************/
static DEVICE_ATTR(dump_lcd, S_IRUGO|S_IWUSR, dump_lcd, NULL);
static DEVICE_ATTR(dump_h_color_bar, S_IRUGO|S_IWUSR, dump_h_color_bar, NULL);
static DEVICE_ATTR(dump_v_color_bar, S_IRUGO|S_IWUSR, dump_v_color_bar, NULL);
static DEVICE_ATTR(vsync_skip, S_IRUGO|S_IWUSR, vsync_skip_r, vsync_skip_w);
static DEVICE_ATTR(show_fps, S_IRUGO|S_IWUSR, fps_show, fps_store);

static struct attribute *lcd_debug_attrs[] = {
	&dev_attr_dump_lcd.attr,
	&dev_attr_dump_h_color_bar.attr,
	&dev_attr_dump_v_color_bar.attr,
	&dev_attr_vsync_skip.attr,
	&dev_attr_show_fps.attr,
	NULL,
};

const char lcd_group_name[] = "debug";
static struct attribute_group lcd_debug_attr_group = {
	.name	= lcd_group_name,
	.attrs	= lcd_debug_attrs,
};


void test_pattern(struct jzfb *jzfb)
{
	int count = 5;
	int next_frm = 0;

	jzfb_set_par(jzfb->fb);
	jzfb_display_v_color_bar(jzfb->fb);
	jzfb_enable(jzfb->fb);

	dump_lcdc_registers(jzfb);
#ifdef CONFIG_JZ_MIPI_DSI
	dump_dsi_reg(jzfb->dsi);
#endif
	while(count--){
		if(next_frm){
			next_frm = 0;
			jzfb_display_v_color_bar(jzfb->fb);
		}
		else{
			next_frm = 1;
			jzfb_display_h_color_bar(jzfb->fb);
		}
		if (jzfb->pdata->lcd_type == LCD_TYPE_LCM) {
#ifndef CONFIG_SLCDC_CONTINUA
			int smart_ctrl = 0;
			smart_ctrl = reg_read(jzfb, SLCDC_CTRL);
			smart_ctrl |= SLCDC_CTRL_DMA_START; //trigger a new frame
			reg_write(jzfb, SLCDC_CTRL, smart_ctrl);
#endif
		}
		mdelay(100);
	}
}
static int __devinit jzfb_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct fb_info *fb;
	struct jzfb_platform_data *pdata = pdev->dev.platform_data;
	struct fb_videomode *video_mode;
	struct resource *mem;

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
		dev_err(&pdev->dev,
			"Failed to request register memory region\n");
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
	jzfb->mem = mem;
#ifdef CONFIG_JZ_MIPI_DSI
	jzfb->jz_dsi_driver = &jz_dsi_driver;
	jzfb->jz_dsi_device = &jz_dsi_device;
	platform_device_register(jzfb->jz_dsi_device);
	if(jzfb->jz_dsi_driver->probe)
		jzfb->jz_dsi_driver->probe(jzfb->jz_dsi_device);
	jzfb->dsi = container_of(pdata->dsi_pdata->dsi_state, struct dsi_device, state);
	if (!jzfb->dsi) {
		goto err_get_dsi;
	}
#endif

	if (pdata->lcd_type != LCD_TYPE_INTERLACED_TV &&
	    pdata->lcd_type != LCD_TYPE_LCM) {
		jzfb->desc_num = MAX_DESC_NUM - 2;
	} else {
		jzfb->desc_num = MAX_DESC_NUM;
	}

	mutex_init(&jzfb->lock);
	mutex_init(&jzfb->suspend_lock);
	sprintf(jzfb->clk_name, "lcd");
	sprintf(jzfb->pclk_name, "cgu_lpc");
	sprintf(jzfb->pwcl_name, "pwc_lcd");
	jzfb->clk = clk_get(&pdev->dev, jzfb->clk_name);
	jzfb->pclk = clk_get(&pdev->dev, jzfb->pclk_name);
	jzfb->pwcl = clk_get(&pdev->dev, jzfb->pwcl_name);

	if (IS_ERR(jzfb->clk) || IS_ERR(jzfb->pclk) || IS_ERR(jzfb->pwcl)) {
		ret = PTR_ERR(jzfb->clk);
		dev_err(&pdev->dev, "Failed to get lcdc clock: %d\n", ret);
		goto err_framebuffer_release;
	}
	/* Don't read or write lcdc registers until here. */
	clk_enable(jzfb->pwcl);
	jzfb_clk_enable(jzfb);

	if (!jzfb->pdata->alloc_vidmem) {
		clk_set_rate(jzfb->pclk, 27000000);
		clk_enable(jzfb->pclk);
	}
	jzfb->base = ioremap(mem->start, resource_size(mem));
	if (!jzfb->base) {
		dev_err(&pdev->dev,
			"Failed to ioremap register memory region\n");
		ret = -EBUSY;
		goto err_put_clk;
	}

	platform_set_drvdata(pdev, jzfb);

	fb_videomode_to_modelist(pdata->modes, pdata->num_modes, &fb->modelist);
	video_mode = jzfb->pdata->modes;
	if (!video_mode)
		goto err_iounmap;
	jzfb_videomode_to_var(&fb->var, video_mode, jzfb->pdata->lcd_type);
	fb->var.width = pdata->width;
	fb->var.height = pdata->height;
	fb->var.bits_per_pixel = pdata->bpp;

        /* Android generic FrameBuffer format is A8B8G8R8(B3B2B1B0), so we set A8B8G8R8 as default.
         *
         * If set rgb order as A8B8G8R8, both SLCD cmd_buffer and data_buffer bytes sequence changed.
         * so remain slcd format X8R8G8B8, until fix this problem.(<lgwang@ingenic.cn>, 2014-06-20)
         */
#ifdef CONFIG_ANDROID
        if (pdata->lcd_type == LCD_TYPE_LCM) {
            jzfb->fmt_order = FORMAT_X8R8G8B8;
        }
        else {
            jzfb->fmt_order = FORMAT_X8B8G8R8;
        }
#else
        jzfb->fmt_order = FORMAT_X8R8G8B8;
#endif

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
	fb->fix.smem_len = jzfb->vidmem_size;
	fb->screen_base = jzfb->vidmem;
	fb->pseudo_palette = (void *)(fb + 1);
	jzfb->irq = platform_get_irq(pdev, 0);
	sprintf(jzfb->irq_name, "lcdc%d", pdev->id);
	if (request_irq(jzfb->irq, jzfb_irq_handler, IRQF_DISABLED,
			jzfb->irq_name, jzfb)) {
		dev_err(&pdev->dev, "request irq failed\n");
		ret = -EINVAL;
		goto err_free_devmem;
	}
#ifdef CONFIG_HAS_EARLYSUSPEND
	jzfb->early_suspend.suspend = jzfb_early_suspend;
	jzfb->early_suspend.resume = jzfb_late_resume;
	jzfb->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&jzfb->early_suspend);
#endif

	ret = sysfs_create_group(&jzfb->dev->kobj, &lcd_debug_attr_group);
	if (ret) {
		dev_err(&pdev->dev, "device create sysfs group failed\n");

		ret = -EINVAL;
		goto err_free_irq;
	}

	vsync_skip_set(jzfb, CONFIG_FB_VSYNC_SKIP);

	init_waitqueue_head(&jzfb->vsync_wq);
	jzfb->vsync_thread = kthread_run(jzfb_wait_for_vsync_thread,
					 jzfb, "jzfb-vsync");
	if (jzfb->vsync_thread == ERR_PTR(-ENOMEM)) {
		dev_err(&pdev->dev, "Failed to run vsync thread");
		goto err_free_file;
	}

	ret = register_framebuffer(fb);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register framebuffer: %d\n",
			ret);
		goto err_kthread_stop;
	}

	if (jzfb->vidmem_phys) {
#ifdef CONFIG_FB_JZ_DEBUG
		test_pattern(jzfb);
#endif
		if (!jzfb_copy_logo(jzfb->fb)) {
			jzfb_change_dma_desc(jzfb->fb);
		}
	}else{
		clk_disable(jzfb->pclk);
		jzfb_clk_disable(jzfb);
		clk_disable(jzfb->pwcl);
	}
	return 0;

err_kthread_stop:
	kthread_stop(jzfb->vsync_thread);
err_free_file:
	sysfs_remove_group(&jzfb->dev->kobj, &lcd_debug_attr_group);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&jzfb->early_suspend);
#endif
err_free_irq:
	free_irq(jzfb->irq, jzfb);
err_free_devmem:
	jzfb_free_devmem(jzfb);
err_iounmap:
	iounmap(jzfb->base);
err_put_clk:
	if (jzfb->clk)
		clk_put(jzfb->clk);
	if (jzfb->pclk)
		clk_put(jzfb->pclk);
	if (jzfb->pwcl)
		clk_put(jzfb->pwcl);
err_framebuffer_release:
	framebuffer_release(fb);
#ifdef CONFIG_JZ_MIPI_DSI
err_get_dsi:
	dev_err(&pdev->dev, "get dsi device error\n");
#endif
err_release_mem_region:
	release_mem_region(mem->start, resource_size(mem));
	return ret;
}

static int __devexit jzfb_remove(struct platform_device *pdev)
{
	struct jzfb *jzfb = platform_get_drvdata(pdev);

	kthread_stop(jzfb->vsync_thread);
	jzfb_free_devmem(jzfb);
	platform_set_drvdata(pdev, NULL);

	clk_put(jzfb->pclk);
	clk_put(jzfb->clk);
	clk_put(jzfb->pwcl);

	sysfs_remove_group(&jzfb->dev->kobj, &lcd_debug_attr_group);

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
	int is_fb_blank;
	mutex_lock(&jzfb->suspend_lock);
	is_fb_blank = (jzfb->is_suspend != 1);
	jzfb->is_suspend = 1;
	mutex_unlock(&jzfb->suspend_lock);
	if (is_fb_blank)
		fb_blank(jzfb->fb, FB_BLANK_POWERDOWN);
};

#ifdef CONFIG_PM
static int jzfb_suspend(struct device *dev)
{
	/* struct platform_device *pdev = to_platform_device(dev); */
	/* struct jzfb *jzfb = platform_get_drvdata(pdev); */
	/* clk_disable(jzfb->clk); */
	/* clk_disable(jzfb->pclk); */
	printk("++++++%s\n",__func__);

	return 0;
}

static int jzfb_resume(struct device *dev)
{
	/* struct platform_device *pdev = to_platform_device(dev); */
	/* struct jzfb *jzfb = platform_get_drvdata(pdev); */
	/* clk_enable(jzfb->pclk); */
	/* jzfb_clk_enable(jzfb); */
	printk("++++++%s\n",__func__);

	return 0;
}

static const struct dev_pm_ops jzfb_pm_ops = {
	.suspend = jzfb_suspend,
	.resume = jzfb_resume,
};
#endif
static struct platform_driver jzfb_driver = {
	.probe = jzfb_probe,
	.remove = jzfb_remove,
	.shutdown = jzfb_shutdown,
	.driver = {
		   .name = "jz-fb",
#ifdef CONFIG_PM
		   .pm = &jzfb_pm_ops,
#endif

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

#ifdef CONFIG_EARLY_INIT_RUN
rootfs_initcall(jzfb_init);
#else
module_init(jzfb_init);
#endif

module_exit(jzfb_cleanup);

MODULE_DESCRIPTION("JZ LCD Controller driver");
MODULE_AUTHOR("Sean Tang <ctang@ingenic.cn>");
MODULE_LICENSE("GPL");
