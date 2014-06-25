#include <linux/platform_device.h>
#include <linux/power/li-ion-charger.h>
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
#include <linux/i2c/ft6x06_ts.h>
#include "board.h"

#if defined(CONFIG_INV_MPU_IIO)
#include <linux/inv_mpu.h>
#endif

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
#ifdef GPIO_MENU
	{
		.gpio		= GPIO_MENU,
		.code   	= KEY_MENU,
		.desc		= "menu key",
		.active_low	= ACTIVE_LOW_MENU,
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

#ifdef GPIO_ENDCALL
	{
		.gpio           = GPIO_ENDCALL,
		.code           = KEY_POWER,
		.desc           = "end call key",
		.active_low     = ACTIVE_LOW_ENDCALL,
		.wakeup         = 1,
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

/* efuse */
#ifdef CONFIG_JZ_EFUSE_V10
static struct jz_efuse_platform_data jz_efuse_pdata = {
       /* supply 2.5V to VDDQ */
	.gpio_vddq_en_n = GPIO_PA(12),
};
#endif

#if (defined(CONFIG_USB_DWC2) || defined(CONFIG_USB_DWC_OTG)) &&  defined(GPIO_USB_ID)
struct jzdwc_pin dwc2_id_pin = {
	.num	      = GPIO_USB_ID,
	.enable_level = LOW_ENABLE
};
#endif

#if (defined(CONFIG_USB_DWC2) || defined(CONFIG_USB_DWC_OTG)) && defined(GPIO_USB_DETE)
struct jzdwc_pin dete_pin = {
	.num			= GPIO_USB_DETE,
	.enable_level		= HIGH_ENABLE,
};
#endif

#ifdef CONFIG_CHARGER_LI_ION
/* li-ion charger */
static struct li_ion_charger_platform_data jz_li_ion_charger_pdata = {
	.gpio_charge = GPIO_PB(1),
	.gpio_ac = GPIO_PA(13),
	.gpio_active_low = 1,
};

static struct platform_device jz_li_ion_charger_device = {
	.name = "li-ion-charger",
	.dev = {
		.platform_data = &jz_li_ion_charger_pdata,
	},
};
#endif

/*touchscreen*/
#ifdef CONFIG_JZ4785_SUPPORT_TSC

#ifdef CONFIG_TOUCHSCREEN_GWTC9XXXB
static struct jztsc_pin fpga_tsc_gpio[] = {
	[0] = {GPIO_TP_INT, LOW_ENABLE},
	[1] = {GPIO_TP_WAKE, HIGH_ENABLE},
};

static struct jztsc_platform_data fpga_tsc_pdata = {
	.gpio = fpga_tsc_gpio,
	.x_max = 800,
	.y_max = 480,
};
#endif

#ifdef CONFIG_TOUCHSCREEN_FT6X06
static struct ft6x06_platform_data ft6x06_tsc_pdata = {
		.x_max          = 300,
		.y_max          = 540,
		.va_x_max		= 300,
		.va_y_max		= 480,
		.irqflags = IRQF_TRIGGER_FALLING|IRQF_DISABLED,
		.irq = (32 * 1 + 0),
		.reset = (32 * 0 + 12),
};
#endif

#endif /*CONFIG_JZ4785_SUPPORT_TSC*/

#if defined(CONFIG_INV_MPU_IIO)
static struct regulator *inv_mpu_power_vdd = NULL;
static struct regulator *inv_mpu_power_vio = NULL;
static atomic_t inv_mpu_powered = ATOMIC_INIT(0);
static int inv_mpu_early_init(struct device *dev)
{
	int res;

	pr_info("mpuirq_init ...\n");
	if ((res = gpio_request(GPIO_GSENSOR_INT1, "mpuirq"))) {
		printk(KERN_ERR "%s -> gpio_request(%d) failed err=%d\n",__func__,GPIO_GSENSOR_INT1, res);
		res = -EBUSY;
		goto err_gpio_request;
	}

	if ((res = gpio_direction_input(GPIO_GSENSOR_INT1))) {
		printk(KERN_ERR "%s -> gpio_direction_input() failed err=%d\n",__func__,res);
		res = -EBUSY;
		goto err_gpio_set_input;
	}

	if (!inv_mpu_power_vdd) {
		inv_mpu_power_vdd = regulator_get(dev, "vcc_sensor3v3");
		if (IS_ERR(inv_mpu_power_vdd)) {
			pr_err("%s -> get regulator VDD failed\n",__func__);
			res = -ENODEV;
			goto err_vdd;
		}
	}

	if (!inv_mpu_power_vio) {
		inv_mpu_power_vio = regulator_get(dev, "vcc_sensor1v8");
		if (IS_ERR(inv_mpu_power_vio)) {
			pr_err("%s -> get regulator VIO failed\n",__func__);
			res = -ENODEV;
			goto err_vio;
		}
	}

	return 0;
err_vio:
	regulator_put(inv_mpu_power_vdd);
	inv_mpu_power_vdd = NULL;
err_vdd:
	inv_mpu_power_vio = NULL;
err_gpio_set_input:
err_gpio_request:
	return res;
}

static int inv_mpu_exit(struct device *dev)
{
	if (inv_mpu_power_vdd != NULL && !IS_ERR(inv_mpu_power_vdd)) {
		regulator_put(inv_mpu_power_vdd);
	}

	if (inv_mpu_power_vio != NULL && !IS_ERR(inv_mpu_power_vio)) {
		regulator_put(inv_mpu_power_vio);
	}

	atomic_set(&inv_mpu_powered, 0);

	return 0;
}

static int inv_mpu_power_on(void)
{
	int res;
	if (!atomic_read(&inv_mpu_powered)) {
		if (!IS_ERR(inv_mpu_power_vdd)) {
			regulator_enable(inv_mpu_power_vdd);
		} else {
			pr_err("inv mpu VDD power unavailable!\n");
			res = -ENODEV;
			goto err_vdd;
		}

		if (!IS_ERR(inv_mpu_power_vio)) {
			regulator_enable(inv_mpu_power_vio);
		} else {
			pr_err("inv mpu VIO power unavailable!\n");
			res = -ENODEV;
			goto err_vio;
		}

		atomic_set(&inv_mpu_powered, 1);

		msleep(200);
	}
	return 0;
err_vio:
	regulator_disable(inv_mpu_power_vdd);
err_vdd:
	return res;
}

static int inv_mpu_power_off(void)
{
	int res;
	if (atomic_read(&inv_mpu_powered)) {
		if (!IS_ERR(inv_mpu_power_vio)) {
			regulator_disable(inv_mpu_power_vio);
		} else {
			pr_err("inv mpu VIO power unavailable!\n");
			res = -ENODEV;
			goto err_vio;
		}

		if (!IS_ERR(inv_mpu_power_vdd)) {
			regulator_disable(inv_mpu_power_vdd);
		} else {
			pr_err("inv mpu VDD power unavailable!\n");
			res = -ENODEV;
			goto err_vdd;
		}

		atomic_set(&inv_mpu_powered, 0);
	}

	return 0;
err_vio:
err_vdd:
	return res;
}

static struct mpu_platform_data mpu9250_platform_data = {
        .int_config  = 0x90,
        .level_shifter = 0,
        .orientation = {  0,  -1,  0,
                          1,   0,  0,
                          0,   0,  1 },
        .key = { 0xec, 0x5c, 0xa6, 0x17, 0x54, 0x3, 0x42, 0x90, 0x74, 0x7e,
                 0x3a, 0x6f, 0xc, 0x2c, 0xdd, 0xb },
#if !defined(CONFIG_SENSORS_AK09911)
        .sec_slave_type = SECONDARY_SLAVE_TYPE_COMPASS,
        .sec_slave_id   = COMPASS_ID_AK8963,
        .secondary_i2c_addr = 0x0C,
	.secondary_orientation = {
                           1,  0,  0,
                           0, -1,  0,
                           0,  0, -1
	},
#endif
	.board_init = inv_mpu_early_init,
	.board_exit = inv_mpu_exit,
	.power_on = inv_mpu_power_on,
	.power_off = inv_mpu_power_off
};
#endif	/* CONFIG_INV_MPU_IIO */

#if (defined(CONFIG_I2C_GPIO) || defined(CONFIG_I2C0_V12_JZ) || defined(CONFIG_I2C0_DMA_V12))
static struct i2c_board_info jz_i2c0_devs[] __initdata = {

#ifdef CONFIG_JZ4785_SUPPORT_TSC
#ifdef CONFIG_TOUCHSCREEN_GWTC9XXXB
	{
	 I2C_BOARD_INFO("gwtc9xxxb_ts", 0x05),
	 .platform_data = &fpga_tsc_pdata,
	},
#endif

#ifdef CONFIG_TOUCHSCREEN_FT6X06
	{
		I2C_BOARD_INFO("ft6x06_ts", 0x38),
		.platform_data = &ft6x06_tsc_pdata,
	},
#endif
#endif /*CONFIG_JZ4785_SUPPORT_TSC*/
#if defined(CONFIG_INV_MPU_IIO)
	{
		I2C_BOARD_INFO("mpu6500", 0x68),
		.irq = (IRQ_GPIO_BASE + GPIO_GSENSOR_INT1),
		.platform_data = &mpu9250_platform_data,
	},
#endif /*CONFIG_INV_MPU_IIO*/
};
#endif

#if (defined(CONFIG_I2C_GPIO) || defined(CONFIG_I2C1_V12_JZ))

#ifdef CONFIG_WM8594_CODEC_V1_2
static struct snd_codec_data wm8594_codec_pdata = {
	    .codec_sys_clk = 12000000,
};
#endif

static struct i2c_board_info jz_i2c1_devs[] __initdata = {
#ifdef CONFIG_WM8594_CODEC_V1_2
	{
		I2C_BOARD_INFO("wm8594", 0x1a),
		.platform_data = &wm8594_codec_pdata,
	},
#endif
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
DEF_GPIO_I2C(1,GPIO_PE(30),GPIO_PE(31));
#endif
#ifndef CONFIG_I2C2_V12_JZ
DEF_GPIO_I2C(2,GPIO_PF(16),GPIO_PF(17));
#endif
#ifndef CONFIG_I2C3_V12_JZ
DEF_GPIO_I2C(3,GPIO_PD(10),GPIO_PD(11));
#endif
#endif /*CONFIG_I2C_GPIO*/


#ifdef CONFIG_BATTERY_JZ4785

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
	.slop_r = 0,
	.cut_r = 0,
};

static struct jz_adc_platform_data adc_platform_data;
#endif

#ifdef CONFIG_SPI_JZ_V1_2
#include <mach/jzssi_v1_2.h>
#ifdef CONFIG_SPI0_JZ_V1_2
static struct spi_board_info jz_spi0_board_info[] = {
       [0] = {
	       .modalias       = "spidev",
	       .bus_num	       = 0,
	       .chip_select    = 0,
	       .max_speed_hz   = 1200000,
       },
};

struct jz_spi_info spi0_info_cfg = {
       .chnl = 0,
       .bus_num = 0,
       .max_clk = 54000000,
       .num_chipselect = 2,
};
#endif

#ifdef CONFIG_SPI1_JZ_V1_2
static struct spi_board_info jz_spi1_board_info[] = {
    [0] = {
	       .modalias       = "spidev",
	       .bus_num	       = 1,
	       .chip_select    = 1,
	       .max_speed_hz   = 120000,
    },
};

struct jz_spi_info spi1_info_cfg = {
       .chnl = 1,
       .bus_num = 1,
       .max_clk = 54000000,
       .num_chipselect = 2,
};
#endif
#endif

#if defined(CONFIG_SPI_GPIO)
static struct spi_gpio_platform_data jz4780_spi_gpio_data = {
	.sck	= (4*32 + 15),
	.mosi	= (4*32 + 17),
	.miso	= (4*32 + 14),
	.num_chipselect	= 2,
};

static struct platform_device jz4780_spi_gpio_device = {
	.name	= "spi_gpio",
	.dev	= {
		.platform_data = &jz4780_spi_gpio_data,
	},
};

static struct spi_board_info jz_spi0_board_info[] = {
       [0] = {
	       .modalias       = "spidev",
	       .bus_num	       = 0,
	       .chip_select    = 0,
	       .max_speed_hz   = 120000,
       },
};
#endif

static int __init board_init(void)
{

	/* ADC */
#ifdef CONFIG_BATTERY_JZ4785
	adc_platform_data.battery_info = dorado_battery_info;
	jz_device_register(&jz_adc_device,&adc_platform_data);
#endif
	/* li-ion charger */
#ifdef CONFIG_CHARGER_LI_ION
	platform_device_register(&jz_li_ion_charger_device);
#endif
	/* VPU */
#ifdef CONFIG_JZ_VPU_V1_2
	platform_device_register(&jz_vpu_device);
#endif
#ifdef CONFIG_KEYBOARD_GPIO
	platform_device_register(&jz_button_device);
#endif
#ifdef CONFIG_BCM_PM_CORE
	platform_device_register(&bcm_power_platform_device);
#endif

#ifdef CONFIG_SPI_JZ_V1_2
#ifdef CONFIG_SPI0_JZ_V1_2
       spi_register_board_info(jz_spi0_board_info, ARRAY_SIZE(jz_spi0_board_info));
       platform_device_register(&jz_ssi0_device);
       platform_device_add_data(&jz_ssi0_device, &spi0_info_cfg, sizeof(struct jz_spi_info));
#endif

#ifdef CONFIG_SPI1_JZ_V1_2
       spi_register_board_info(jz_spi1_board_info, ARRAY_SIZE(jz_spi1_board_info));
       platform_device_register(&jz_ssi1_device);
       platform_device_add_data(&jz_ssi1_device, &spi1_info_cfg, sizeof(struct jz_spi_info));
#endif
#endif


/*i2c*/
#ifdef CONFIG_I2C_GPIO
#ifndef CONFIG_I2C0_V12_JZ
	platform_device_register(&i2c0_gpio_device);
#endif
#ifndef CONFIG_I2C1_V12_JZ
	platform_device_register(&i2c1_gpio_device);
#endif

#ifndef CONFIG_I2C2_V12_JZ
	platform_device_register(&i2c2_gpio_device);
#endif
#ifndef CONFIG_I2C3_V12_JZ
	platform_device_register(&i2c3_gpio_device);
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
/* panel and bl */
#ifdef CONFIG_LCD_KD50G2_40NM_A2
	platform_device_register(&kd50g2_40nm_a2_device);
#endif
#ifdef CONFIG_LCD_BYD_BM8766U
	platform_device_register(&byd_bm8766u_device);
#endif
#ifdef CONFIG_BM347WV_F_8991FTGF_HX8369
	platform_device_register(&byd_8991_device);
#endif
#ifdef CONFIG_LCD_KFM701A21_1A
	platform_device_register(&kfm701a21_1a_device);
#endif
#ifdef CONFIG_LCD_LH155
	platform_device_register(&lh155_device);
#endif
#ifdef CONFIG_LCD_HX8389_B11_G
	platform_device_register(&hx8389_b11_g_device);
#endif

#ifdef CONFIG_LCD_TRULY_TFT240240_2_E
	platform_device_register(&truly_tft240240_device);
#endif

#ifdef CONFIG_LCD_CV90_M5377_P30
	platform_device_register(&cv90_m5377_p30_device);
#endif

#ifdef CONFIG_BACKLIGHT_PWM
	platform_device_register(&backlight_device);
#endif
#ifdef CONFIG_BACKLIGHT_DIGITAL_PULSE
	platform_device_register(&digital_pulse_backlight_device);
#endif
/* lcdc framebuffer*/
#ifdef CONFIG_FB_JZ_V1_2
	jz_device_register(&jz_fb_device, &jzfb0_pdata);
#endif

/*mipi-dsi */
#ifdef CONFIG_JZ_MIPI_DSI
	jz_device_register(&jz_dsi_device, &jzdsi_pdata);
#endif

/*ipu*/
#if defined CONFIG_JZ_IPU_V1_2
	platform_device_register(&jz_ipu_device);
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
#ifdef CONFIG_SOUND_JZ_I2S_V1_2
	jz_device_register(&jz_i2s_device,&i2s_data);
	jz_device_register(&jz_mixer0_device,&snd_mixer0_data);
#endif
#ifdef CONFIG_SOUND_JZ_SPDIF_V1_2
	jz_device_register(&jz_spdif_device,&spdif_data);
	jz_device_register(&jz_mixer2_device,&snd_mixer2_data);
#endif
#ifdef CONFIG_SOUND_JZ_DMIC_V1_2
	jz_device_register(&jz_dmic_device,&dmic_data);
	jz_device_register(&jz_mixer3_device,&snd_mixer3_data);
#endif

#ifdef CONFIG_SOUND_JZ_PCM_V1_2
	jz_device_register(&jz_pcm_device,&pcm_data);
	jz_device_register(&jz_mixer1_device,&snd_mixer1_data);
#endif
#ifdef CONFIG_JZ_INTERNAL_CODEC_V1_2
	jz_device_register(&jz_codec_device, &codec_data);
#endif
	/* ovisp */
#ifdef CONFIG_VIDEO_OVISP
       jz_device_register(&ovisp_device_camera, &ovisp_camera_info);
#endif
#ifdef CONFIG_RTC_DRV_JZ4775
	platform_device_register(&jz_rtc_device);
#endif
/* efuse */
#ifdef CONFIG_JZ_EFUSE_V10
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
