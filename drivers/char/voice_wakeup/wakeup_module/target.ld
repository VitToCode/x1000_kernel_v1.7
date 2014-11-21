OUTPUT_ARCH(mips)
ENTRY(start)
MEMORY{
	/*
	 * 256Kbytes
	 *	
	 * */
	voice_mem(rw): ORIGIN = 0x8FF00000, LENGTH = 0x40000
	/* 
	 * last 80Kbytes store resource file.
	 * */
	voice_res(rw): ORIGIN = 0x8FF2c000, LENGTH = 0x14000
}

SECTIONS
{
	.voice_mem : {
		. = ALIGN(4);
			*(.text)
			/**(.data)*/
			
			/**(.bss)*/
	} > voice_mem

	.voice_res : {

	} > voice_res
	/DISCARD/ : {
		*(COMMON)
		*(.pdri)
		*(.comment)
		*(.gnu.attributes)
		*(.reginfo)
	}
}
