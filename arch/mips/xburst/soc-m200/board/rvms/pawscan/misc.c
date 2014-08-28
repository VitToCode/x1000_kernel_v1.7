#include <linux/platform_device.h>
#include <linux/power/jz_battery.h>
#include <linux/power/li_ion_charger.h>
#include <linux/jz_adc.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/tsc.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/android_pmem.h>
#include <linux/interrupt.h>
#include <linux/jz_dwc.h>
#include <linux/delay.h>
#include <mach/jzsnd.h>
#include <mach/platform.h>
#include <mach/jz_efuse.h>
#include <gpio.h>
#include "board.h"
#include <mach/jz_dsim.h>

#if defined(CONFIG_USB_DWC2) || defined(CONFIG_USB_DWC_OTG)
#if defined(GPIO_USB_ID) && defined(GPIO_USB_ID_LEVEL)
struct jzdwc_pin dwc2_id_pin = {
	.num = GPIO_USB_ID,
	.enable_level = GPIO_USB_ID_LEVEL,
};
#endif

#if defined(GPIO_USB_DETE) && defined(GPIO_USB_DETE_LEVEL)
struct jzdwc_pin dwc2_dete_pin = {
	.num = GPIO_USB_DETE,
	.enable_level = GPIO_USB_DETE_LEVEL,
};
#endif

#if defined(GPIO_USB_DRVVBUS) && defined(GPIO_USB_DRVVBUS_LEVEL) && !defined(USB_DWC2_DRVVBUS_FUNCTION_PIN)
struct jzdwc_pin dwc2_drvvbus_pin = {
	.num = GPIO_USB_DRVVBUS,
	.enable_level = GPIO_USB_DRVVBUS_LEVEL,
};
#endif
#endif /*CONFIG_USB_DWC2 || CONFIG_USB_DWC_OTG*/

#if (defined(CONFIG_I2C_GPIO) || defined(CONFIG_I2C0_V12_JZ) || defined(CONFIG_I2C0_DMA_V12))
static struct i2c_board_info jz_i2c0_devs[] __initdata = {
};
#endif

#if (defined(CONFIG_I2C_GPIO) || defined(CONFIG_I2C1_V12_JZ))

static struct i2c_board_info jz_i2c1_devs[] __initdata = {
};
#endif  /*I2C1*/

	/*
	 * define gpio i2c,if you use gpio i2c,
	 * please enable gpio i2c and disable i2c controller
	 */
#ifdef CONFIG_I2C_GPIO
#define DEF_GPIO_I2C(NO,GPIO_I2C_SDA,GPIO_I2C_SCK)			\
	static struct i2c_gpio_platform_data i2c##NO##_gpio_data = {	\
		.sda_pin	= GPIO_I2C_SDA,				\
		.scl_pin	= GPIO_I2C_SCK,				\
	};								\
	static struct platform_device i2c##NO##_gpio_device = {     	\
		.name	= "i2c-gpio",					\
		.id	= NO,						\
		.dev	= { .platform_data = &i2c##NO##_gpio_data,},	\
	};

#ifndef CONFIG_I2C0_V12_JZ
DEF_GPIO_I2C(0,GPIO_PD(30),GPIO_PD(31));
#endif
#ifndef CONFIG_I2C1_V12_JZ
DEF_GPIO_I2C(1,GPIO_PA(12),GPIO_PA(13));
#endif
#endif /*CONFIG_I2C_GPIO*/


#ifdef CONFIG_MFD_JZ_SADC_V12
#ifdef CONFIG_JZ_BATTERY
static struct jz_battery_info  dorado_battery_info = {
	.max_vol        = 4050,
	.min_vol        = 3600,
	.usb_max_vol    = 4100,
	.usb_min_vol    = 3760,
	.ac_max_vol     = 4100,
	.ac_min_vol     = 3760,
	.battery_max_cpt = 3000,
	.ac_chg_current = 800,
	.usb_chg_current = 400,
};
#endif
static struct jz_adc_platform_data adc_platform_data;
#endif //CONFIG_MFD_JZ_SADC_V12

