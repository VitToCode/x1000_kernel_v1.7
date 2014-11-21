/*
 * main.c
 */
#include <common.h>
#include <jz_cpm.h>

#include "ivDefine.h"
#include "ivIvwDefine.h"
#include "ivIvwErrorCode.h"
#include "ivIVW.h"
#include "ivPlatform.h"

#include "interface.h"
#include "voice_wakeup.h"
#include "dma_ops.h"
#include "dmic_ops.h"
#include "jz_dmic.h"
#include "rtc_ops.h"
#include "dmic_config.h"
#include "jz_dma.h"

#include <archdefs.h>

#define TAG	"[voice_wakeup]"

int (*h_handler)(const char *fmt, ...);

#define printk	h_handler


int open(int mode)
{
	dma_open();
	switch (mode) {
		case EARLY_SLEEP:
			break;
		case DEEP_SLEEP:
			printk("deep sleep open\n");
			rtc_init();
			dmic_init_mode(DEEP_SLEEP);
			wakeup_open();
			dmic_enable();
			///* UNMASK INTC */
			REG32(0xB000100C) = 1<<0; /*dmic int en*/
			REG32(0xB000102C) = 1<<0; /*rtc int en*/
			break;
		case NORMAL_RECORD:
			//dmic_init_record();
			dmic_init_mode(NORMAL_RECORD);
			dmic_enable();
			break;
		default:
			printk("%s:bad open mode\n", TAG);
			break;


	}

	return 1;
}



/* mask ints that we don't care */
#define INTC0_MASK	0xfffffffe
#define INTC1_MASK	0xfffffffe

int handler(int par)
{
	int ret;

	//pdma_start(DMA_CHANNEL);
	//REG32(CPM_IOBASE + CPM_CLKGR0) &= ~(1 << 21);
	/*if wakeup source is none of business with dmic, just return SYS_WAKEUP_OK*/
	unsigned int opcr;
	opcr = REG32(0xb0000000 + CPM_OPCR);
	opcr &= ~(3<<25);
	opcr |= 1 << 30;
	REG32(0xb0000000 + CPM_OPCR) = opcr;

	while(1) {
		if((REG32(0xb0001010) & INTC0_MASK) || (REG32(0xb0001030) & INTC1_MASK)) {
			TCSM_PCHAR('p');
			serial_put_hex(REG32(0xb0001010));
			serial_put_hex(REG32(0xb0001030));
			//serial_put_hex(REG_DMIC_ICR);
			//serial_put_hex(REG_DMIC_IMR);
			TCSM_PCHAR('J');
			ret = SYS_WAKEUP_OK;
			//return SYS_WAKEUP_OK;
		}

		/* RTC interrupt pending */
		if(REG32(0xb0001030) & (1<<0)) {
			TCSM_PCHAR('R');
			TCSM_PCHAR('T');
			TCSM_PCHAR('C');
			ret = rtc_int_handler();
			if(ret == SYS_TIMER) {
				serial_put_hex(REG32(0xb0001010));
				serial_put_hex(REG32(0xb0001030));
				ret = SYS_WAKEUP_OK;
				//return SYS_WAKEUP_OK;
			} else if (ret == DMIC_TIMER){

			}
		}

		/* DMIC interrupt pending */
		if(dmic_handler() == SYS_WAKEUP_OK) {
			ret = SYS_WAKEUP_OK;
			//return SYS_WAKEUP_OK;
		} else {
			ret = SYS_WAKEUP_FAILED;
			//return SYS_WAKEUP_FAILED;
		}
		if(ret == SYS_WAKEUP_FAILED) {
			REG32(CPM_IOBASE + CPM_CLKGR0) |= (1<<21);
			__asm__ volatile(".set mips32\n\t"
					"wait\n\t"
					"nop\n\t"
					"nop\n\t"
					"nop\n\t"
					//"jr %0\n\t"
					//         ".set mips32 \n\t"
					);

			REG32(CPM_IOBASE + CPM_CLKGR0) &= ~(1<<21);
		} else {
			break;
		}
	}
	return ret;
	//pdma_end(DMA_CHANNEL);
	//REG32(CPM_IOBASE + CPM_CLKGR0) |= 1 << 21;
}
int close(int mode)
{
	printk("module close\n");
	/* MASK INTC*/
	REG32(0xB0001008) |= 1<< 0;
	REG32(0xB0001028) |= 1<< 0;
	wakeup_close();
	dmic_disable();
	dma_close();
	return 0;
}

int set_handler(void *handler)
{
	h_handler = (int(*)(const char *fmt, ...)) handler;

	return 0;
}

unsigned int get_dma_address(void)
{
	return pdma_trans_addr(DMA_CHANNEL, DMA_DEV_TO_MEM);
}

int ioctl(int cmd, unsigned long args)
{
	return dmic_ioctl(cmd, args);
}


#define LOAD_ADDR	0x8ff00000
#define LOAD_SIZE	(256 * 1024)

void cache_prefetch(void)
{
	int i;
	volatile unsigned int *addr = (unsigned int *)0x8ff00000;
	volatile unsigned int a;
	for(i=0; i<LOAD_SIZE/32; i++) {
		a = *(addr + i * 8);
	}
}

