/* kernel/drivers/video/jz4780_fb.c
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

#include <mach/jzfb.h>

#include "jz4780_fb.h"
#include "regs.h"

static void dump_lcdc_registers(struct jzfb *jzfb);

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

	//do we need to reset the lcdc if surfaceflinger has crashed
	/* reset lcdc after SurfaceFlinger crach when playing video */
	tmp = reg_read(jzfb, LCDC_OSDC);
	if (tmp & LCDC_OSDC_F1EN) {
		dev_info(info->dev,"jzfb_open() LCDC_OSDC_F1EN, disable FG1.\n");
	}

	return 0;
}

void jzfb_videomode_to_var(struct fb_var_screeninfo *var,
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

static struct fb_videomode *jzfb_get_mode(struct jzfb *jzfb,
		struct fb_var_screeninfo *var)
{
	size_t i;
	struct fb_videomode *mode = jzfb->pdata->modes;

	for (i = 0; i < jzfb->pdata->num_modes; ++i, ++mode) {
		if (mode->xres == var->xres && mode->yres == var->yres)
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

	if (osd->fg0.bpp == 16) {
		ctrl |= LCDC_OSDCTRL_BPP_15_16 | LCDC_OSDCTRL_RGB0_RGB565;
	} else {
		osd->fg0.bpp = 32;
		ctrl |= LCDC_OSDCTRL_BPP_18_24;
	}

	cfg = LCDC_OSDC_OSDEN | LCDC_OSDC_F0EN;

	rgb_ctrl = (LCDC_RGBC_ODD_BGR << LCDC_RGBC_ODDRGB_BIT)
		| (LCDC_RGBC_EVEN_BGR << LCDC_RGBC_EVENRGB_BIT);

	if (rgb_ctrl == (reg_read(jzfb, LCDC_RGBC))) {
		dev_err(info->dev,"fg0 rgb order not change\n");
	}


	reg_write(jzfb, LCDC_OSDC, cfg);
	reg_write(jzfb, LCDC_OSDCTRL, ctrl);

	reg_write(jzfb, LCDC_RGBC, rgb_ctrl);
}

int jzfb_prepare_dma_desc(struct fb_info *info)
{
	int size0;
	int fg0_line_size, fg0_frm_size;
	int panel_line_size;

	struct jzfb *jzfb = info->par;
	struct fb_videomode *mode = info->mode;

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
	panel_line_size = ALIGN(mode->xres, PIXEL_ALIGN) *
		(jzfb->osd.fg0.bpp / 8);
	panel_line_size = ((panel_line_size + 3) >> 2) << 2; /* word aligned */

	jzfb->frm_size = panel_line_size * mode->yres;


	if(!jzfb->pdata->is_smart) {
		jzfb->framedesc->next = jzfb->framedesc_phys;
		jzfb->framedesc->databuf = jzfb->vidmem_phys;
		jzfb->framedesc->cmd = LCDC_CMD_SOFINT | fg0_frm_size / 4;
		jzfb->framedesc->offsize = (panel_line_size - fg0_line_size) / 4;

		if (jzfb->framedesc->offsize == 0) {
			jzfb->framedesc->page_width = 0;
		} else {
			jzfb->framedesc->page_width = fg0_line_size / 4;
		}
		jzfb->framedesc->desc_size = size0;
	} else {
	}

	reg_write(jzfb, LCDC_DA0, jzfb->framedesc->next);

	return 0;
}

static int jzfb_check_var(struct fb_var_screeninfo *var, struct fb_info *fb)
{
	struct jzfb *jzfb = fb->par;
	struct fb_videomode *mode;

	if (var->bits_per_pixel != jzfb_get_controller_bpp(jzfb) &&
			var->bits_per_pixel != jzfb->pdata->bpp)
		return -EINVAL;

	mode = jzfb_get_mode(jzfb, var);
	if (mode == NULL)
		return -EINVAL;

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
		case 18:
			var->red.offset = 16;
			var->red.length = 6;
			var->green.offset = 8;
			var->green.length = 6;
			var->blue.offset = 0;
			var->blue.length = 6;
			var->bits_per_pixel = 32;
			break;
		case 32:
		case 24:
			var->transp.offset = 24;
			var->transp.length = 8;
			var->red.offset = 16;
			var->red.length = 8;
			var->green.offset = 8;
			var->green.length = 8;
			var->blue.offset = 0;
			var->blue.length = 8;
			var->bits_per_pixel = 32;
			break;
		default:
			break;
	}

	return 0;
}

static int jzfb_set_par(struct fb_info *info)
{
	struct jzfb *jzfb = info->par;
	struct jzfb_platform_data *pdata = jzfb->pdata;
	struct fb_var_screeninfo *var = &info->var;
	struct fb_videomode *mode;
	unsigned int pcfg;
	uint16_t hds, vds;
	uint16_t hde, vde;
	uint16_t ht, vt;
	uint32_t ctrl;
	uint32_t cfg;
	unsigned long rate;

	mode = jzfb_get_mode(jzfb, var);
	if (mode == NULL)
		return -EINVAL;

	if (mode == info->mode)
		return 0;

	info->mode = mode;

	hds = mode->hsync_len + mode->left_margin;
	hde = hds + mode->xres;
	ht = hde + mode->right_margin;

	vds = mode->vsync_len + mode->upper_margin;
	vde = vds + mode->yres;
	vt = vde + mode->lower_margin;

	ctrl = LCDC_CTRL_BST_64;
	cfg = LCDC_CFG_NEWDES | LCDC_CFG_RECOVER; /* use 8words descriptor */

	cfg |= pdata->lcd_type;

	if(pdata->is_smart) {
		cfg |= LCDC_CFG_LCDPIN_SLCD;
	}

	switch (pdata->bpp) {
		case 16:
			ctrl |= LCDC_CTRL_BPP_16 | LCDC_CTRL_RGB565;
			break;
		case 18:
		case 24:
		case 32:
			ctrl |= LCDC_CTRL_BPP_18_24;
			break;
		default:
			dev_err(info->dev,"The BPP %d is not supported\n", pdata->bpp);
			ctrl |= LCDC_CTRL_BPP_18_24;
			break;
	}

	if (!(mode->sync & FB_SYNC_HOR_HIGH_ACT))
		cfg |= LCDC_CFG_HSP;

	if (!(mode->sync & FB_SYNC_VERT_HIGH_ACT))
		cfg |= LCDC_CFG_VSP;

	if (pdata->pixclk_falling_edge)
		cfg |= LCDC_CFG_PCP;

	if (pdata->date_enable_active_low)
		cfg |= LCDC_CFG_DEP;

	if (mode->pixclock) {
		rate = PICOS2KHZ(mode->pixclock) * 1000;
		mode->refresh = rate / vt / ht;
	} else {
		if (pdata->lcd_type == LCD_TYPE_8BIT_SERIAL)
			rate = mode->refresh * (vt + 2 * mode->xres) * ht;
		else
			rate = mode->refresh * vt * ht;
		mode->pixclock = KHZ2PICOS(rate / 1000);

		var->pixclock = mode->pixclock;
	}

	mutex_lock(&jzfb->lock);
	if (!jzfb->is_enabled)
		clk_enable(jzfb->ldclk);
	else
		ctrl |= LCDC_CTRL_ENA;

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

	if(pdata->is_smart) {
		//reg_write(jzfb, LCDC_SPL, pdata->smart_config.spl);
	}

	reg_write(jzfb, LCDC_HSYNC, mode->hsync_len);
	reg_write(jzfb, LCDC_VSYNC, mode->vsync_len);

	reg_write(jzfb, LCDC_VAT, (ht << 16) | vt);

	reg_write(jzfb, LCDC_DAH, (hds << 16) | hde);
	reg_write(jzfb, LCDC_DAV, (vds << 16) | vde);

	reg_write(jzfb, LCDC_CFG, cfg);

	reg_write(jzfb, LCDC_CTRL, ctrl);

	pcfg = 0xC0000000 | (511<<18) | (400<<9) | (256<<0) ;
	reg_write(jzfb, LCDC_PCFG, pcfg);

	jzfb_config_fg0(info);

	jzfb_prepare_dma_desc(info);

	if (!jzfb->is_enabled)
		clk_disable(jzfb->ldclk);

	mutex_unlock(&jzfb->lock);

	clk_set_rate(jzfb->lpclk, rate);
	clk_set_rate(jzfb->ldclk, rate * 3);

	dev_info(info->dev,"LCDC: PixClock:%lu\n", rate);
	dev_info(info->dev,"LCDC: PixClock:%lu(real)\n", clk_get_rate(jzfb->lpclk));

	udelay(1000);

	return 0;
}

static void jzfb_enable(struct jzfb *jzfb)
{
	uint32_t ctrl;

	clk_enable(jzfb->ldclk);

	reg_write(jzfb, LCDC_STATE, 0);

	reg_write(jzfb, LCDC_DA0, jzfb->framedesc->next);

	ctrl = reg_read(jzfb, LCDC_CTRL);
	ctrl |= LCDC_CTRL_ENA;
	ctrl &= ~LCDC_CTRL_DIS;
	reg_write(jzfb, LCDC_CTRL, ctrl);
}

static void jzfb_disable(struct jzfb *jzfb)
{
	uint32_t ctrl;

	ctrl = reg_read(jzfb, LCDC_CTRL);
	ctrl |= LCDC_CTRL_DIS;
	reg_write(jzfb, LCDC_CTRL,ctrl);
	do {
		ctrl = reg_read(jzfb, LCDC_STATE);
	} while (!(ctrl & LCDC_STATE_LDD));

	clk_disable(jzfb->ldclk);
}

static int jzfb_blank(int blank_mode, struct fb_info *info)
{
	struct jzfb *jzfb = info->par;

	switch (blank_mode) {
		case FB_BLANK_UNBLANK:
			mutex_lock(&jzfb->lock);
			if (jzfb->is_enabled) {
				mutex_unlock(&jzfb->lock);
				return 0;
			}

			jzfb_enable(jzfb);
			jzfb->is_enabled = 1;

			mutex_unlock(&jzfb->lock);
			break;
		default:
			mutex_lock(&jzfb->lock);
			if (!jzfb->is_enabled) {
				mutex_unlock(&jzfb->lock);
				return 0;
			}

			jzfb_disable(jzfb);
			jzfb->is_enabled = 0;

			mutex_unlock(&jzfb->lock);
			break;
	}

	return 0;
}

static int jzfb_alloc_devmem(struct jzfb *jzfb)
{
	int max_videosize = 0;
	struct fb_videomode *mode = jzfb->pdata->modes;
	void *page;
	int i;

	for (i = 0; i < jzfb->pdata->num_modes; ++mode, ++i) {
		if (max_videosize < mode->xres * mode->yres)
			max_videosize = mode->xres * mode->yres
				* NUM_FRAME_BUFFERS;
	}

	max_videosize *= jzfb_get_controller_bpp(jzfb) >> 3;

	jzfb->vidmem_size = PAGE_ALIGN(max_videosize);
	jzfb->vidmem = dma_alloc_coherent(&jzfb->pdev->dev,
			jzfb->vidmem_size,
			&jzfb->vidmem_phys, GFP_KERNEL);

	if (!jzfb->vidmem)
		return -ENOMEM;

	for (page = jzfb->vidmem;
			page < jzfb->vidmem + PAGE_ALIGN(jzfb->vidmem_size);
			page += PAGE_SIZE) {
		SetPageReserved(virt_to_page(page));
	}

	jzfb->framedesc = dma_alloc_coherent(&jzfb->pdev->dev,
			sizeof(*jzfb->framedesc) * jzfb->desc_num,
			&jzfb->framedesc_phys, GFP_KERNEL);

	if (!jzfb->framedesc)
		return -ENOMEM;

	return 0;
}

static void jzfb_free_devmem(struct jzfb *jzfb)
{
	dma_free_coherent(&jzfb->pdev->dev, jzfb->vidmem_size,
			jzfb->vidmem, jzfb->vidmem_phys);
	dma_free_coherent(&jzfb->pdev->dev, sizeof(*jzfb->framedesc) * jzfb->desc_num,
			jzfb->framedesc, jzfb->framedesc_phys);
}

static int jzfb_pan_display(struct fb_var_screeninfo *var, struct fb_info *info)
{
	unsigned int state;
	int now_frm,next_frm,ctrl;
	struct jzfb *jzfb = info->par;

	if (var->xoffset - info->var.xoffset) {
		dev_err(info->dev,"No support for X panning for now!\n");
		return -EINVAL;
	}

	now_frm = reg_read(jzfb,LCDC_IID);
	next_frm = var->yoffset / var->yres;
	if(now_frm == next_frm)
		return 0;

	if(!jzfb->pdata->is_smart) {
		jzfb->framedesc->databuf = jzfb->vidmem_phys + jzfb->frm_size * next_frm;
		jzfb->framedesc->id = next_frm;
	} else {
		//smart tft spec code here.
	}

	jzfb->frm_id = 0xff;
	state = reg_read(jzfb, LCDC_STATE);
	reg_write(jzfb, LCDC_STATE, state & ~LCDC_STATE_SOF);
	ctrl = reg_read(jzfb, LCDC_CTRL);
	reg_write(jzfb, LCDC_CTRL, ctrl | (1<<12));

	if(!wait_event_interruptible_timeout(jzfb->frame_wq, next_frm == jzfb->frm_id, HZ)) {
		dev_err(info->dev,"wait for filp timeout!\n");
		return -ETIME;
	}

	ctrl = reg_read(jzfb, LCDC_CTRL);
	reg_write(jzfb, LCDC_CTRL,ctrl & ~(1<<12));

	return 0;
}

static int jzfb_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static irqreturn_t jzfb_irq_handler(int irq, void *data)
{
	static int irq_cnt = 0;
	unsigned int state;
	struct jzfb *jzfb = (struct jzfb *)data;

	state = reg_read(jzfb, LCDC_STATE);

	if (state & LCDC_STATE_SOF) {
		reg_write(jzfb, LCDC_STATE, state & ~LCDC_STATE_SOF);
		jzfb->frm_id = reg_read(jzfb,LCDC_IID);

		wake_up(&jzfb->frame_wq);
	}

	if (state & LCDC_STATE_OFU) {
		reg_write(jzfb, LCDC_STATE, state & ~LCDC_STATE_OFU);
		if ( irq_cnt++ > 100 ) {
			pr_err("disable Out FiFo underrun irq.\n");
		}
		pr_err("%s, Out FiFo underrun.\n", __FUNCTION__);		
	}

	return IRQ_HANDLED;
}

static struct fb_ops jzfb_ops = {
	.owner = THIS_MODULE,
	.fb_open		= jzfb_open,
	.fb_check_var 		= jzfb_check_var,
	.fb_set_par 		= jzfb_set_par,
	.fb_blank		= jzfb_blank,
	.fb_pan_display		= jzfb_pan_display,
	.fb_fillrect		= cfb_fillrect,
	.fb_copyarea		= cfb_copyarea, //recover it later
	.fb_imageblit		= cfb_imageblit,
	.fb_ioctl		= jzfb_ioctl,	
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void jzfb_early_suspend(struct early_suspend *h)
{
	struct jzfb *jzfb = container_of(h, struct jzfb, early_suspend);
	fb_set_suspend(jzfb->fb, 1);
	jzfb_disable(jzfb);
}

static void jzfb_late_resume(struct early_suspend *h)
{
	struct jzfb *jzfb = container_of(h, struct jzfb, early_suspend);
	jzfb_enable(jzfb);
	fb_set_suspend(jzfb->fb, 0);
}
#endif

static void dump_lcdc_registers(struct jzfb *jzfb)
{
#if 0
	unsigned int * pii;
	int i, j;
	/* LCD Controller Resgisters */
	printk("LCDC_CFG:\t0x%08x\n", reg_read(jzfb, LCDC_CFG));
	printk("LCDC_CTRL:\t0x%08x\n", reg_read(jzfb, LCDC_CTRL));
	printk("LCDC_STATE:\t0x%08x\n", reg_read(jzfb, LCDC_STATE));
	printk("LCDC_OSDC:\t0x%08x\n", reg_read(jzfb, LCDC_OSDC));
	printk("LCDC_OSDCTRL:\t0x%08x\n", reg_read(jzfb, LCDC_OSDCTRL));
	printk("LCDC_OSDS:\t0x%08x\n", reg_read(jzfb, LCDC_OSDS));
	printk("LCDC_BGC:\t0x%08x\n", reg_read(jzfb, LCDC_BGC));
	printk("LCDC_KEY0:\t0x%08x\n", reg_read(jzfb, LCDC_KEY0));
	printk("LCDC_KEY1:\t0x%08x\n", reg_read(jzfb, LCDC_KEY1));
	printk("LCDC_ALPHA:\t0x%08x\n", reg_read(jzfb, LCDC_ALPHA));
	printk("LCDC_IPUR:\t0x%08x\n", reg_read(jzfb, LCDC_IPUR));
	printk("LCDC_VAT:\t0x%08x\n",reg_read(jzfb, LCDC_VAT));
	printk("LCDC_DAH:\t0x%08x\n", reg_read(jzfb, LCDC_DAH));
	printk("LCDC_DAV:\t0x%08x\n", reg_read(jzfb, LCDC_DAV));
	printk("LCDC_XYP0:\t0x%08x\n", reg_read(jzfb, LCDC_XYP0));
	printk("LCDC_XYP1:\t0x%08x\n", reg_read(jzfb, LCDC_XYP1));
	printk("LCDC_SIZE0:\t0x%08x\n", reg_read(jzfb, LCDC_SIZE0));
	printk("LCDC_SIZE1:\t0x%08x\n", reg_read(jzfb, LCDC_SIZE1));
	printk("LCDC_RGBC\t0x%08x\n", reg_read(jzfb, LCDC_RGBC));
	printk("LCDC_VSYNC:\t0x%08x\n", reg_read(jzfb, LCDC_VSYNC));
	printk("LCDC_HSYNC:\t0x%08x\n", reg_read(jzfb, LCDC_HSYNC));
	printk("LCDC_PS:\t0x%08x\n", reg_read(jzfb, LCDC_PS));
	printk("LCDC_CLS:\t0x%08x\n", reg_read(jzfb, LCDC_CLS));
	printk("LCDC_SPL:\t0x%08x\n", reg_read(jzfb, LCDC_SPL));
	printk("LCDC_REV:\t0x%08x\n", reg_read(jzfb, LCDC_REV));
	printk("LCDC_IID:\t0x%08x\n", reg_read(jzfb, LCDC_IID));
	printk("LCDC_DA0:\t0x%08x\n", reg_read(jzfb, LCDC_DA0));
	printk("LCDC_SA0:\t0x%08x\n", reg_read(jzfb, LCDC_SA0));
	printk("LCDC_FID0:\t0x%08x\n", reg_read(jzfb, LCDC_FID0));
	printk("LCDC_CMD0:\t0x%08x\n", reg_read(jzfb, LCDC_CMD0));
	printk("LCDC_OFFS0:\t0x%08x\n", reg_read(jzfb, LCDC_OFFS0));
	printk("LCDC_PW0:\t0x%08x\n", reg_read(jzfb, LCDC_PW0));
	printk("LCDC_CNUM0:\t0x%08x\n", reg_read(jzfb, LCDC_CNUM0));
	printk("LCDC_DESSIZE0:\t0x%08x\n", reg_read(jzfb, LCDC_DESSIZE0));
	printk("LCDC_DA1:\t0x%08x\n", reg_read(jzfb, LCDC_DA1));
	printk("LCDC_SA1:\t0x%08x\n", reg_read(jzfb, LCDC_SA1));
	printk("LCDC_FID1:\t0x%08x\n", reg_read(jzfb, LCDC_FID1));
	printk("LCDC_CMD1:\t0x%08x\n", reg_read(jzfb, LCDC_CMD1));
	printk("LCDC_OFFS1:\t0x%08x\n", reg_read(jzfb, LCDC_OFFS1));
	printk("LCDC_PW1:\t0x%08x\n", reg_read(jzfb, LCDC_PW1));
	printk("LCDC_CNUM1:\t0x%08x\n", reg_read(jzfb, LCDC_CNUM1));
	printk("LCDC_DESSIZE1:\t0x%08x\n", reg_read(jzfb, LCDC_DESSIZE1));
	printk("==================================\n");
	printk("LCDC_VSYNC:\t%08x\n", reg_read(jzfb, LCDC_VSYNC));
	printk("LCDC_HSYNC:\t%08x\n", reg_read(jzfb, LCDC_HSYNC));
	printk("LCDC_VAT:\t%08x\n", reg_read(jzfb, LCDC_VAT));
	printk("LCDC_DAH:\t%08x\n", reg_read(jzfb, LCDC_DAH));
	printk("LCDC_DAV:\t%08x\n", reg_read(jzfb, LCDC_DAV));
	printk("==================================\n");
	printk("LCDC_PCFG:\t0x%08lx\n", reg_read(jzfb, LCDC_PCFG));

#endif
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
	struct jzfb *jzfb;
	struct fb_info *fb;
	struct jzfb_platform_data *pdata = pdev->dev.platform_data;
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
	jzfb->pdev = pdev;
	jzfb->pdata = pdata;
	jzfb->mem = mem;

	if(!jzfb->pdata->is_smart)
		jzfb->desc_num = 1;
	else
		jzfb->desc_num = 4;

	//	jzfb->ldclk = clk_get(&pdev->dev, "lcd");
	if (IS_ERR(jzfb->ldclk)) {
		ret = PTR_ERR(jzfb->ldclk);
		dev_err(&pdev->dev, "Failed to get lcd cupdate_lock: %d\n", ret);
		goto err_framebuffer_release;
	}

	//	jzfb->lpclk = clk_get(&pdev->dev, "lcd_pclk");
	if (IS_ERR(jzfb->lpclk)) {
		ret = PTR_ERR(jzfb->lpclk);
		dev_err(&pdev->dev, "Failed to get lcd pixel cupdate_lock: %d\n", ret);
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

	init_waitqueue_head(&jzfb->frame_wq);

	fb_videomode_to_modelist(pdata->modes, pdata->num_modes,
			&fb->modelist);
	jzfb_videomode_to_var(&fb->var, pdata->modes);
	fb->var.width = pdata->width;
	fb->var.height = pdata->height;
	fb->var.bits_per_pixel = pdata->bpp;

	jzfb_check_var(&fb->var, fb);

	ret = jzfb_alloc_devmem(jzfb);
	if (ret) {
		dev_err(&pdev->dev, "Failed to allocate video memory\n");
		goto err_iounmap;
	}

	fb->fix = jzfb_fix;
	fb->fix.line_length = fb->var.bits_per_pixel * ALIGN(fb->var.xres,
			PIXEL_ALIGN) / 8;
	fb->fix.mmio_start = mem->start;
	fb->fix.mmio_len = resource_size(mem);
	fb->fix.smem_start = jzfb->vidmem_phys;
	fb->fix.smem_len =  jzfb->vidmem_size;
	fb->screen_base = jzfb->vidmem;

	clk_enable(jzfb->ldclk);
	jzfb->is_enabled = pdata->enable;

	fb->mode = NULL;
	jzfb_set_par(fb);

	ret = register_framebuffer(fb);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register framebuffer: %d\n", ret);
		goto err_free_devmem;
	}

	jzfb->irq = platform_get_irq(pdev, 0);
	if (request_irq(jzfb->irq, jzfb_irq_handler, IRQF_SHARED,
				pdev->name, jzfb)) {
		dev_err(&pdev->dev,"request irq failed\n");
		ret = -EINVAL;
		goto err_free_irq;
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
		goto err_free_file;
	}
	if (jzfb->is_enabled) {
		jzfb_enable(jzfb);
	} else {
		jzfb_disable(jzfb);
	}

	return 0;

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
	fb_dealloc_cmap(&fb->cmap);
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

	jzfb_blank(FB_BLANK_POWERDOWN, jzfb->fb);

	iounmap(jzfb->base);
	release_mem_region(jzfb->mem->start, resource_size(jzfb->mem));

	jzfb_free_devmem(jzfb);
	platform_set_drvdata(pdev, NULL);

	clk_put(jzfb->lpclk);
	clk_put(jzfb->ldclk);

	framebuffer_release(jzfb->fb);

	for (i = 0; i < ARRAY_SIZE(lcd_sysfs_attrs); i++) {
		device_remove_file(&pdev->dev, &lcd_sysfs_attrs[i]);
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&jzfb->early_suspend);
#endif

	return 0;
}

static struct platform_driver jzfb_driver = {
	.probe 	= jzfb_probe,
	.remove = jzfb_remove,
	.driver = {
		.name = "jz4780-fb",
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
