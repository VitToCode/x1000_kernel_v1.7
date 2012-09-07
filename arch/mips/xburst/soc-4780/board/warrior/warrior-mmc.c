#include <linux/mmc/host.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/wakelock.h>
#include <linux/err.h>
#include <linux/delay.h>

#include <mach/jzmmc.h>
#include "warrior.h"

#define KBYTE				(1024LL)
#define MBYTE				((KBYTE)*(KBYTE))
#define UINT32_MAX			(0xffffffffU)
#define GPIO_WL_RST_N			GPIO_PF(17)
#define RESET				0
#define NORMAL				1

static struct wifi_data			iw8101_data;

int iw8101_wlan_init(void);

struct mmc_partition_info warrior_inand_partition_info[] = {
	[0] = {"mbr",           0,       512, 0}, 	//0 - 512KB
	[1] = {"xboot",		0,     2*MBYTE, 0}, 	//0 - 2MB
	[2] = {"boot",      3*MBYTE,   8*MBYTE, 0}, 	//3MB - 8MB
	[3] = {"recovery", 12*MBYTE,   8*MBYTE, 0}, 	//12MB - 8MB
	[4] = {"misc",     21*MBYTE,   4*MBYTE, 0}, 	//21MB - 4MB
	[5] = {"battery",  26*MBYTE,   1*MBYTE, 0}, 	//26MB - 1MB
	[6] = {"cache",    28*MBYTE,  30*MBYTE, 1}, 	//28MB - 30MB
	[7] = {"device_id",59*MBYTE,   2*MBYTE, 0},	//59MB - 2MB
	[8] = {"system",   64*MBYTE, 256*MBYTE, 1}, 	//64MB - 256MB
	[9] = {"data",    321*MBYTE, 512*MBYTE, 1}, 	//321MB - 512MB
};

static struct mmc_recovery_info warrior_inand_recovery_info = {
	.partition_info			= warrior_inand_partition_info,
	.partition_num			= ARRAY_SIZE(warrior_inand_partition_info),
	.permission			= MMC_BOOT_AREA_PROTECTED,
	.protect_boundary		= 21*MBYTE,
};
	
struct jzmmc_platform_data warrior_inand_pdata = {
	.removal  			= DONTCARE,
	.sdio_clk			= 0,
	.ocr_avail			= MMC_VDD_32_33 | MMC_VDD_33_34,
	.capacity  			= MMC_CAP_SD_HIGHSPEED | MMC_CAP_4_BIT_DATA | MMC_CAP_NONREMOVABLE,
	.recovery_info			= &warrior_inand_recovery_info,
	.gpio				= NULL,
};

struct jzmmc_platform_data warrior_sdio_pdata = {
	.removal  			= MANUAL,
	.sdio_clk			= 1,
	.ocr_avail			= MMC_VDD_32_33 | MMC_VDD_33_34,
	.capacity  			= MMC_CAP_4_BIT_DATA,
	.recovery_info			= NULL,
	.gpio				= NULL,
	.private_init			= iw8101_wlan_init,
};

/*
 * WARING:
 * If a GPIO is not used or undefined, it must be set -1,
 * or PA0 will be request.
 */
static struct card_gpio warrior_tf_gpio = {
	.cd				= {GPIO_PF(20),		LOW_ENABLE},
	.wp				= {-1,			-1},
	.pwr				= {-1,			-1},
};

struct jzmmc_platform_data warrior_tf_pdata = {
	.removal  			= REMOVABLE,
	.sdio_clk			= 0,
	.ocr_avail			= MMC_VDD_32_33 | MMC_VDD_33_34,
	.capacity  			= MMC_CAP_SD_HIGHSPEED | MMC_CAP_4_BIT_DATA,
	.recovery_info			= NULL,
	.gpio				= &warrior_tf_gpio,
};

int iw8101_wlan_init(void)
{
	static struct wake_lock	*wifi_wake_lock = &iw8101_data.wifi_wake_lock;
	struct regulator *power = iw8101_data.wifi_power;
	int reset = iw8101_data.wifi_reset;

	power = regulator_get(NULL, "vwifi");
	if (IS_ERR(power)) {
		pr_err("wifi regulator missing\n");
		return -EINVAL;
	}

	reset = GPIO_WL_RST_N;
	if (gpio_request(GPIO_WL_RST_N, "wifi_reset")) {
		pr_err("no wifi_reset pin available\n");
		regulator_put(power);

		return -EINVAL;
	} else {
		gpio_direction_output(reset, 1);
	}

	wake_lock_init(wifi_wake_lock, WAKE_LOCK_SUSPEND, "wifi_wake_lock");

	return 0;
}

void IW8101_wlan_power_on(int flag)
{
	static struct wake_lock	*wifi_wake_lock = &iw8101_data.wifi_wake_lock;
	struct regulator *power = iw8101_data.wifi_power;
	int reset = iw8101_data.wifi_reset;

	pr_debug("wlan power on:%d\n", flag);
	jzrtc_switch_clk32k(1);
	mdelay(200);

	switch(flag) {
		case RESET:
			regulator_enable(power);

			gpio_set_value(reset, 0);
			mdelay(200);

			gpio_set_value(reset, 1);
			mdelay(200);

			break;

		case NORMAL:
			regulator_enable(power);

			gpio_set_value(reset, 0);
			mdelay(200);

			gpio_set_value(reset, 1);

			mdelay(200);
			jzmmc_manual_detect(1, 1);

			break;
	}
	wake_lock(wifi_wake_lock);
}

void IW8101_wlan_power_off(int flag)
{
	static struct wake_lock	*wifi_wake_lock = &iw8101_data.wifi_wake_lock;
	struct regulator *power = iw8101_data.wifi_power;
	int reset = iw8101_data.wifi_reset;

	pr_debug("wlan power off:%d\n", flag);
	switch(flag) {
		case RESET:
			gpio_set_value(reset, 0);

			regulator_disable(power);
			break;

		case NORMAL:
			gpio_set_value(reset, 0);

			regulator_disable(power);

 			jzmmc_manual_detect(1, 0);
			break;
	}

	wake_unlock(wifi_wake_lock);

	jzrtc_switch_clk32k(0);
}

EXPORT_SYMBOL(IW8101_wlan_power_on);
EXPORT_SYMBOL(IW8101_wlan_power_off);
