/* kernel/drivers/video/jz4780/image_enh.c
 *
 * Copyright (c) 2012 Ingenic Semiconductor Co., Ltd.
 *              http://www.ingenic.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/fb.h>
#include <linux/delay.h>
#include <mach/jzfb.h>

#include "jz4780_fb.h"
#include "regs.h"
#include "image_enh.h"

static void jzfb_get_gamma(struct fb_info *info, struct enh_gamma *gamma)
{
	struct jzfb *jzfb = info->par;
	unsigned int tmp;
	int i = 1000;

	tmp = reg_read(jzfb, LCDC_ENH_CFG);
	gamma->gamma_en = tmp & LCDC_ENH_CFG_GAMMA_EN;

	for (i = 0; i < LCDC_ENH_GAMMA_LEN >> 2; i++) {
		tmp = reg_read(jzfb, LCDC_ENH_GAMMA + i * 4);
		gamma->gamma_data0[i] = (tmp & LCDC_ENH_GAMMA_GAMMA_DATA0_MASK) >>
			LCDC_ENH_GAMMA_GAMMA_DATA0_BIT;
		gamma->gamma_data1[i] = (tmp & LCDC_ENH_GAMMA_GAMMA_DATA1_MASK) >>
			LCDC_ENH_GAMMA_GAMMA_DATA1_BIT;
	}
}

static int jzfb_set_gamma(struct fb_info *info, struct enh_gamma *gamma)
{
	struct jzfb *jzfb = info->par;
	unsigned int tmp;
	int i = 1000;

	tmp = reg_read(jzfb, LCDC_ENH_CFG);
	if (gamma->gamma_en) {
		tmp |= LCDC_ENH_CFG_GAMMA_EN;
		reg_write(jzfb, LCDC_ENH_CFG, tmp);
	} else {
		tmp &= ~LCDC_ENH_CFG_GAMMA_EN;
		reg_write(jzfb, LCDC_ENH_CFG, tmp);
		do {
			mdelay(1);
			tmp = reg_read(jzfb, LCDC_ENH_STATUS);
		}
		while (!(tmp & LCDC_ENH_STATUS_GAMMA_DIS) && i--);
		if (!i)
			dev_info(info->dev, "Disable gamma time out");
		return 0;
	}

	for (i = 0; i < LCDC_ENH_GAMMA_LEN >> 2; i++) {
		tmp = gamma->gamma_data0[i] << LCDC_ENH_GAMMA_GAMMA_DATA0_BIT
			& LCDC_ENH_GAMMA_GAMMA_DATA0_MASK;
		tmp |= (gamma->gamma_data1[i] << LCDC_ENH_GAMMA_GAMMA_DATA1_BIT
			& LCDC_ENH_GAMMA_GAMMA_DATA1_MASK);
		reg_write(jzfb, LCDC_ENH_GAMMA + i * 4, tmp);
	}

	return 0;
}

static void jzfb_get_csc(struct fb_info *info, struct enh_csc *csc)
{
	struct jzfb *jzfb = info->par;
	unsigned int tmp;

	tmp = reg_read(jzfb, LCDC_ENH_CFG);
	csc->rgb2ycc_en = tmp & LCDC_ENH_CFG_RGB2YCC_EN;
	csc->ycc2rgb_en = tmp & LCDC_ENH_CFG_YCC2RGB_EN;

	tmp = reg_read(jzfb, LCDC_ENH_CSCCFG);
	csc->rgb2ycc_mode = (tmp & LCDC_ENH_CSCCFG_RGB2YCCMD_MASK) >>
		LCDC_ENH_CSCCFG_RGB2YCCMD_BIT;
	csc->ycc2rgb_mode = (tmp & LCDC_ENH_CSCCFG_YCC2RGBMD_MASK) >>
		LCDC_ENH_CSCCFG_YCC2RGBMD_BIT;
}

static void jzfb_set_csc(struct fb_info *info, struct enh_csc *csc)
{
	struct jzfb *jzfb = info->par;
	unsigned int tmp;
	int i = 1000;

	tmp = csc->rgb2ycc_mode << LCDC_ENH_CSCCFG_RGB2YCCMD_BIT &
		LCDC_ENH_CSCCFG_RGB2YCCMD_MASK;
	tmp |= (csc->ycc2rgb_mode << LCDC_ENH_CSCCFG_YCC2RGBMD_BIT &
		LCDC_ENH_CSCCFG_YCC2RGBMD_MASK);
	reg_write(jzfb, LCDC_ENH_CSCCFG, tmp);

	tmp = reg_read(jzfb, LCDC_ENH_CFG);
	if (csc->rgb2ycc_en) {
		tmp |= LCDC_ENH_CFG_RGB2YCC_EN;
		reg_write(jzfb, LCDC_ENH_CFG, tmp);
	} else {
		tmp &= ~LCDC_ENH_CFG_RGB2YCC_EN;
		reg_write(jzfb, LCDC_ENH_CFG, tmp);
		do {
			mdelay(1);
			tmp = reg_read(jzfb, LCDC_ENH_STATUS);
		}
		while (!(tmp & LCDC_ENH_STATUS_RGB2YCC_DIS) && i--);
		if (!i)
			dev_info(info->dev, "Disable rgb2ycc time out");
	}

	tmp = reg_read(jzfb, LCDC_ENH_CFG);
	if (csc->ycc2rgb_en) {
		tmp |= LCDC_ENH_CFG_YCC2RGB_EN;
		reg_write(jzfb, LCDC_ENH_CFG, tmp);
	} else {
		tmp &= ~LCDC_ENH_CFG_YCC2RGB_EN;
		reg_write(jzfb, LCDC_ENH_CFG, tmp);
		i = 1000;
		do {
			mdelay(1);
			tmp = reg_read(jzfb, LCDC_ENH_STATUS);
		}
		while (!(tmp & LCDC_ENH_STATUS_YCC2RGB_DIS) && i--);
		if (!i)
			dev_info(info->dev, "Disable ycc2rgb time out");
	}
}

static void jzfb_get_luma(struct fb_info *info, struct enh_luma *luma)
{
	struct jzfb *jzfb = info->par;
	unsigned int tmp;

	tmp = reg_read(jzfb, LCDC_ENH_CFG);
	luma->brightness_en = tmp & LCDC_ENH_CFG_BRIGHTNESS_EN;
	luma->contrast_en = tmp & LCDC_ENH_CFG_CONTRAST_EN;

	tmp = reg_read(jzfb, LCDC_ENH_LUMACFG);
	luma->brightness = (tmp & LCDC_ENH_LUMACFG_BRIGHTNESS_MASK) >>
			LCDC_ENH_LUMACFG_BRIGHTNESS_BIT;
	luma->contrast = (tmp & LCDC_ENH_LUMACFG_CONTRAST_MASK) >>
			LCDC_ENH_LUMACFG_CONTRAST_BIT;
}

static void jzfb_set_luma(struct fb_info *info, struct enh_luma *luma)
{
	struct jzfb *jzfb = info->par;
	unsigned int tmp;
	int i = 1000;

	if (luma->brightness_en) {
		tmp = reg_read(jzfb, LCDC_ENH_CFG);
		tmp |= LCDC_ENH_CFG_BRIGHTNESS_EN;
		reg_write(jzfb, LCDC_ENH_CFG, tmp);

		tmp = reg_read(jzfb, LCDC_ENH_LUMACFG);
		luma->brightness = (tmp & LCDC_ENH_LUMACFG_BRIGHTNESS_MASK) >>
			LCDC_ENH_LUMACFG_BRIGHTNESS_BIT;
		reg_write(jzfb, LCDC_ENH_LUMACFG, tmp);
	} else {
		tmp = reg_read(jzfb, LCDC_ENH_CFG);
		tmp &= ~LCDC_ENH_CFG_BRIGHTNESS_EN;
		reg_write(jzfb, LCDC_ENH_CFG, tmp);
		do {
			mdelay(1);
			tmp = reg_read(jzfb, LCDC_ENH_STATUS);
		}
		while (!(tmp & LCDC_ENH_STATUS_BRIGHTNESS_DIS) && i--);
		if (!i)
			dev_info(info->dev, "Disable brightness time out");
	}
	
	if (luma->contrast_en) {
		tmp = reg_read(jzfb, LCDC_ENH_CFG);
		tmp |= LCDC_ENH_CFG_CONTRAST_EN;
		reg_write(jzfb, LCDC_ENH_CFG, tmp);

		tmp = reg_read(jzfb, LCDC_ENH_LUMACFG);
		luma->contrast = (tmp & LCDC_ENH_LUMACFG_CONTRAST_MASK) >>
			LCDC_ENH_LUMACFG_CONTRAST_BIT;
		reg_write(jzfb, LCDC_ENH_LUMACFG, tmp);
	} else {
		tmp = reg_read(jzfb, LCDC_ENH_CFG);
		tmp &= ~LCDC_ENH_CFG_CONTRAST_EN;
		reg_write(jzfb, LCDC_ENH_CFG, tmp);
		do {
			mdelay(1);
			tmp = reg_read(jzfb, LCDC_ENH_STATUS);
		}
		while (!(tmp & LCDC_ENH_STATUS_CONTRAST_DIS) && i--);
		if (!i)
			dev_info(info->dev, "Disable contrast time out");
	}
}

static void jzfb_get_hue(struct fb_info *info, struct enh_hue *hue)
{
	struct jzfb *jzfb = info->par;
	unsigned int tmp;

	tmp = reg_read(jzfb, LCDC_ENH_CFG);
	hue->hue_en = tmp & LCDC_ENH_CFG_HUE_EN;

	tmp = reg_read(jzfb, LCDC_ENH_CHROCFG0);
	hue->hue_sin = (tmp & LCDC_ENH_CHROCFG0_HUE_SIN_MASK) >>
		LCDC_ENH_CHROCFG0_HUE_SIN_BIT;
	hue->hue_cos = (tmp & LCDC_ENH_CHROCFG0_HUE_COS_MASK) >>
		LCDC_ENH_CHROCFG0_HUE_COS_BIT;
}

static void jzfb_set_hue(struct fb_info *info, struct enh_hue *hue)
{
	struct jzfb *jzfb = info->par;
	unsigned int tmp;
	int i = 1000;

	if (hue->hue_en) {
		tmp = reg_read(jzfb, LCDC_ENH_CFG);
		tmp |= LCDC_ENH_CFG_HUE_EN;
		reg_write(jzfb, LCDC_ENH_CFG, tmp);

		tmp = hue->hue_sin << LCDC_ENH_CHROCFG0_HUE_SIN_BIT
			& LCDC_ENH_CHROCFG0_HUE_SIN_MASK;
		tmp |= (hue->hue_cos << LCDC_ENH_CHROCFG0_HUE_COS_BIT
			& LCDC_ENH_CHROCFG0_HUE_COS_MASK); 
		reg_write(jzfb, LCDC_ENH_CHROCFG0, tmp);
	} else {
		tmp = reg_read(jzfb, LCDC_ENH_CFG);
		tmp &= ~LCDC_ENH_CFG_HUE_EN;
		reg_write(jzfb, LCDC_ENH_CFG, tmp);
		do {
			mdelay(1);
			tmp = reg_read(jzfb, LCDC_ENH_STATUS);
		}
		while (!(tmp & LCDC_ENH_STATUS_HUE_DIS) && i--);
		if (!i)
			dev_info(info->dev, "Disable hue time out");
	}
}

static void jzfb_get_saturation(struct fb_info *info, struct enh_chroma *chroma)
{
	struct jzfb *jzfb = info->par;
	unsigned int tmp;

	tmp = reg_read(jzfb, LCDC_ENH_CFG);
	chroma->saturation_en = tmp & LCDC_ENH_CFG_SATURATION_EN;

	tmp = reg_read(jzfb, LCDC_ENH_CHROCFG1);
	chroma->saturation = (tmp & LCDC_ENH_CHROCFG1_SATURATION_MASK) >>
		LCDC_ENH_CHROCFG1_SATURATION_BIT;
}

static void jzfb_set_saturation(struct fb_info *info, struct enh_chroma *chroma)
{
	struct jzfb *jzfb = info->par;
	unsigned int tmp;
	int i = 1000;

	if (chroma->saturation_en) {
		tmp = reg_read(jzfb, LCDC_ENH_CFG);
		tmp |= LCDC_ENH_CFG_SATURATION_EN;
		reg_write(jzfb, LCDC_ENH_CFG, tmp);

		tmp = chroma->saturation << LCDC_ENH_CHROCFG1_SATURATION_BIT
			& LCDC_ENH_CHROCFG1_SATURATION_MASK; 
		reg_write(jzfb, LCDC_ENH_CHROCFG0, tmp);
	} else {
		tmp = reg_read(jzfb, LCDC_ENH_CFG);
		tmp &= ~LCDC_ENH_CFG_SATURATION_EN;
		reg_write(jzfb, LCDC_ENH_CFG, tmp);
		do {
			mdelay(1);
			tmp = reg_read(jzfb, LCDC_ENH_STATUS);
		}
		while (!(tmp & LCDC_ENH_STATUS_SATURATION_DIS) && i--);
		if (!i)
			dev_info(info->dev, "Disable saturation time out");
	}
}

static void jzfb_get_vee(struct fb_info *info, struct enh_vee *vee)
{
	struct jzfb *jzfb = info->par;
	unsigned int tmp;
	int i = 1000;

	tmp = reg_read(jzfb, LCDC_ENH_CFG);
	vee->vee_en = tmp & LCDC_ENH_CFG_VEE_EN;

	for (i = 0; i < LCDC_ENH_VEE_LEN >> 2; i++) {
		tmp = reg_read(jzfb, LCDC_ENH_VEE + i * 4);
		vee->vee_data0[i] = (tmp & LCDC_ENH_VEE_VEE_DATA0_MASK) >>
			LCDC_ENH_VEE_VEE_DATA0_BIT;
		vee->vee_data1[i] = (tmp & LCDC_ENH_VEE_VEE_DATA1_MASK) >>
			LCDC_ENH_VEE_VEE_DATA1_BIT;
	}
}

static int jzfb_set_vee(struct fb_info *info, struct enh_vee *vee)
{
	struct jzfb *jzfb = info->par;
	unsigned int tmp;
	int i = 1000;

	tmp = reg_read(jzfb, LCDC_ENH_CFG);
	if (vee->vee_en) {
		tmp |= LCDC_ENH_CFG_VEE_EN;
		reg_write(jzfb, LCDC_ENH_CFG, tmp);
	} else {
		tmp &= ~LCDC_ENH_CFG_VEE_EN;
		reg_write(jzfb, LCDC_ENH_CFG, tmp);
		do {
			mdelay(1);
			tmp = reg_read(jzfb, LCDC_ENH_STATUS);
		}
		while (!(tmp & LCDC_ENH_STATUS_VEE_DIS) && i--);
		if (!i)
			dev_info(info->dev, "Disable vee time out");
		return 0;
	}

	for (i = 0; i < LCDC_ENH_VEE_LEN >> 2; i++) {
		tmp = vee->vee_data0[i] << LCDC_ENH_VEE_VEE_DATA0_BIT
			& LCDC_ENH_VEE_VEE_DATA0_MASK;
		tmp |= (vee->vee_data1[i] << LCDC_ENH_VEE_VEE_DATA1_BIT
			& LCDC_ENH_VEE_VEE_DATA1_MASK);
		reg_write(jzfb, LCDC_ENH_VEE + i * 4, tmp);
	}

	return 0;
}

static void jzfb_get_dither(struct fb_info *info, struct enh_dither *dither)
{
	struct jzfb *jzfb = info->par;
	unsigned int tmp;

	tmp = reg_read(jzfb, LCDC_ENH_CFG);
	dither->dither_en = tmp & LCDC_ENH_CFG_DITHER_EN;

	tmp = reg_read(jzfb, LCDC_ENH_DITHERCFG);
	dither->dither_red = (tmp & LCDC_ENH_DITHERCFG_DITHERMD_RED_MASK) >>
		LCDC_ENH_DITHERCFG_DITHERMD_RED_BIT;
	dither->dither_green = (tmp & LCDC_ENH_DITHERCFG_DITHERMD_GREEN_MASK) >>
		LCDC_ENH_DITHERCFG_DITHERMD_GREEN_BIT;
	dither->dither_blue = (tmp & LCDC_ENH_DITHERCFG_DITHERMD_BLUE_MASK) >>
		LCDC_ENH_DITHERCFG_DITHERMD_BLUE_BIT;
}

static void jzfb_set_dither(struct fb_info *info, struct enh_dither *dither)
{
	struct jzfb *jzfb = info->par;
	unsigned int tmp;
	int i = 1000;

	if (dither->dither_en) {
		tmp = reg_read(jzfb, LCDC_ENH_CFG);
		tmp |= LCDC_ENH_CFG_DITHER_EN;
		reg_write(jzfb, LCDC_ENH_CFG, tmp);

		tmp = dither->dither_red << LCDC_ENH_DITHERCFG_DITHERMD_RED_BIT
			& LCDC_ENH_DITHERCFG_DITHERMD_RED_MASK;
		tmp |= (dither->dither_green << LCDC_ENH_DITHERCFG_DITHERMD_GREEN_BIT
			& LCDC_ENH_DITHERCFG_DITHERMD_GREEN_MASK);
		tmp |= (dither->dither_blue << LCDC_ENH_DITHERCFG_DITHERMD_BLUE_BIT
			& LCDC_ENH_DITHERCFG_DITHERMD_BLUE_MASK);
		reg_write(jzfb, LCDC_ENH_DITHERCFG, tmp);
	} else {
		tmp = reg_read(jzfb, LCDC_ENH_CFG);
		tmp &= ~LCDC_ENH_CFG_DITHER_EN;
		reg_write(jzfb, LCDC_ENH_CFG, tmp);
		do {
			mdelay(1);
			tmp = reg_read(jzfb, LCDC_ENH_STATUS);
		}
		while (!(tmp & LCDC_ENH_STATUS_DITHER_DIS) && i--);
		if (!i)
			dev_info(info->dev, "Disable dither time out");
	}
}

int jzfb_config_image_enh(struct fb_info *info)
{
	struct jzfb *jzfb = info->par;
	struct jzfb_platform_data *pdata = jzfb->pdata;
	unsigned int tmp;
	struct enh_dither *dither;

	if (pdata->dither_enable) {
		dither = kzalloc(sizeof(struct enh_dither), GFP_KERNEL);
		dither->dither_en = pdata->dither_enable;
		dither->dither_red = pdata->dither.dither_red;
		dither->dither_green = pdata->dither.dither_green;
		dither->dither_blue = pdata->dither.dither_blue;
		jzfb_set_dither(info, dither);
		kzfree(dither);
	}

	/* enable the global image enhancement bit */
	tmp = reg_read(jzfb, LCDC_ENH_CFG);
	tmp |= LCDC_ENH_CFG_ENH_EN;
	reg_write(jzfb, LCDC_ENH_CFG, tmp);

	return 0;
}

