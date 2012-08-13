#ifndef __WARRIOR_H__
#define __WARRIOR_H__
#include <gpio.h>

/**
 * mmc platform data
 **/
extern struct jzmmc_platform_data warrior_inand_pdata;
extern struct jzmmc_platform_data warrior_tf_pdata;
extern struct jzmmc_platform_data warrior_sdio_pdata;

/**
 * lcd platform data
 **/
#ifdef CONFIG_FB_JZ4780_LCDC0
extern struct jzfb_platform_data jzfb0_pdata;
#endif
#ifdef CONFIG_FB_JZ4780_LCDC1
extern struct jzfb_platform_data jzfb1_pdata;
#endif

/**
 * lcd platform device
 **/
#ifdef CONFIG_LCD_AUO_A043FL01V2
extern struct platform_device auo_a043fl01v2_device;
#endif
#ifdef CONFIG_LCD_AT070TN93
extern struct platform_device at070tn93_device;
#endif
extern struct platform_device warrior_backlight_device;

/**
 * sound platform data
 **/
extern struct snd_codec_data codec_data;

/**
 * nand platform data
 **/
extern struct platform_nand_data jz_nand_chip_data;

#endif /* __WARRIOR_H__ */
