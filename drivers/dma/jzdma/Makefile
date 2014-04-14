obj-y += jzdma.o

ifeq ($(CONFIG_NAND), y)

ifeq ($(CONFIG_XBURST_DMAC_COMMON),y)
	dir := common_firmware
else
	dir := toggle_firmware
endif

$(obj)/jzdma.o:$(obj)/firmware_hex

$(obj)/firmware_hex:
	$(Q)$(MAKE) $(build)=drivers/dma/jzdma/$(dir) firmware_hex
	cp drivers/dma/jzdma/$(dir)/firmware.hex drivers/dma/jzdma/

endif