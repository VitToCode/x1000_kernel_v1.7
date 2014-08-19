#
# Makefile
#

KBUILD_CFLAGS	+= -I$/drivers/nand/driver/inc/ \
		-I$/drivers/nand/manager/inc/

obj-$(CONFIG_NAND)	+= hw/transadaptor/cpu_trans.o \
			hw/transadaptor/dma_trans.o \
			hw/nand_bch.o

ifdef CONFIG_NAND_NEMC
obj-$(CONFIG_NAND) += hw/nand_io_emc.o
else
obj-$(CONFIG_NAND) += hw/nand_io_nfi.o
endif

obj-$(CONFIG_NAND)	+= ops/

obj-$(CONFIG_NAND)	+= chips/nand_chip.o chips/nand_info.o chips/nand_errpt.o \
			utils/os_clib.o utils/speed_dug.o \
			utils/nand_debug.o nand_api.o

obj-$(CONFIG_NAND)	+= nand_driver.o