int jzfb_image_enh_ioctl(struct fb_info *info, unsigned int cmd,
			 unsigned long arg)
{
	void __user *argp = (void __user *)arg;

	union {
		struct enh_gamma gamma;
		struct enh_csc csc;
		struct enh_luma luma;
		struct enh_hue hue;
		struct enh_chroma chroma;
		struct enh_vee vee;
		struct enh_dither dither;
	} enh;

	switch (cmd) {
	case JZFB_GET_GAMMA:
		jzfb_get_gamma(info, &enh.gamma);
		if (copy_to_user(argp, &enh.gamma, sizeof(struct enh_gamma)))
			return -EFAULT;
		break;
	case JZFB_SET_GAMMA:
		if (copy_from_user(&enh.gamma, argp, sizeof(struct enh_gamma))) {
			dev_err(info->dev, "copy gamma from user error");
			return -EFAULT;
		} else {
			jzfb_set_gamma(info, &enh.gamma);
		}
		break;
	case JZFB_GET_CSC:
		jzfb_get_csc(info, &enh.csc);
		if (copy_to_user(argp, &enh.csc, sizeof(struct enh_csc)))
			return -EFAULT;
		break;
	case JZFB_SET_CSC:
		if (copy_from_user(&enh.csc, argp, sizeof(struct enh_csc))) {
			dev_err(info->dev, "copy csc from user error");
			return -EFAULT;
		} else {
			jzfb_set_csc(info, &enh.csc);
		}
		break;
	case JZFB_GET_LUMA:
		jzfb_get_luma(info, &enh.luma);
		if (copy_to_user(argp, &enh.luma, sizeof(struct enh_luma)))
			return -EFAULT;
		break;
	case JZFB_SET_LUMA:
		if (copy_from_user(&enh.luma, argp, sizeof(struct enh_luma))) {
			dev_err(info->dev, "copy luma from user error");
			return -EFAULT;
		} else {
			jzfb_set_luma(info, &enh.luma);
		}
		break;
	case JZFB_GET_HUE:
		jzfb_get_hue(info, &enh.hue);
		if (copy_to_user(argp, &enh.hue, sizeof(struct enh_hue)))
			return -EFAULT;
		break;
	case JZFB_SET_HUE:
		if (copy_from_user(&enh.hue, argp, sizeof(struct enh_hue))) {
			dev_err(info->dev, "copy hue from user error");
			return -EFAULT;
		} else {
			jzfb_set_hue(info, &enh.hue);
		}
		break;
	case JZFB_GET_CHROMA:
		jzfb_get_saturation(info, &enh.chroma);
		if (copy_to_user(argp, &enh.chroma, sizeof(struct enh_chroma)))
			return -EFAULT;
		break;
	case JZFB_SET_CHROMA:
		if (copy_from_user(&enh.chroma, argp, sizeof(struct enh_chroma))) {
			dev_err(info->dev, "copy chroma from user error");
			return -EFAULT;
		} else {
			jzfb_set_saturation(info, &enh.chroma);
		}
		break;
	case JZFB_GET_VEE:
		jzfb_get_vee(info, &enh.vee);
		if (copy_to_user(argp, &enh.vee, sizeof(struct enh_vee)))
			return -EFAULT;
		break;
	case JZFB_SET_VEE:
		if (copy_from_user(&enh.vee, argp, sizeof(struct enh_vee))) {
			dev_err(info->dev, "copy vee from user error");
			return -EFAULT;
		} else {
			jzfb_set_vee(info, &enh.vee);
		}
		break;
	case JZFB_GET_DITHER:
		jzfb_get_dither(info, &enh.dither);
		if (copy_to_user(argp, &enh.dither, sizeof(struct enh_dither)))
			return -EFAULT;
		break;
	case JZFB_SET_DITHER:
		if (copy_from_user(&enh.dither, argp, sizeof(struct enh_dither))) {
			dev_err(info->dev, "copy dither from user error");
			return -EFAULT;
		} else {
			jzfb_set_dither(info, &enh.dither);
		}
		break;
	default:
		dev_info(info->dev, "Unknown ioctl 0x%x\n", cmd);
		break;
	}

	return 0;
}
