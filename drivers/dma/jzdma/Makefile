
obj-y += jzdma.o

$(obj)/jzdma.o:$(obj)/firmware_hex

$(obj)/firmware_hex:
	$(Q)$(MAKE) $(build)=drivers/dma/jzdma/firmware firmware_hex
