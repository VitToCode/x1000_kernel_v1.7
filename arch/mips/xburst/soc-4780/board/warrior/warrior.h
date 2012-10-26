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
#ifdef CONFIG_LCD_KR070LA0S_270
extern struct platform_device kr070la0s_270_device;
#endif
#ifdef CONFIG_LCD_EK070TN93
extern struct platform_device ek070tn93_device;
#endif
extern struct platform_device warrior_backlight_device;

/**
 * sound platform data
 **/
extern struct snd_codec_data codec_data;

/**
 * headphone and speaker mute gpio
 **/

#define GPIO_HP_MUTE	GPIO_PD(13)
#define GPIO_SPEAKER_EN GPIO_PE(6)

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
#define GPIO_BACK			GPIO_PB(5)
#define ACTIVE_LOW_BACK			1

#define GPIO_MENU			GPIO_PB(4)
#define ACTIVE_LOW_MENU			1

#define GPIO_ENDCALL			GPIO_PA(30)
#define ACTIVE_LOW_ENDCALL		1

#ifndef CONFIG_NAND_JZ4780
#define GPIO_VOLUMEUP			GPIO_PD(18)
#define ACTIVE_LOW_VOLUMEUP		0

#define GPIO_VOLUMEDOWN			GPIO_PD(17)
#define ACTIVE_LOW_VOLUMEDOWN		1
#else
#define GPIO_VOLUMEUP			GPIO_PD(18)
#define ACTIVE_LOW_VOLUMEUP		1

#define GPIO_VOLUMEDOWN			GPIO_PD(17)
#define ACTIVE_LOW_VOLUMEDOWN		0
#endif

#define GPIO_SP0838_EN			GPIO_PB(18)
#define GPIO_SP0838_RST			GPIO_PB(26)

/**
 * motor gpio
 */
#define GPIO_MOTOR_PIN			GPIO_PB(25) /* PB25 */


// #define GPIO_HOME
// #define ACTIVE_LOW_HOME			1
// #define GPIO_CALL
// #define ACTIVE_LOW_CALL			1
#endif /* __WARRIOR_H__ */
