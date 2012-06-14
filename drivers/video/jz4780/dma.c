/* kernel/drivers/video/jz4780/input/tft_dma.c
 *
 * Copyright (c) 2012 Ingenic Semiconductor Co., Ltd.
 *              http://www.ingenic.com/
 *
 * Input file for Ingenic Display Controller driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/fb.h>
#include <asm/cacheflush.h>

#include "regs.h"
#include "jz4780_fb.h"

/*
* Normal TFT panel's DMA Chan0:
*	TO LCD Panel:
* 		no palette:	dma0_desc0 <<==>> dma0_desc0
* 		palette :	dma0_desc_palette <<==>> dma0_desc0
*	TO TV Encoder:
* 		no palette:	dma0_desc0 <<==>> dma0_desc1
* 		palette:	dma0_desc_palette --> dma0_desc0
* 				--> dma0_desc1 --> dma0_desc_palette --> ...
* DMA Chan1:
*	TO LCD Panel:
* 		dma1_desc0 <<==>> dma1_desc0
*	TO TV Encoder:
* 		dma1_desc0 <<==>> dma1_desc1
*/
int prepare_dma_descriptor(struct jzfb *jzfb)
{
	int size0;
	int fg0_line_size, fg0_frm_size;
	int panel_line_size, panel_frm_size;
	struct jzfb_framedesc *dma0_desc0;
	struct fb_info *info = jzfb->fb;
	struct fb_videomode *mode = info->mode;

	/* alloc dma descriptor room */
	if (!jzfb->framedesc[0]) {
		jzfb->framedesc[0] = 
			(struct jzfb_framedesc *)kzalloc(sizeof(struct jzfb_framedesc), GFP_KERNEL);
	}

	dma0_desc0 = jzfb->framedesc[0] + 0;

	/* Foreground 0, caculate size */
	if (jzfb->osd.fg0.x >= mode->xres)
		jzfb->osd.fg0.x = mode->xres - 1;
	if (jzfb->osd.fg0.y >= mode->yres)
		jzfb->osd.fg0.y = mode->yres - 1;

	size0 = jzfb->osd.fg0.h << 16 | jzfb->osd.fg0.w;

	/* lcd display area */
	fg0_line_size = jzfb->osd.fg0.w * jzfb->osd.fg0.bpp / 8;
	fg0_line_size = ((fg0_line_size + 3) >> 2) << 2; /* word aligned */
	fg0_frm_size= fg0_line_size * jzfb->osd.fg0.h;

	/* panel PIXEL_ALIGN stride buffer area */
	panel_line_size = ((mode->xres + (PIXEL_ALIGN - 1)) & (~(PIXEL_ALIGN - 1)))
					 * (jzfb->osd.fg0.bpp / 8);
	panel_line_size = ((panel_line_size + 3) >> 2) << 2; /* word aligned */
	panel_frm_size = panel_line_size * mode->yres;

	/* next */
	dma0_desc0->next_desc = (unsigned int)virt_to_phys(dma0_desc0);
	/* frame id */
	dma0_desc0->frame_id = (unsigned int)0x0000da00; /* DMA0'0 */
	/* frame phys addr */
	dma0_desc0->databuf = virt_to_phys((void *)info->screen_base);
	/* others */
	dma0_desc0->cmd = LCDC_CMD_EOFINT | fg0_frm_size / 4;
	dma0_desc0->offsize = (panel_line_size - fg0_line_size) / 4;
	dma0_desc0->page_width = fg0_line_size / 4;
	dma0_desc0->desc_size = size0;

	reg_write(jzfb, LCDC_DA0, virt_to_phys(dma0_desc0)); /* tft lcd */
	reg_write(jzfb, LCDC_DESSIZE0, size0);

	dma_cache_wback((unsigned int)(dma0_desc0), sizeof(struct jzfb_framedesc));

	return 0;
}

int tft_lcd_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	int dy;
	int fg0_line_size, fg0_frm_size;
	int panel_line_size, panel_frm_size;
	struct jzfb *jzfb = info->par;
	struct jzfb_framedesc *dma0_desc0 = jzfb->framedesc[0];
	struct fb_videomode *mode = info->mode;

	//prt_debug("enter tft_lcd_pan_display\n");

	if (!var || !info) {
		dev_err(info->dev,"NULL value!!!\n");
		return -EINVAL;
	}
	if (!((reg_read(jzfb, LCDC_CTRL)) & LCDC_CTRL_ENA)) {
		dev_err(info->dev,"lcdc%d is disable\n", jzfb->id);
		return 0;
	}
	if (!((reg_read(jzfb, LCDC_OSDC)) & LCDC_OSDC_F0EN)) {
		dev_err(info->dev,"lcdc%d's fg0 is disable\n", jzfb->id);
		return 0;
	}
	if (var->xoffset - info->var.xoffset) {
		/* No support for X panning for now! */
		dev_err(info->dev,"No support for X panning for now!\n");
		return -EINVAL;
	}

	dy = var->yoffset;

	/* panel PIXEL_ALIGN stride buffer area */
	panel_line_size = ((mode->xres + (PIXEL_ALIGN - 1)) &
					 (~(PIXEL_ALIGN - 1))) * (jzfb->osd.fg0.bpp / 8);
	panel_line_size = ((panel_line_size + 3) >> 2) << 2;
	panel_frm_size = panel_line_size * mode->yres;

	/* lcd display area */
	fg0_line_size = jzfb->osd.fg0.w * jzfb->osd.fg0.bpp / 8;
	fg0_line_size = ((fg0_line_size + 3) >> 2) << 2; /* word aligned */
	fg0_frm_size= fg0_line_size * jzfb->osd.fg0.h;