static int __init board_init(void)
{
	/* ADC */
#ifdef CONFIG_MFD_JZ_SADC_V12
#ifdef CONFIG_JZ_BATTERY
	adc_platform_data.battery_info = dorado_battery_info;
#endif
	jz_device_register(&jz_adc_device,&adc_platform_data);
#endif //CONFIG_MFD_JZ_SADC_V12

	/* li-ion charger */
#ifdef CONFIG_CHARGER_LI_ION
	platform_device_register(&jz_li_ion_charger_device);
#endif
	/* VPU */
#ifdef CONFIG_JZ_VPU_V12
	platform_device_register(&jz_vpu_device);
#endif
#ifdef CONFIG_BCM_PM_CORE
	platform_device_register(&bcm_power_platform_device);
#endif

/*i2c*/
#ifdef CONFIG_I2C_GPIO
#ifndef CONFIG_I2C0_V12_JZ
	platform_device_register(&i2c0_gpio_device);
#endif
#ifndef CONFIG_I2C1_V12_JZ
	platform_device_register(&i2c1_gpio_device);
#endif

#endif	/* CONFIG_I2C_GPIO */

#ifdef CONFIG_I2C0_V12_JZ
	platform_device_register(&jz_i2c0_device);
#endif

#ifdef CONFIG_I2C1_V12_JZ
	platform_device_register(&jz_i2c1_device);
#endif

#ifdef CONFIG_I2C2_V12_JZ
	platform_device_register(&jz_i2c2_device);
#endif

#ifdef CONFIG_I2C3_V12_JZ
	platform_device_register(&jz_i2c3_device);
#endif

#if (defined(CONFIG_I2C_GPIO) || defined(CONFIG_I2C0_V12_JZ) || defined(CONFIG_I2C0_DMA_V12))
	i2c_register_board_info(0, jz_i2c0_devs, ARRAY_SIZE(jz_i2c0_devs));
#endif

#if (defined(CONFIG_I2C_GPIO) || defined(CONFIG_I2C1_V12_JZ) || defined(CONFIG_I2C1_DMA_V12))
	i2c_register_board_info(1, jz_i2c1_devs, ARRAY_SIZE(jz_i2c1_devs));
#endif

/*dma*/
#ifdef CONFIG_XBURST_DMAC
	platform_device_register(&jz_pdma_device);
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
#ifdef CONFIG_USB_OHCI_HCD
	platform_device_register(&jz_ohci_device);
#endif
#ifdef CONFIG_USB_EHCI_HCD
	platform_device_register(&jz_ehci_device);
#endif
#ifdef CONFIG_USB_DWC2
	platform_device_register(&jz_dwc_otg_device);
#endif

/* msc */
#ifndef CONFIG_NAND
#ifdef CONFIG_JZMMC_V12_MMC0
	jz_device_register(&jz_msc0_device, &inand_pdata);
#endif
#ifdef CONFIG_JZMMC_V12_MMC1
	jz_device_register(&jz_msc1_device, &sdio_pdata);
#endif
#else
#ifdef CONFIG_JZMMC_V12_MMC0
	jz_device_register(&jz_msc0_device, &tf_pdata);
#endif
#ifdef CONFIG_JZMMC_V12_MMC1
	jz_device_register(&jz_msc1_device, &sdio_pdata);
#endif
#endif

/* ethnet */
#ifdef CONFIG_JZ4775_MAC
	platform_device_register(&jz4775_mii_bus);
	platform_device_register(&jz4775_mac_device);
#endif

/* audio */
#ifdef CONFIG_SOUND_JZ_I2S_V12
	jz_device_register(&jz_i2s_device,&i2s_data);
	jz_device_register(&jz_mixer0_device,&snd_mixer0_data);
#endif
#ifdef CONFIG_SOUND_JZ_SPDIF_V12
	jz_device_register(&jz_spdif_device,&spdif_data);
	jz_device_register(&jz_mixer2_device,&snd_mixer2_data);
#endif
#ifdef CONFIG_SOUND_JZ_DMIC_V12
	jz_device_register(&jz_dmic_device,&dmic_data);
	jz_device_register(&jz_mixer3_device,&snd_mixer3_data);
#endif

#ifdef CONFIG_SOUND_JZ_PCM_V12
	jz_device_register(&jz_pcm_device,&pcm_data);
	jz_device_register(&jz_mixer1_device,&snd_mixer1_data);
#endif
#ifdef CONFIG_JZ_INTERNAL_CODEC_V12
	jz_device_register(&jz_codec_device, &codec_data);
#endif

/* ovisp */
#ifdef CONFIG_VIDEO_OVISP
       jz_device_register(&ovisp_device_camera, &ovisp_camera_info);
#endif
#ifdef CONFIG_RTC_DRV_JZ
	platform_device_register(&jz_rtc_device);
#endif

/* efuse */
#ifdef CONFIG_JZ_EFUSE_V12
       jz_device_register(&jz_efuse_device, &jz_efuse_pdata);
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
