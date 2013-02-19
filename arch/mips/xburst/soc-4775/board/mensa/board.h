#ifndef __BOARD_H__
#define __BOARD_H__
#include <gpio.h>

/* MSC GPIO Definition */
#define GPIO_SD0_VCC_EN_N	GPIO_PB(3)
#define GPIO_SD0_CD_N		GPIO_PB(2)

extern struct jzmmc_platform_data inand_pdata;
extern struct jzmmc_platform_data tf_pdata;
extern struct jzmmc_platform_data sdio_pdata;

#ifdef CONFIG_FB_JZ4780_LCDC0
extern struct jzfb_platform_data jzfb0_pdata;
#endif
#ifdef CONFIG_FB_JZ4780_LCDC1
extern struct jzfb_platform_data jzfb1_pdata;
#endif

/**
 *audio gpio
 **/
#define GPIO_I2S_MUTE		GPIO_PB(30)

#define GPIO_SPEAKER_SHUTDOWN	GPIO_PB(3)

#define GPIO_HP_DETECT		GPIO_PA(17)

/**
 * sound platform data
 **/
extern struct snd_codec_data codec_data;

extern struct platform_device backlight_device;

#ifdef CONFIG_LCD_AUO_A043FL01V2
extern struct platform_device auo_a043fl01v2_device;
#endif
#ifdef CONFIG_LCD_AT070TN93
extern struct platform_device at070tn93_device;
#endif

/**
* nand platform data
**/
extern struct platform_nand_data jz_nand_chip_data;

#endif /* __BOARD_H__ */
