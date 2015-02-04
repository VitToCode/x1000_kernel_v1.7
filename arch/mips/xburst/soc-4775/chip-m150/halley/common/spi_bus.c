#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>

#include <mach/jzssi.h>
#include "board_base.h"


#ifdef CONFIG_JZ_SPI_NOR
struct spi_nor_block_info flash_block_info[] = {
	{
		.blocksize      = 64 * 1024,
		.cmd_blockerase = 0xD8,
		.be_maxbusy     = 1200  /* 1.2s */
	},

	{
		.blocksize      = 32 * 1024,
		.cmd_blockerase = 0x52,
		.be_maxbusy     = 1000  /* 1s */
	},
};

struct spi_nor_platform_data spi_nor_pdata = {
	.pagesize       = 256,
	.sectorsize     = 4 * 1024,
	.chipsize       = 16384 * 1024,

	.block_info     = flash_block_info,
	.num_block_info = ARRAY_SIZE(flash_block_info),

	.addrsize       = 3,
	.pp_maxbusy     = 3,            /* 3ms */
	.se_maxbusy     = 400,          /* 400ms */
	.ce_maxbusy     = 80 * 1000,    /* 80s */

	.st_regnum      = 3,
};


struct spi_board_info jz_spi0_board_info[]  = {

	[0] ={
		.modalias       = "jz_nor",
		.platform_data          = &spi_nor_pdata,
		.controller_data        = (void *)GPIO_PA(23), /* cs for spi gpio */
		.max_speed_hz           = 12000000,
		.bus_num                = 0,
		.chip_select            = 0,
	},
};
int jz_spi0_devs_size = ARRAY_SIZE(jz_spi0_board_info);
#endif

#ifdef CONFIG_SPI0_JZ47XX
struct jz47xx_spi_info spi0_info_cfg = {
	.chnl = 0,
	.bus_num = 0,
	.max_clk = 54000000,
	.num_chipselect = 1,
	.chipselect     = {GPIO_PA(23)},
};
#endif

#ifdef CONFIG_SPI_GPIO
static struct spi_gpio_platform_data jz47xx_spi_gpio_data = {

	.sck	= GPIO_SPI_SCK,
	.mosi	= GPIO_SPI_MOSI,
	.miso	= GPIO_SPI_MISO,
	.num_chipselect = 1,
};

struct platform_device jz47xx_spi_gpio_device = {
	.name   = "spi_gpio",
	.dev    = {
		.platform_data = &jz47xx_spi_gpio_data,
	},
};
#endif

