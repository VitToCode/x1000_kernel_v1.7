/*
 * [board]-misc.c - This file defines most of devices on the board.
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/platform_device.h>
#include <linux/gpio_keys.h>

#include <mach/platform.h>
#include <mach/jzsnd.h>
#include <mach/jzmmc.h>
#include <gpio.h>

#include "warrior.h"

#ifdef CONFIG_KEYBOARD_GPIO
static struct gpio_keys_button board_buttons[] = {
#ifdef GPIO_CALL
	{
		.gpio		= GPIO_CALL,
		.code   	= KEY_SEND,
		.desc		= "call key",
		.active_low	= ACTIVE_LOW_CALL,
	},
#endif
#ifdef GPIO_HOME
	{
		.gpio		= GPIO_HOME,
		.code   	= KEY_HOME,
		.desc		= "home key",
		.active_low	= ACTIVE_LOW_HOME,
	},
#endif
#ifdef GPIO_BACK
	{
		.gpio		= GPIO_BACK,
		.code   	= KEY_BACK,
		.desc		= "back key",
		.active_low	= ACTIVE_LOW_BACK,
	},
#endif
#ifdef GPIO_MENU
	{
		.gpio		= GPIO_MENU,
		.code   	= KEY_MENU,
		.desc		= "menu key",
		.active_low	= ACTIVE_LOW_MENU,
	},
#endif
#ifdef GPIO_ENDCALL
	{
		.gpio		= GPIO_ENDCALL,
		.code   	= KEY_END,
		.desc		= "end call key",
		.active_low	= ACTIVE_LOW_ENDCALL,
		.wakeup		= 1,
	},
#endif
#ifdef GPIO_VOLUMEDOWN
	{
		.gpio		= GPIO_VOLUMEDOWN,
		.code   	= KEY_VOLUMEDOWN,
		.desc		= "volum down key",
		.active_low	= ACTIVE_LOW_VOLUMEDOWN,
	},
	{
		.gpio		= GPIO_VOLUMEUP,
		.code   	= KEY_VOLUMEUP,
		.desc		= "volum up key",
		.active_low	= ACTIVE_LOW_VOLUMEUP,
	},
#endif
};
static struct gpio_keys_platform_data board_button_data = {
	.buttons	= board_buttons,
	.nbuttons	= ARRAY_SIZE(board_buttons),
};

static struct platform_device jz_button_device = {
	.name		= "gpio-keys",
	.id		= -1,
	.num_resources	= 0,
	.dev		= {
		.platform_data	= &board_button_data,
	}
};
#endif

static int __init warrior_board_init(void)
{
/* dma */
#ifdef CONFIG_XBURST_DMAC
	platform_device_register(&jz_pdma_device);
#endif
/* i2c */
#ifdef CONFIG_I2C0_JZ4780
	platform_device_register(&jz_i2c0_device);
#endif
#ifdef CONFIG_I2C1_JZ4780
	platform_device_register(&jz_i2c1_device);
#endif
#ifdef CONFIG_I2C2_JZ4780
	platform_device_register(&jz_i2c2_device);
#endif
#ifdef CONFIG_I2C3_JZ4780
	platform_device_register(&jz_i2c3_device);
#endif
#ifdef CONFIG_I2C4_JZ4780
	platform_device_register(&jz_i2c4_device);
#endif
/* ipu */
#ifdef CONFIG_JZ4780_IPU
	platform_device_register(&jz_ipu0_device);
#endif
#ifdef CONFIG_JZ4780_IPU
	platform_device_register(&jz_ipu1_device);
#endif
/* mmc */
#ifdef CONFIG_MMC0_JZ4780
	jz_device_register(&jz_msc0_device, &warrior_inand_pdata);
#endif
#ifdef CONFIG_MMC1_JZ4780
	jz_device_register(&jz_msc1_device, &warrior_sdio_pdata);
#endif
#ifdef CONFIG_MMC2_JZ4780
	jz_device_register(&jz_msc2_device, &warrior_tf_pdata);
#endif
/* sound */
#ifdef CONFIG_SOUND_I2S0_JZ47XX
	jz_device_register(&jz_i2s0_device,&i2s0_data);
#endif
#ifdef CONFIG_SOUND_I2S1_JZ47XX
	jz_device_register(&jz_i2s1_device,&i2s1_data);
#endif
#ifdef CONFIG_SOUND_PCM0_JZ47XX
	jz_device_register(&jz_pcm0_device,&pcm0_data);
#endif
#ifdef CONFIG_SOUND_PCM1_JZ47XX
	jz_device_register(&jz_pcm1_device,&pcm1_data);
#endif
#ifdef CONFIG_JZ4780_INTERNAL_CODEC
	jz_device_register(&jz_codec_device, &codec_data);
#endif
/* panel and bl */
#ifdef CONFIG_LCD_AUO_A043FL01V2
	platform_device_register(&auo_a043fl01v2_device);
#endif
#ifdef CONFIG_LCD_AT070TN93
	platform_device_register(&at070tn93_device);
#endif
#ifdef CONFIG_BACKLIGHT_PWM
	platform_device_register(&warrior_backlight_device);
#endif
/* lcdc framebuffer*/
#ifdef CONFIG_FB_JZ4780_LCDC0
	jz_device_register(&jz_fb0_device, &jzfb0_pdata);
#endif
#ifdef CONFIG_FB_JZ4780_LCDC1
	jz_device_register(&jz_fb1_device, &jzfb1_pdata);
#endif
/* uart */
#ifdef CONFIG_SERIAL_JZ47XX_UART0
	platform_device_register(&jz_uart0_device);
#endif
#ifdef CONFIG_SERIAL_JZ47XX_UART1
	platform_device_register(&jz_uart1_device);
#endif
#ifdef CONFIG_SERIAL_JZ47XX_UART2
	platform_device_register(&jz_uart2_device);
#endif
#ifdef CONFIG_SERIAL_JZ47XX_UART3
	platform_device_register(&jz_uart3_device);
#endif
#ifdef CONFIG_SERIAL_JZ47XX_UART4
	platform_device_register(&jz_uart4_device);
#endif
/* camera */
#ifdef CONFIG_JZCIM
	platform_device_register(&jz_cim_device);
#endif
/* USB */
#ifdef CONFIG_USB_OHCI_HCD
	platform_device_register(&jz_ohci_device);
#endif
#ifdef CONFIG_USB_EHCI_HCD
	platform_device_register(&jz_ehci_device);
#endif
/* net */
#ifdef CONFIG_JZ_MAC
	platform_device_register(&jz_mac);
#endif
/* nand */
#ifdef CONFIG_NAND_JZ4780
	jz_device_register(&jz_nand_device, &jz_nand_chip_data);
#endif
/* hdmi */
#ifdef CONFIG_HDMI_JZ4780
	platform_device_register(&jz_hdmi);
#endif
/* rtc */
#ifdef CONFIG_RTC_DRV_JZ4780
	platform_device_register(&jz_rtc_device);
#endif
/* gpio keyboard */
#ifdef CONFIG_KEYBOARD_GPIO
	platform_device_register(&jz_button_device);
#endif
/* tcsm */
#ifdef CONFIG_JZ_TCSM_DEFAULT
	platform_device_register(&jz_tcsm_device);
#endif

	return 0;
}

arch_initcall(warrior_board_init);
