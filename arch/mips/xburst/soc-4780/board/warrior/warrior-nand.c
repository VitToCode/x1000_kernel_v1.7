#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/resource.h>

#include <soc/gpio.h>
#include <soc/base.h>
#include <soc/irq.h>

#include <mach/platform.h>

#include <mach/jznand.h>
#include "warrior.h"

#define ECCBIT 24

static struct platform_nand_partition partition_info[] = {
	{
	name:"x-boot",
	offset:0 * 0x100000LL,
	size:4 * 0x100000LL,
	mode:DIRECT_MANAGER,
	eccbit:ECCBIT,
	use_planes:ONE_PLANE,
	part_attrib:PART_XBOOT
	},
	{
	name:"boot",
	offset:4 * 0x100000LL,
	size:8 * 0x100000LL,
	mode:DIRECT_MANAGER,
	eccbit:ECCBIT,
	use_planes:ONE_PLANE,
	part_attrib:PART_KERNEL
	},
    {
    name:"recovery",
    offset:12 * 0x100000LL,
    size:16 * 0x100000LL,
    mode:DIRECT_MANAGER,
    eccbit:ECCBIT,
    use_planes:ONE_PLANE,
    part_attrib:PART_KERNEL
    },
	{
    name:"ndcache",
    offset:28 * 0x100000LL,
    size:36 * 0x100000LL,
    mode:ZONE_MANAGER,
    eccbit:ECCBIT,
    use_planes:ONE_PLANE,
    part_attrib:PART_KERNEL
    },
	{
	name:"ndsystem",
	offset:64 * 0x100000LL,
	size:512 * 0x100000LL,
	mode:ZONE_MANAGER,
	eccbit:ECCBIT,
	use_planes:ONE_PLANE,
	part_attrib:PART_SYSTEM
    },
	{
	name:"nddata",
	offset:576 * 0x100000LL,
	size:512 * 0x100000LL,
	mode:ZONE_MANAGER,
	eccbit:ECCBIT,
	use_planes:ONE_PLANE,
	part_attrib:PART_DATA
    },
	{
	name:"ndmisc",
	offset:1088 * 0x100000LL,
	size:512 * 0x100000LL,
	mode:ZONE_MANAGER,
	eccbit:ECCBIT,
	use_planes:ONE_PLANE,
	part_attrib:PART_MISC
    },
};

/* Define max reserved bad blocks for each partition.
 * This is used by the mtdblock-jz.c NAND FTL driver only.
 *
 * The NAND FTL driver reserves some good blocks which can't be
 * seen by the upper layer. When the bad block number of a partition
 * exceeds the max reserved blocks, then there is no more reserved
 * good blocks to be used by the NAND FTL driver when another bad
 * block generated.
 */
static int partition_reserved_badblocks[] = {
	2,			/* reserved blocks of mtd0 */
	4,			/* reserved blocks of mtd1 */
	4,			/* reserved blocks of mtd2 */
	8,			/* reserved blocks of mtd3 */
	20,			/* reserved blocks of mtd4 */
	20,			/* reserved blocks of mtd5 */
	20,         /* reserved blocks of mtd6 */
};

struct platform_nand_data jz_nand_chip_data = {
	.nr_partitions = ARRAY_SIZE(partition_info),
	.partitions = partition_info,
	/* there is no room for bad block info in struct platform_nand_data */
	/* so we have to use chip.priv */
	.priv = &partition_reserved_badblocks,
};
