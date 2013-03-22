#ifndef __BOARD_H__
#define __BOARD_H__
#include <gpio.h>
#include <soc/gpio.h>


#ifndef CONFIG_BOARD_NAME
#define CONFIG_BOARD_NAME "mensa"
#endif

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
/* panel and bl platform device */
#ifdef CONFIG_LCD_KFM701A21_1A
extern struct platform_device kfm701a21_1a_device;
#endif

/**
 * lcd gpio
 **/
#define GPIO_LCD_PWM		GPIO_PE(1)
#define GPIO_LCD_DISP		GPIO_PB(30)

/**
 * TP gpio
 **/
#define GPIO_TP_WAKE		GPIO_PB(28)
#define GPIO_TP_INT		GPIO_PB(29)

#define GPIO_HP_MUTE		-1	/*hp mute gpio*/
#define GPIO_HP_MUTE_LEVEL		-1		/*vaild level*/

#define GPIO_SPEAKER_EN			-1/*speaker enable gpio*/
#define GPIO_SPEAKER_EN_LEVEL	-1

#define GPIO_HANDSET_EN		  -1		/*handset enable gpio*/
#define GPIO_HANDSET_EN_LEVEL -1

#define	GPIO_HP_DETECT		GPIO_PA(17)	/*hp detect gpio*/
#define GPIO_HP_INSERT_LEVEL    1	
#define GPIO_MIC_SELECT		-1		/*mic select gpio*/
#define GPIO_BUILDIN_MIC_LEVEL	-1		/*builin mic select level*/
#define GPIO_MIC_DETECT		-1
#define GPIO_MIC_INSERT_LEVEL -1
#define GPIO_MIC_DETECT_EN		-1  /*mic detect enable gpio*/
#define GPIO_MIC_DETECT_EN_LEVEL	-1 /*mic detect enable gpio*/


/**
 * KEY gpio
 **/
#define GPIO_HOME		GPIO_PG(15)
#define GPIO_BACK		GPIO_PD(19)
#define GPIO_VOLUMEDOWN		GPIO_PD(17)
#define GPIO_VOLUMEUP		GPIO_PD(18)
#define GPIO_ENDCALL            GPIO_PA(30)

#define ACTIVE_LOW_HOME		1
#define ACTIVE_LOW_BACK		1
#define ACTIVE_LOW_ENDCALL      1

#if defined(CONFIG_NAND)
#define ACTIVE_LOW_VOLUMEDOWN	0
#define ACTIVE_LOW_VOLUMEUP	1
#else
#define ACTIVE_LOW_VOLUMEDOWN	1
#define ACTIVE_LOW_VOLUMEUP	0
#endif

/**
 * USB detect pin
 **/
#define GPIO_USB_DETE                   GPIO_PA(16)

/**
 * sound platform data
 **/
extern struct snd_codec_data codec_data;

extern struct platform_device backlight_device;

#ifdef CONFIG_LCD_BYD_BM8766U
extern struct platform_device byd_bm8766u_device;
#endif

/**
* nand platform data
**/
extern struct platform_nand_data jz_nand_chip_data;

#endif /* __BOARD_H__ */
