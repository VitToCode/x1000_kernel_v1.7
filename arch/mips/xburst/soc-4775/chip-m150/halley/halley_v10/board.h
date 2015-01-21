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


#endif /* __BOARD_H__ */
