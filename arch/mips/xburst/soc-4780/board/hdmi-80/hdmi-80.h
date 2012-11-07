#ifndef __HDMI_80_H__
#define __HDMI_80_H__
#include <gpio.h>

/**
 * mmc platform data
 **/
extern struct jzmmc_platform_data hdmi_80_inand_pdata;
extern struct jzmmc_platform_data hdmi_80_tf_pdata;
extern struct jzmmc_platform_data hdmi_80_sdio_pdata;

/**
 * lcd platform data
 **/
#ifdef CONFIG_FB_JZ4780_LCDC0
extern struct jzfb_platform_data jzfb0_hdmi_pdata;
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
extern struct platform_device hdmi_80_backlight_device;

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

#define GPIO_MENU			GPIO_PD(17)
#define ACTIVE_LOW_MENU		0

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
#endif /* __HDMI_80_H__ */
