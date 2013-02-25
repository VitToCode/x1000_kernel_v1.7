#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/tsc.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <mach/platform.h>
#include <mach/jzsnd.h>
#include <mach/jzmmc.h>
#include <mach/jzssi.h>
#include <gpio.h>
#include <linux/jz_dwc.h>

#include "board.h"

#ifdef CONFIG_KEYBOARD_GPIO
static struct gpio_keys_button board_buttons[] = {
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
#ifdef GPIO_VOLUMEDOWN
	{
		.gpio		= GPIO_VOLUMEDOWN,
		.code   	= KEY_VOLUMEDOWN,
		.desc		= "volum down key",
		.active_low	= ACTIVE_LOW_VOLUMEDOWN,
	},
#endif
#ifdef GPIO_VOLUMEUP
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

#ifdef CONFIG_JZ4775_SUPPORT_TSC
static struct jztsc_pin mensa_tsc_gpio[] = { 
	        [0] = {GPIO_TP_INT,         LOW_ENABLE},
		[1] = {GPIO_TP_WAKE,        HIGH_ENABLE},
};

static struct jztsc_platform_data mensa_tsc_pdata = { 
	        .gpio           = mensa_tsc_gpio,
		.x_max          = 800,
		.y_max          = 480,
};

#ifdef CONFIG_TOUCHSCREEN_GWTC9XXXB
static struct i2c_board_info mensa_i2c0_devs[] __initdata = { 
		        {   
				I2C_BOARD_INFO("gwtc9xxxb_ts", 0x05),
				.platform_data = &mensa_tsc_pdata,
			},  
	};
#endif
#endif

#if (defined(CONFIG_USB_DWC2) || defined(CONFIG_USB_DWC_OTG)) && defined(GPIO_USB_DETE)
struct jzdwc_pin dete_pin = {
        .num                            = GPIO_USB_DETE,
        .enable_level                   = HIGH_ENABLE,
};
#endif

static int __init board_init(void)
{
/* dma */
#ifdef CONFIG_XBURST_DMAC
	platform_device_register(&jz_pdma_device);
#endif
/* i2c */
#ifdef CONFIG_I2C0_JZ4775
	platform_device_register(&jz_i2c0_device);
#endif
#ifdef CONFIG_I2C1_JZ4775
	platform_device_register(&jz_i2c1_device);
#endif
#ifdef CONFIG_I2C2_JZ4775
	platform_device_register(&jz_i2c2_device);
#endif

/* mmc */
#ifdef CONFIG_MMC0_JZ4775
	jz_device_register(&jz_msc0_device, &tf_pdata);
#endif
#ifdef CONFIG_MMC1_JZ4775
	jz_device_register(&jz_msc1_device, &sdio_pdata);
#endif

/* sound */
#ifdef CONFIG_SOUND_I2S0_JZ47XX
	jz_device_register(&jz_i2s_device,&i2s_data);
#endif
#ifdef CONFIG_SOUND_PCM0_JZ47XX
	jz_device_register(&jz_pcm_device,&pcm_data);
#endif
#ifdef CONFIG_JZ4780_INTERNAL_CODEC
	jz_device_register(&jz_codec_device, &codec_data);
#endif

/* panel and bl */
//#ifdef CONFIG_LCD_BYD_BM8766U
//	platform_device_register(&byd_bm8766u_device);
//#endif
#ifdef CONFIG_BACKLIGHT_PWM
	platform_device_register(&backlight_device);
#endif

/* lcdc framebuffer*/
#ifdef CONFIG_FB_JZ4780_LCDC0
	jz_device_register(&jz_fb0_device, &jzfb0_pdata);
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

#ifdef CONFIG_JZ_CIM
	platform_device_register(&jz_cim_device);
#endif

/* x2d */
#ifdef CONFIG_JZ_X2D
        platform_device_register(&jz_x2d_device);
#endif

#ifdef CONFIG_USB_OHCI_HCD
	platform_device_register(&jz_ohci_device);
#endif

#ifdef CONFIG_USB_EHCI_HCD
	platform_device_register(&jz_ehci_device);
#endif

#ifdef CONFIG_JZ_MAC
	platform_device_register(&jz_mac);
#endif

#ifdef CONFIG_KEYBOARD_GPIO
	platform_device_register(&jz_button_device);
#endif
/* nand */
#ifdef CONFIG_NAND_JZ4780
	jz_device_register(&jz_nand_device, &jz_nand_chip_data);
#endif

#ifdef CONFIG_HDMI_JZ4780
	platform_device_register(&jz_hdmi);
#endif

#ifdef CONFIG_JZ4775_SUPPORT_TSC
	i2c_register_board_info(0, mensa_i2c0_devs, ARRAY_SIZE(mensa_i2c0_devs));
#endif
#ifdef CONFIG_RTC_DRV_JZ4780
	platform_device_register(&jz_rtc_device);
#endif


#ifdef CONFIG_SPI_JZ4780
#ifdef CONFIG_SPI0_JZ4780
       spi_register_board_info(jz_spi0_board_info, ARRAY_SIZE(jz_spi0_board_info));
       platform_device_register(&jz_ssi0_device);
       platform_device_add_data(&jz_ssi0_device, &spi0_info_cfg, sizeof(struct jz47xx_spi_info));
#endif

#ifdef CONFIG_SPI1_JZ4780
       spi_register_board_info(jz_spi1_board_info, ARRAY_SIZE(jz_spi1_board_info));
       platform_device_register(&jz_ssi1_device);
       platform_device_add_data(&jz_ssi1_device, &spi1_info_cfg, sizeof(struct jz47xx_spi_info));
#endif
#endif

#ifdef CONFIG_SPI_GPIO
       spi_register_board_info(jz_spi0_board_info, ARRAY_SIZE(jz_spi0_board_info));
       platform_device_register(&jz4780_spi_gpio_device);
#endif

#ifdef CONFIG_USB_DWC2
        platform_device_register(&jz_dwc_otg_device);
#endif
	return 0;
}

/**
 * Called by arch/mips/kernel/proc.c when 'cat /proc/cpuinfo'.
 * Android requires the 'Hardware:' field in cpuinfo to setup the init.%hardware%.rc.
 */
const char *get_board_type(void)
{
	return CONFIG_BOARD_NAME;
}

arch_initcall(board_init);
