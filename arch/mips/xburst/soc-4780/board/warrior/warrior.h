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

#ifdef CONFIG_NAND_JZ4780
extern struct platform_nand_data jz_nand_chip_data;
#endif

/**
 * tsc gpio interface
 **/
#define GPIO_CTP_IRQ			GPIO_PF(19)
#define GPIO_CTP_WAKE_UP		GPIO_PF(18)

/**
 * g sensor gpio interface
 **/
#define GPIO_MMA8452_INT1		GPIO_PF(9)
#define GPIO_LIS3DH_INT1		GPIO_PF(9)

/**
 * keyboard gpio interface
 **/
#define GPIO_BACK			GPIO_PB(4)
#define ACTIVE_LOW_BACK			1

#define GPIO_MENU			GPIO_PB(3)
#define ACTIVE_LOW_MENU			1

#define GPIO_ENDCALL			GPIO_PA(30)
#define ACTIVE_LOW_ENDCALL		1

#define GPIO_VOLUMEUP			GPIO_PD(17)
#define ACTIVE_LOW_VOLUMEUP		1

#define GPIO_VOLUMEDOWN			GPIO_PD(18)
#define ACTIVE_LOW_VOLUMEDOWN		1

// #define GPIO_HOME
// #define ACTIVE_LOW_HOME			1
// #define GPIO_CALL
// #define ACTIVE_LOW_CALL			1
#endif /* __WARRIOR_H__ */
