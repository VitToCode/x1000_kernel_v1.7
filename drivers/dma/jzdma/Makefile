obj-y += jz_dma.o

ifeq ($(CONFIG_NAND), y)

ifdef CONFIG_NAND_NEMC
$(obj)/jzdma.o:$(obj)/firmware_nemc.hex
else
$(obj)/jzdma.o:$(obj)/firmware_nfi.hex
endif

endif
