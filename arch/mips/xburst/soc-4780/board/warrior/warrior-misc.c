#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/gsensor.h>
#include <linux/tsc.h>

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

#if (defined(CONFIG_SENSORS_MMA8452) && defined(CONFIG_I2C1_JZ4780))
static struct gsensor_platform_data mma8452_platform_pdata = {
	.gpio_int = GPIO_MMA8452_INT1,
	.poll_interval = 100,
	.min_interval = 40,
	.max_interval = 200,
	.g_range = GSENSOR_2G,
	.axis_map_x = 1,
	.axis_map_y = 0,
	.axis_map_z = 2,
	.negate_x = 1,
	.negate_y = 0,
	.negate_z = 1,
};
#endif

#if (defined(CONFIG_SENSORS_LIS3DH) && defined(CONFIG_I2C1_JZ4780))
static struct gsensor_platform_data lis3dh_platform_data = { 
	.gpio_int = GPIO_LIS3DH_INT1, 
	.poll_interval = 100,
       	.min_interval = 40,
	.max_interval = 200,
	.g_range = GSENSOR_2G,
	.axis_map_x = 1,
	.axis_map_y = 0,
	.axis_map_z = 2,							        
	.negate_x = 1,
	.negate_y = 0,
	.negate_z = 1,
};
#endif

#if (defined(CONFIG_JZ4780_SUPPORT_TSC) && defined(CONFIG_I2C3_JZ4780))
static struct jztsc_pin warrior_tsc_gpio[] = {
	[0] = {GPIO_CTP_IRQ,		HIGH_ENABLE},
	[1] = {GPIO_CTP_WAKE_UP,	HIGH_ENABLE},
};

static struct jztsc_platform_data warrior_tsc_pdata = {
	.gpio		= warrior_tsc_gpio,
};
#endif

#ifdef CONFIG_I2C1_JZ4780 /*I2C1*/
static struct i2c_board_info warrior_i2c1_devs[] __initdata = {
#ifdef CONFIG_SENSORS_MMA8452
	{
		I2C_BOARD_INFO("gsensor_mma8452",0x1c),
		.irq = 0,
		.platform_data = &mma8452_platform_pdata,
	},
#endif
#ifdef CONFIG_SENSORS_LIS3DH
	{
	       	I2C_BOARD_INFO("gsensor_lis3dh",0x18),
		.irq = 0,
		.platform_data = &lis3dh_platform_data,
	},						        
#endif
};
#endif	/*I2C1*/

#ifdef CONFIG_I2C3_JZ4780 /*I2C3*/
static struct i2c_board_info warrior_i2c3_devs[] __initdata = {
#ifdef CONFIG_JZ4780_SUPPORT_TSC
	{
		I2C_BOARD_INFO("ft5x0x_tsc", 0x36),
		.irq = 0,
		.platform_data	= &warrior_tsc_pdata,
	},
#endif
};
#endif /*I2C3*/

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

#ifdef CONFIG_JZCIM
	platform_device_register(&jz_cim_device);
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

#ifdef CONFIG_I2C1_JZ4780
	i2c_register_board_info(1, warrior_i2c1_devs, ARRAY_SIZE(warrior_i2c1_devs));
#endif

#ifdef CONFIG_I2C3_JZ4780
	i2c_register_board_info(3, warrior_i2c3_devs, ARRAY_SIZE(warrior_i2c3_devs));
#endif	

#ifdef CONFIG_RTC_DRV_JZ4780
	platform_device_register(&jz4780_device_rtc);
#endif
	return 0;

#ifdef CONFIG_KEYBOARD_GPIO
	platform_device_register(&jz_button_device);
#endif

}


arch_initcall(warrior_board_init);
