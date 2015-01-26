#ifndef __BOARD_H__
#define __BOARD_H__
#include <gpio.h>
#include <soc/gpio.h>


/* LED */
#ifdef CONFIG_JZ_LED_RGB
#define GPIO_JZ_LED_RGB_R    GPIO_PC(5)
#define GPIO_JZ_LED_RGB_G    GPIO_PC(4)
#define GPIO_JZ_LED_RGB_B    GPIO_PC(13)
#endif

/* ****************************GPIO AUDIO START****************************** */
#define GPIO_HP_MUTE        -1  /*hp mute gpio*/
#define GPIO_HP_MUTE_LEVEL  -1  /*vaild level*/

#define GPIO_SPEAKER_EN    GPIO_PC(7)         /*speaker enable gpio*/
#define GPIO_SPEAKER_EN_LEVEL   1

#define GPIO_HANDSET_EN     -1  /*handset enable gpio*/
#define GPIO_HANDSET_EN_LEVEL   -1

#define GPIO_HP_DETECT  -1      /*hp detect gpio*/
#define GPIO_HP_INSERT_LEVEL    1
#define GPIO_MIC_SELECT     -1  /*mic select gpio*/
#define GPIO_BUILDIN_MIC_LEVEL  -1  /*builin mic select level*/
#define GPIO_MIC_DETECT     -1
#define GPIO_MIC_INSERT_LEVEL   -1
#define GPIO_MIC_DETECT_EN  -1  /*mic detect enable gpio*/
#define GPIO_MIC_DETECT_EN_LEVEL -1 /*mic detect enable gpio*/

#define HP_SENSE_ACTIVE_LEVEL   1
#define HOOK_ACTIVE_LEVEL       -1
/* ****************************GPIO AUDIO END******************************** */


#endif /* __BOARD_H__ */
