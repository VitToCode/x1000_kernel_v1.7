OUTPUT_ARCH(mips)
ENTRY(_reset)
MEMORY{
	tcsm_bank0(rw)  : ORIGIN = 0xF4000000 , LENGTH = 0x2000
	tcsm_bank2(rw)  : ORIGIN = 0xF4002000 , LENGTH = 0x1000
	tcsm_bank3(rw)  : ORIGIN = 0xF4003000 , LENGTH = 0x1000
}

SECTIONS
{
	.tcsm_bank0 : {
		. = ALIGN(4);
                *(.text)

                *(.data)
                CONSTRUCTORS
                _gp = ALIGN(4);
                *(.sdata)
                *(.sdata.*)
                *(.sbss)
                *(.bss)
                *(.scommon)
                *(.rodata)
	} > tcsm_bank0

	.tcsm_bank2 : {
		*(.tcsm_data2)
	} > tcsm_bank2

	.tcsm_bank3 : {
		*(.tcsm_data3)
	} > tcsm_bank3

	/DISCARD/ : {
		*(COMMON)
		*(.pdri)
		*(.comment)
		*(.gnu.attributes)
		*(.reginfo)
	}
}

