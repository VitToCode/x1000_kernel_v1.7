#ifndef __BOARD_BASE_H__
#define __BOARD_BASE_H__

#include <board.h>
#include <linux/spi/spi.h>
#include <linux/i2c.h>

extern struct snd_codec_data codec_data;

#ifdef CONFIG_JZ_INTERNAL_CODEC_V12
extern struct snd_codec_data codec_data;
#endif

#ifdef CONFIG_JZMMC_V12_MMC0
extern struct jzmmc_platform_data tf_pdata;
#endif
#ifdef CONFIG_JZMMC_V12_MMC1
extern struct jzmmc_platform_data sdio_pdata;
#endif

#ifdef CONFIG_JZMMC_V11_MMC2
extern struct jzmmc_platform_data sdio_pdata;
#endif

#ifdef CONFIG_BROADCOM_RFKILL
extern struct platform_device	bt_power_device;
extern struct platform_device	bluesleep_device;
#endif

#ifdef CONFIG_BCM_AP6212_RFKILL
extern struct platform_device   bt_power_device;
#endif

#ifdef CONFIG_USB_DWC2
extern struct platform_device   jz_dwc_otg_device;
#endif

#ifdef CONFIG_JZ_SPI0
extern struct jz_spi_info spi0_info_cfg;
#endif
#ifdef CONFIG_JZ_SFC
extern struct jz_sfc_info sfc_info_cfg;
#endif
#ifdef CONFIG_JZ_SPI_NOR
extern struct spi_board_info jz_spi0_board_info[];
extern int jz_spi0_devs_size;
#endif
#ifdef CONFIG_MTD_JZ_SPI_NORFLASH
extern struct spi_board_info jz_spi0_board_info[];
extern int jz_spi0_devs_size;
#endif
#ifdef CONFIG_SPI_GPIO
extern struct platform_device jz_spi_gpio_device;
#endif

#ifdef CONFIG_JZ_EFUSE_V11
extern struct jz_efuse_platform_data jz_efuse_pdata;
#endif

#ifdef CONFIG_KEYBOARD_MATRIX
extern struct platform_device jz_matrix_kdb_device;
#endif
#endif
