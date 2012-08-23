# drivers/nand/driver/Makefile 
#

KBUILD_CFLAGS += -I$(srctree)/drivers/nand/inc

obj-$(REAL_NAND_DRIVER) += nand_api.o
obj-$(REAL_NAND_DRIVER) += core/ jz_ctrl/ test/
obj-$(CONFIG_NAND_DMA) += dma/
obj-$(FAKE_NAND_DRIVER) += vnand_test/
