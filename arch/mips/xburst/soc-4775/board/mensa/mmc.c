#include <linux/mmc/host.h>

#include <mach/jzmmc.h>

#include "board.h"

static struct card_gpio tf_gpio = {
	.cd				= {GPIO_SD0_CD_N,	LOW_ENABLE},
	.pwr				= {GPIO_SD0_VCC_EN_N,	HIGH_ENABLE},
};

struct jzmmc_platform_data tf_pdata = {
	.removal  			= REMOVABLE,
	.sdio_clk			= 0,
	.ocr_avail			= MMC_VDD_32_33 | MMC_VDD_33_34,
	.capacity  			= MMC_CAP_SD_HIGHSPEED | MMC_CAP_4_BIT_DATA,
	.recovery_info			= NULL,
	.gpio				= &tf_gpio,
};

struct jzmmc_platform_data sdio_pdata = {
	.removal  			= MANUAL,
	.sdio_clk			= 1,
	.ocr_avail			= MMC_VDD_32_33 | MMC_VDD_33_34,
	.capacity  			= MMC_CAP_4_BIT_DATA,
	.recovery_info			= NULL,
	.gpio				= NULL,
};
