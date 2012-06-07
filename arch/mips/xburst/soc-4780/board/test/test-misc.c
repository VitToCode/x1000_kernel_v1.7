#include <linux/platform_device.h>

#include <mach/platform.h>

#include "test.h"

static int __init test_board_init(void)
{
#ifdef CONFIG_MMC0_JZ4780
	jz_device_register(&jz_msc0_device, &test_inand_pdata);
#endif
#ifdef CONFIG_MMC1_JZ4780
	jz_device_register(&jz_msc1_device, &test_tf_pdata);
#endif
#ifdef CONFIG_MMC2_JZ4780
	jz_device_register(&jz_msc2_device, &test_sdio_pdata);
#endif
	return 0;
}

arch_initcall(test_board_init);
