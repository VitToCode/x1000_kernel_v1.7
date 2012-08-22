# drivers/nand/driver/Makefile 
#

KBUILD_CFLAGS += -I$(srctree)/drivers/nand/inc

obj-y += nand_api.o
obj-y += core/ jz_ctrl/
obj-y += test/
obj-$(CONFIG_NAND_DMA) += dma/
#obj-y += vnand_test/
