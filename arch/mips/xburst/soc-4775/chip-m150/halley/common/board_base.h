#ifndef __BOARD_BASE_H__
#define __BOARD_BASE_H__

#include <board.h>


#ifdef CONFIG_JZ_LED_RGB
extern struct platform_device jz_led_RGB;
#endif

#ifdef CONFIG_JZ4775_INTERNAL_CODEC
extern struct snd_codec_data codec_data;
#endif

#ifdef CONFIG_MMC2_JZ4775
extern struct jzmmc_platform_data sdio_pdata;
#endif

#ifdef CONFIG_BROADCOM_RFKILL
extern struct platform_device	bt_power_device;
extern struct platform_device	bluesleep_device;
#endif

#ifdef CONFIG_USB_DWC2
extern struct platform_device   jz_dwc_otg_device;
#endif









#endif
