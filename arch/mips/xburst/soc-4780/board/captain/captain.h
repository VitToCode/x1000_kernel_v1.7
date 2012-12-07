#ifndef __CAPTAIN_H__
#define __CAPTAIN_H__
#include <gpio.h>

/**
 * mmc platform data
 **/
extern struct jzmmc_platform_data captain_inand_pdata;
extern struct jzmmc_platform_data captain_tf_pdata;
extern struct jzmmc_platform_data captain_sdio_pdata;

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
#ifdef CONFIG_LCD_KD50G2_40NM_A2
extern struct platform_device kd50g2_40nm_a2_device;
#endif

extern struct platform_device captain_backlight_device;

/**
 * sound platform data
 **/
extern struct snd_codec_data codec_data;

/**
 * audio gpio
 **/
#define GPIO_HP_MUTE		-1	//GPIO_PD(13)	/*hp mute gpio*/
#define GPIO_HP_MUTE_LEVEL	-1	//1		/*vaild level*/

#define GPIO_SPEAKER_EN		-1	//GPIO_PE(6)	/*speaker enable gpio*/
#define GPIO_SPEAKER_EN_LEVEL	-1	//1

#define	GPIO_HP_DETECT		GPIO_PE(7)	/*hp detect gpio*/
#define GPIO_HP_INSERT_LEVEL	1

#define GPIO_MIC_DETECT		-1		/*mic detect gpio*/
#define GPIO_MIC_INSERT_LEVEL	-1

#define GPIO_MIC_SELECT		-1		/*mic select gpio*/
#define GPIO_HP_MIC_LEVEL	-1		/*headset mic select level*/

/**
 * nand platform data
 **/

#ifdef CONFIG_NAND_JZ4780
extern struct platform_nand_data jz_nand_chip_data;
#endif

/**
 * tsc gpio interface
 **/
#define GPIO_CTP_WAKE_UP		GPIO_PD(10)
#define GPIO_CTP_IRQ			GPIO_PE(8)

/**
 * g sensor gpio interface
 **/
#define GPIO_MMA8452_INT1		-1 //GPIO_PF(9)
#define GPIO_LIS3DH_INT1		-1 //GPIO_PF(9)

/**
 * keyboard gpio interface
 **/
#define GPIO_BACK			GPIO_PF(9)
#define ACTIVE_LOW_BACK			1

#define GPIO_MENU			GPIO_PF(10)
#define ACTIVE_LOW_MENU			1

#define GPIO_ENDCALL			GPIO_PA(30)
#define ACTIVE_LOW_ENDCALL		1

#define GPIO_VOLUMEUP			GPIO_PD(18)
#define ACTIVE_LOW_VOLUMEUP		1

#define GPIO_VOLUMEDOWN			GPIO_PD(17)
#define ACTIVE_LOW_VOLUMEDOWN		1

#define GPIO_SP0838_EN			GPIO_PB(18)
#define GPIO_SP0838_RST			GPIO_PB(26)

/**
 * usb detect pin
 **/
#define GPIO_USB_DETE			GPIO_PF(12)
/**
 * pmem information
 **/
#define JZ_PMEM_CAMERA_BASE	0x5f000000
#define JZ_PMEM_CAMERA_SIZE	0x01000000

/**
 * motor gpio
 */
#define GPIO_MOTOR_PIN			-1	//GPIO_PB(25) /* PB25 */


// #define GPIO_HOME
// #define ACTIVE_LOW_HOME			1
// #define GPIO_CALL
// #define ACTIVE_LOW_CALL			1
#endif /* __CAPTAIN_H__ */