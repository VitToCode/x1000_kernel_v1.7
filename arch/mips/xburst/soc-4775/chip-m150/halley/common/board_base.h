#ifndef __BOARD_BASE_H__
#define __BOARD_BASE_H__

#include <board.h>


#ifdef CONFIG_JZ_LED_RGB
extern struct platform_device jz_led_RGB;
#endif

#ifdef CONFIG_JZ4775_INTERNAL_CODEC
extern struct snd_codec_data codec_data;
#endif












#endif
