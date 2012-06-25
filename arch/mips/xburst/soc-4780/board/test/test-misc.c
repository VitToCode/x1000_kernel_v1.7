#include <linux/platform_device.h>

#include <mach/platform.h>

#include "test.h"

static int __init test_board_init(void)
{
/* mmc */
#ifdef CONFIG_MMC0_JZ4780
	jz_device_register(&jz_msc0_device, &test_inand_pdata);
#endif
#ifdef CONFIG_MMC1_JZ4780
	jz_device_register(&jz_msc1_device, &test_tf_pdata);
#endif
#ifdef CONFIG_MMC2_JZ4780
	jz_device_register(&jz_msc2_device, &test_sdio_pdata);
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
#ifdef CONFIG_SOUND_CODEC_JZ4780
	jz_device_register(&jz_codec_device, &codec_data);
#endif

	platform_device_register(&test_lcd_device);
	platform_device_register(&test_backlight_device);

	jz_device_register(&jz_fb0_device,&jzfb_pdata);
	jz_device_register(&jz_fb0_device,&jzfb_pdata);

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
	return 0;
}

arch_initcall(test_board_init);
