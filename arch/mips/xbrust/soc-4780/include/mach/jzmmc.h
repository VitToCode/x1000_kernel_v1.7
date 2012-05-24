#ifndef __JZ_MMC_H__
#define __JZ_MMC_H__

enum {
	DONTCARE = 0,
	NONREMOVABLE,
	REMOVABLE,
	MANUAL,
};

struct mmc_partition_info {
	char				name[32];
	unsigned int			saddr;
	unsigned int			len;
	int				type;
};

struct mmc_recovery_info {
	struct mmc_partition_info	*partition_info;
	unsigned int			partition_num;
	unsigned int			permission;
	unsigned int			protect_boundary;
};

struct jzmmc_pin {
	unsigned short			num;
#define LOW_ENABLE			0
#define HIGH_ENABLE			1
	unsigned short 			enable_level;
};

struct card_gpio {
	struct jzmmc_pin 		wp;
	struct jzmmc_pin 		cd;
	struct jzmmc_pin 		pwr;
};

/**
 * struct jzmmc_platform_data is a struct which defines board MSC informations
 * @removal: This shows the card slot's type:
 *	REMOVABLE/IRREMOVABLE/MANUAL (Tablet card/Phone card/build-in SDIO).
 * @sdio_clk: SDIO device's clock can't use Low-Power-Mode.
 * @ocr_mask: This one shows the voltage that host provide.
 * @capacity: Shows the host's speed capacity and bus width.
 * @recovery_info: Informations that Android recovery mode uses.
 * @gpio: Slot's gpio information including pins of write-protect, card-detect and power.
 * @insert_irq_info: The informations of slot's insert-interrupt pin, if it's removal = REMOVABLE.
 */
struct jzmmc_platform_data {
	unsigned short			removal;
	unsigned short			sdio_clk;
	unsigned int			ocr_avail;
	unsigned int			capacity;
	struct mmc_recovery_info	*recovery_info;
	struct card_gpio		*gpio;
};

#endif
