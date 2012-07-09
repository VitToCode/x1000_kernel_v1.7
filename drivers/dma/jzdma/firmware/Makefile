
targets		:= firmware_hex firmware.o

drop-sections := .reginfo .mdebug .comment .note .pdr .options .MIPS.options
strip-flags   := $(addprefix --remove-section=,$(drop-sections))
OBJCOPY_ARGS 	:= -O elf32-tradlittlemips

LDFLAGS	:= -nostdlib -EL -T target.ld

SRCS := $(obj)/src/start.S		\
	$(obj)/src/main.c		\
	$(obj)/src/irq_pdma.c		\
	$(obj)/src/pdma.c		\
	$(obj)/src/nand/bch.c		\
	$(obj)/src/nand/nand_mcu.c	\
	$(obj)/src/nand/nand_pdma.c

FIRMWARE_ELF := $(obj)/firmware.elf
FIRMWARE_BIN := $(obj)/firmware.bin

firmware_hex: $(FIRMWARE_BIN) FORCE
	@hexdump -v -e '12/1 "0x%02x, " "\n"' $< > $(obj)/../firmware.hex
	@rm $(obj)/tmp.elf $(obj)/firmware.o $< -rf

$(FIRMWARE_BIN):$(obj)/firmware.o
	$(LD) -nostdlib -EL -T $(obj)/target.ld $< -o $(obj)/tmp.elf
	$(OBJCOPY) $(strip-flags) -O binary $(obj)/tmp.elf $@

$(obj)/firmware.o:$(SRCS)
	@$(CC) $(c_flags) -I$(obj)/include $(SRCS) -o $@


