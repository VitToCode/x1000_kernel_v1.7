#ifndef __INTERFACE_H__
#define __INTERFACE_H__

/*extern unsigned __res_mem char *res_buf;*/
/*extern struct host_handler *h_handler;*/



extern int (*h_handler)(const char *fmt, ...);
#define printk	h_handler
#define printf printk



enum open_mode {
	EARLY_SLEEP = 1,
	DEEP_SLEEP,
	NORMAL_RECORD
};


#define DMIC_IOCTL_SET_SAMPLERATE	0x200





#define OFF_TDR         (0x00)
#define OFF_LCR         (0x0C)
#define OFF_LSR         (0x14)

#define LSR_TDRQ        (1 << 5)
#define LSR_TEMT        (1 << 6)




#define UART1_IOBASE    0x10031000
#define U1_IOBASE (UART1_IOBASE + 0xa0000000)
#define TCSM_PCHAR(x)                           \
	*((volatile unsigned int*)(U1_IOBASE+OFF_TDR)) = x;     \
while ((*((volatile unsigned int*)(U1_IOBASE + OFF_LSR)) & (LSR_TDRQ | LSR_TEMT)) != (LSR_TDRQ | LSR_TEMT))


static inline void serial_put_hex(unsigned int x) {
	int i;
	unsigned int d;
	for(i = 7;i >= 0;i--) {
		d = (x  >> (i * 4)) & 0xf;
		if(d < 10) d += '0';
		else d += 'A' - 10;
		TCSM_PCHAR(d);
	}
}

#endif