#ifdef CONFIG_JZ47XX_AOSDC
	if (lcdc_should_use_com_decom_mode()) {
		//TODO
	} else {
		jz4780_flush_dcache_with_prefetch_allocate();

		dma0_desc0->cmd = LCDC_CMD_EOFINT | fg0_frm_size / 4;
		dma0_desc0->offsize = (panel_line_size - fg0_line_size) / 4;
		dma0_desc0->page_width = fg0_line_size / 4;

		//prt_debug("dy = %d\n", dy);

		if (dy) {
		    dma0_desc0->databuf = 
				(unsigned int)virt_to_phys((void *)info->screen_base + (info->fix.line_length * dy));
			dma0_desc0->frame_id = 0x82826661;
			jzfb->next_frameID = 0x82826661;
		} else {
			dma0_desc0->databuf = (unsigned int)virt_to_phys((void *)info->screen_base);
			dma0_desc0->frame_id = 0x19491001;
			jzfb->next_frameID = 0x19491001;
		}
		dma_cache_wback((unsigned int)(dma0_desc0), sizeof(struct jzfb_framedesc));
	}
#else
	if (dy) {
		dma0_desc0->databuf = 
			(unsigned int)virt_to_phys((void *)info->screen_base + (info->fix.line_length * dy));
		dma0_desc0->frame_id = 0x82826661;
		jzfb->next_frameID = 0x82826661;
	}
	else {
		dma0_desc0->databuf = (unsigned int)virt_to_phys((void *)info->screen_base);
		dma0_desc0->frame_id = 0x19491001;
		jzfb->next_frameID = 0x19491001;
	}
#endif

	return 0;
}

/*
 * The rules of f0, f1's position:
 * 	f0.x + f0.w <= mode->xres;
 * 	f0.y + f0.h <= mode->yres;
 *
 * When output is LCD panel, fg.y and fg.h can be odd number or even number.
 * When output is TVE, as the TVE has odd frame and even frame,
 * to simplified operation, fg.y and fg.h should be even number always.
 *
 */
int set_tft_lcd_fg0_size(struct jzfb_fg_t *fg0, struct fb_info *info)
{
	int size0, fg0_line_size, fg0_frm_size;
	int panel_line_size, panel_frm_size;
	unsigned long tmp = 0;
	struct jzfb *jzfb = info->par;
	struct jzfb_framedesc *dma0_desc0 = jzfb->framedesc[0];

	struct fb_videomode *mode = info->mode;

	if (fg0->x + fg0->w > mode->xres)
		fg0->w = mode->xres - fg0->x;
	if (fg0->y + fg0->h > mode->yres)
		fg0->h = mode->yres - fg0->y;

	dev_err(info->dev,"fg0->x: %d, fg0->w: %d, mode->xres: %d, fg0->h:%d, mode->yres:%d\n", 
		   fg0->x, fg0->w, mode->xres, fg0->h, mode->yres);

	size0 = fg0->h << 16 | fg0->w;

	if (reg_read(jzfb, LCDC_DESSIZE0) == size0) {
		printk("FG0: same size\n");
		return 0;
	}

    /* panel PIXEL_ALIGN stride buffer area */
	panel_line_size = ((mode->xres + (PIXEL_ALIGN - 1)) &
					 (~(PIXEL_ALIGN - 1))) * (jzfb->osd.fg0.bpp / 8);
	panel_line_size = ((panel_line_size + 3) >> 2) << 2; /* word aligned */
	panel_frm_size = panel_line_size * mode->yres;

	/* lcd display area */
	fg0_line_size = jzfb->osd.fg0.w * jzfb->osd.fg0.bpp / 8;
	fg0_line_size = ((fg0_line_size + 3) >> 2) << 2; /* word aligned */
	fg0_frm_size= fg0_line_size * jzfb->osd.fg0.h;

	tmp =reg_read(jzfb, LCDC_OSDCTRL);
//	tmp |= LCDC_OSDCTRL_CHANGES;

	/* set change bit */
	reg_write(jzfb, LCDC_OSDCTRL, tmp);
	reg_write(jzfb, LCDC_OSDCTRL, tmp);

	dma0_desc0->cmd = LCDC_CMD_EOFINT | fg0_frm_size / 4;
	dma0_desc0->offsize = (panel_line_size - fg0_line_size) / 4;
	dma0_desc0->page_width = fg0_line_size / 4;
	dma0_desc0->desc_size = size0;

	reg_write(jzfb, LCDC_DESSIZE0, size0);

	dma_cache_wback((unsigned int)(dma0_desc0), sizeof(struct jzfb_framedesc));

	return 0;
}
