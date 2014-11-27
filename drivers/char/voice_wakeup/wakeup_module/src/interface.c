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

enum wakeup_source {
	WAKEUP_BY_OTHERS = 1,
	WAKEUP_BY_DMIC
};

struct dma_buffers {
	unsigned char *buffer[8];
	unsigned long buffer_len;
	unsigned long buffer_num;
};

unsigned int cpu_wakeup_by = 0;
unsigned int open_cnt = 0;
unsigned int current_mode = 0;
unsigned char *sleep_buffer;
unsigned char sleep_buffer_len;

int open(int mode)
{
	switch (mode) {
		case EARLY_SLEEP:
			/*  if EARLY_SLEEP called. it must be opened by record, or wakeup.
			 *	first disable dmic for now.
			 *	then config dma to dst setted by set_early_sleep_buffer.
			 *	then start dma.
			 *	enable dmic.
			 * */
			//dmic_disable();
			//dma_close();

			//early_sleep_dma_config(sleep_buffer, sleep_buffer_len);

			//dmic_enable();
			break;
		case DEEP_SLEEP:
			printk("deep sleep open\n");
			rtc_init();
			dmic_init_mode(DEEP_SLEEP);
			wakeup_open();
			///* UNMASK INTC */
			REG32(0xB000100C) = 1<<0; /*dmic int en*/
			REG32(0xB000102C) = 1<<0; /*rtc int en*/
			break;
		case NORMAL_RECORD:
			dmic_init_mode(NORMAL_RECORD);
		case NORMAL_WAKEUP:
			wakeup_open();
			dmic_init_mode(NORMAL_RECORD);
			break;
		default:
			printk("%s:bad open mode\n", TAG);
			break;


	}
	/* no matter how many times open, dma and dmic need only init once.
	 * other resource depends. such as rtc, intc mask.
	 * */
	//if((mode == NORMAL_RECORD) && (open_cnt == 0)) {
	//dma_open();
	//dmic_enable();
	//}
	if(((open_cnt == 0) &&(mode != EARLY_SLEEP)) || (mode == DEEP_SLEEP)) {
		dma_open();
		dmic_enable();
	}
	open_cnt++;
	return 0;
}

/* mask ints that we don't care */
#define INTC0_MASK	0xfffffffe
#define INTC1_MASK	0xfffffffe

/* desc: this function is only called when cpu is in deep sleep
 * @par: no use.
 * @return : SYS_WAKEUP_OK, SYS_WAKEUP_FAILED.
 * */
int handler(int par)
{
	int ret;

	/*if wakeup source is none of business with dmic, just return SYS_WAKEUP_OK*/
	unsigned int opcr;
	opcr = REG32(0xb0000000 + CPM_OPCR);
	opcr &= ~(3<<25);
	opcr |= 1 << 30;
	REG32(0xb0000000 + CPM_OPCR) = opcr;

	while(1) {
		opcr = REG32(0xb0000000 + CPM_OPCR);
		opcr &= ~(3<<25);
		opcr |= 1 << 30;
		REG32(0xb0000000 + CPM_OPCR) = opcr;
		if((REG32(0xb0001010) & INTC0_MASK) || (REG32(0xb0001030) & INTC1_MASK)) {
			//TCSM_PCHAR('p');
			serial_put_hex(REG32(0xb0001010));
			serial_put_hex(REG32(0xb0001030));
			//serial_put_hex(REG_DMIC_ICR);
			//serial_put_hex(REG_DMIC_IMR);
			//TCSM_PCHAR('J');
			cpu_wakeup_by = WAKEUP_BY_OTHERS;
			ret = SYS_WAKEUP_OK;
			break;
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
				cpu_wakeup_by = WAKEUP_BY_OTHERS;
				break;
			} else if (ret == DMIC_TIMER){

			}
		}
		/* DMIC interrupt pending */
		if(dmic_handler() == SYS_WAKEUP_OK) {
			ret = SYS_WAKEUP_OK;
			cpu_wakeup_by = WAKEUP_BY_DMIC;
			break;
		} else {
			ret = SYS_WAKEUP_FAILED;
		}
		if(ret == SYS_WAKEUP_FAILED) {
			/* checkout dmic fifo, if we should go to sleep */
			if(cpu_should_sleep()) {

				__asm__ volatile(".set mips32\n\t"
						"wait\n\t"
						"nop\n\t"
						"nop\n\t"
						"nop\n\t"
						);
				TCSM_PCHAR('S');
			}
		}
	}
	serial_put_hex(cpu_wakeup_by);
	serial_put_hex(&cpu_wakeup_by);
	return ret;
}
int close(int mode)
{
	printk("module close\n");
	/* MASK INTC*/
	if(mode == DEEP_SLEEP) {
		REG32(0xB0001008) |= 1<< 0;
		REG32(0xB0001028) |= 1<< 0;
		dmic_disable_tri();
		wakeup_close();
	}
	if((--open_cnt) == 0) {
		printk("#############################wakeup module closed for real\n");
		dmic_disable();
		dma_close();
	}
	return 0;
}

int set_handler(void *handler)
{
	h_handler = (int(*)(const char *fmt, ...)) handler;

	return 0;
}


/* @fn: used by recorder to get address for now.
 * @return : dma trans address.
 * */
unsigned int get_dma_address(void)
{
	return pdma_trans_addr(DMA_CHANNEL, DMA_DEV_TO_MEM);
}

/* @fn: used by recorder to change dmic config.
 *
 * */
int ioctl(int cmd, unsigned long args)
{
	return dmic_ioctl(cmd, args);
}


/* @fn: used by deep sleep procedure to load module to L2 cache.
 *
 * */
#define LOAD_ADDR	0x8ff00000
#define LOAD_SIZE	(256 * 1024)
void cache_prefetch(void)
{
	//printf("####cache prefetch!!:size:(%d * 1024)Bytes\n", (LOAD_SIZE/1024));
	int i;
	volatile unsigned int *addr = (unsigned int *)0x8ff00000;
	volatile unsigned int a;
	for(i=0; i<LOAD_SIZE/32; i++) {
		a = *(addr + i * 8);
	}
}

/* @fn: used by wakeup driver to get voice_resrouce buffer address.
 *
 * */
unsigned char * get_resource_addr(void)
{
	printk("wakeup_res addrs:%08x\n", wakeup_res);
	return (unsigned char *)wakeup_res;
}

/* used by wakeup driver.
 * @return SYS_WAKEUP_FAILED, SYS_WAKEUP_OK.
 * */
int process_data(void)
{
	return process_dma_data();
}
/* used by wakeup drvier */
int is_cpu_wakeup_by_dmic(void)
{
	printk("########## cpu wakeup by:%d, #####:%p\n", cpu_wakeup_by, &cpu_wakeup_by);
	return cpu_wakeup_by == WAKEUP_BY_DMIC ? 1 : 0;
}

/* used by wakeup driver when earyl sleep. */
int set_sleep_buffer(unsigned char *buffer, unsigned long len)
{
	printk("####sleep buffer:%p, sleep_buffer_len:%d\n", buffer, len);
	sleep_buffer = buffer;
	sleep_buffer_len = len;
}
/* used by cpu eary sleep.
 * @return SYS_WAKEUP_OK, SYS_WAKEUP_FAILED.
 * */
int get_sleep_process()
{
	/* close hardware. wait deep sleep to reconfig. release open_cnt */
	dmic_disable();
	dma_close();
	open_cnt --;
	return process_buffer_data(sleep_buffer, sleep_buffer_len);
}
