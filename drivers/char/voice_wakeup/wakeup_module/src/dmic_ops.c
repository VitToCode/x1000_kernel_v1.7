#include <jz_cpm.h>
#include <jz_gpio.h>
#include "dmic_config.h"
#include "dmic_ops.h"
#include "jz_dma.h"
#include "voice_wakeup.h"
#include "jz_dmic.h"
#include "interface.h"
#include "common.h"
#include "trigger_value_adjust.h"


int dmic_current_state;
int thr_need_reconfig;
unsigned int cur_thr_value;
unsigned int wakeup_failed_times;
unsigned int dmic_recommend_thr;
unsigned int last_dma_count;

//#define DMIC_FIFO_THR	(48) /* 48 * 4Bytes, or 48 * 2 Bytes.*/
#define DMIC_FIFO_THR	(64) /* 48 * 4Bytes, or 48 * 2 Bytes.*/
//#define DMIC_FIFO_THR	(128)


#define __dmic_reset()									\
	do {												\
		REG_DMIC_CR0 |= (1<< DMICCR0_RESET);		\
		while (REG_DMIC_CR0 & (1 << DMICCR0_RESET));		\
	} while (0)
#define __dmic_reset_tri()			\
	do {							\
		REG_DMIC_CR0 |= (1 << DMICCR0_RESET_TRI);		\
		while(REG_DMIC_CR0 & (1<<DMICCR0_RESET_TRI));	\
	} while(0)

void dump_dmic_regs(void)
{
	printf("REG_DMIC_CR0			%08x\n",	REG_DMIC_CR0		);
	printf("REG_DMIC_GCR            %08x\n",    REG_DMIC_GCR		);
	printf("REG_DMIC_IMR            %08x\n",    REG_DMIC_IMR		);
	printf("REG_DMIC_ICR            %08x\n",    REG_DMIC_ICR		);
	printf("REG_DMIC_TRICR          %08x\n",    REG_DMIC_TRICR	);
	printf("REG_DMIC_THRH           %08x\n",    REG_DMIC_THRH		);
	printf("REG_DMIC_THRL           %08x\n",    REG_DMIC_THRL		);
	printf("REG_DMIC_TRIMMAX        %08x\n",    REG_DMIC_TRIMMAX	);
	printf("REG_DMIC_TRINMAX        %08x\n",    REG_DMIC_TRINMAX	);
	printf("REG_DMIC_DR             %08x\n",    REG_DMIC_DR		);
	printf("REG_DMIC_FCR            %08x\n",    REG_DMIC_FCR		);
	printf("REG_DMIC_FSR            %08x\n",    REG_DMIC_FSR		);
	printf("REG_DMIC_CGDIS          %08x\n",    REG_DMIC_CGDIS	);
}


void dump_gpio()
{
	printf("REG_GPIO_PXINT(5)%08x\n", REG_GPIO_PXINT(5));
	printf("REG_GPIO_PXMASK(5):%08x\n", REG_GPIO_PXMASK(5));
	printf("REG_GPIO_PXPAT1(5):%08x\n", REG_GPIO_PXPAT1(5));
	printf("REG_GPIO_PXPAT0(5):%08x\n", REG_GPIO_PXPAT0(5));
}
void dump_cpm_reg()
{
	printf("CPM_LCR:%08x\n", REG32(CPM_IOBASE + CPM_LCR));
	printf("CPM_CLKGR0:%08x\n", REG32(CPM_IOBASE + CPM_CLKGR0));
	printf("CPM_CLKGR1:%08x\n", REG32(CPM_IOBASE + CPM_CLKGR1));
	printf("CPM_SPCR0:%08x\n",  REG32(CPM_IOBASE + CPM_SPCR0));
	printf("CPM_SPCR1:%08x\n", REG32(CPM_IOBASE + CPM_SPCR1));
	printf("I2S_DEVCLK:%08x\n", REG32(CPM_IOBASE + CPM_I2SCDR));
}

void dmic_clk_config(void)
{
	/*1. cgu config 12MHz , extern clock */
	/* dmic need a 12MHZ clock from cpm */
	REG32(CPM_IOBASE + CPM_LCR)		&= ~(1<<7);

	REG32(CPM_IOBASE + CPM_I2SCDR) |= 2 << 30;
	REG32(CPM_IOBASE + CPM_I2SCDR) |= 1 << 29; /*CE*/
	REG32(CPM_IOBASE + CPM_I2SCDR) &= ~0xFF; /*24Mhz/2*/
	REG32(CPM_IOBASE + CPM_I2SCDR) |= 1; /*24Mhz/2*/

	while(REG32(CPM_IOBASE + CPM_I2SCDR) &(1<<28));

	printf("####### dmic clk config ok!!!\n");

	REG32(CPM_IOBASE + CPM_I2SCDR) &= ~(1 << 27);

	REG32(CPM_IOBASE + CPM_I2SCDR) &= ~(1 << 29); /*CE*/

	/*2.gate clk*/

	REG32(CPM_IOBASE + CPM_CLKGR1) &= ~(1 << 7); /* clk on */

	/*3.extern clk sleep mode enable*/
	REG32(CPM_IOBASE + CPM_OPCR) |= 1 << 4;

	dump_cpm_reg();
}
void gpio_as_dmic(void)
{
	/* dmic 0, PF06,PF07, func 3*/
	REG_GPIO_PXINTC(5)	|= 3 << 6;
	REG_GPIO_PXMASKC(5) |= 3 << 6;
	REG_GPIO_PXPAT1S(5) |= 3 << 6;
	REG_GPIO_PXPAT0S(5) |= 3 << 6;

	/* dmic 1, PF10, PF11, func 1*/
	REG_GPIO_PXINTC(5)	|= 3 << 10;
	REG_GPIO_PXMASKC(5) |= 3 << 10;
	REG_GPIO_PXPAT1C(5) |= 3 << 10;
	REG_GPIO_PXPAT0S(5) |= 3 << 10;
	dump_gpio();
}

void dmic_init(void)
{
	gpio_as_dmic();
	dmic_clk_config();

	__dmic_reset();
	__dmic_reset_tri();

}

int cpu_should_sleep(void)
{
	return ((REG_DMIC_FSR & 0x7f) < (DMIC_FIFO_THR - 20)) &&(last_dma_count != DMADTC(5)) ? 1 : 0;
	//return ((REG_DMIC_FSR & 0x7f) < (DMIC_FIFO_THR - 20)) ? 1 : 0;
}

int dmic_init_mode(int mode)
{
	dmic_init();
	switch (mode) {
		case EARLY_SLEEP:

			break;
		case DEEP_SLEEP:
			dmic_current_state = WAITING_TRIGGER;
			wakeup_failed_times = 0;
			thr_need_reconfig = 0;
			//cur_thr_value = 0;
			cur_thr_value = thr_table[0];
			dmic_recommend_thr = 0;

			REG_DMIC_CR0 |= 3<<6;
			//REG_DMIC_CR0 &= ~(3<<6);

			/*packen ,unpack disable*/
			REG_DMIC_CR0 |= 1<<8 | 1 << 12;
			//REG_DMIC_CR0 &= ~(1<<8 | 1 << 12);

			REG_DMIC_FCR |= 1 << 31 | DMIC_FIFO_THR;
			//REG_DMIC_IMR &= ~(0x1f);
			REG_DMIC_IMR |= 0x1f; /*mask all ints*/
			REG_DMIC_GCR = 9;
			REG_DMIC_CR0 |= 1<<2; /*hpf1en*/
			REG_DMIC_THRL = cur_thr_value; /*SET A MIDDLE THR_VALUE, AS INIT */

			REG_DMIC_TRICR	|= 1 <<3; /*hpf2 en*/
			REG_DMIC_TRICR	|= 2 << 1; /* prefetch 16k*/

			/* trigger mode config */
			REG_DMIC_TRICR &= ~(0xf << 16); /* tri mode: larger than thr trigger.*/
			//REG_DMIC_TRICR |= 2 << 16; /* > N times of thrl. */
			//REG_DMIC_TRINMAX = 30;
			//REG_DMIC_TRINMAX = 5;

			REG_DMIC_CR0 |= 1<<1; /*ENABLE DMIC tri*/
			REG_DMIC_ICR |= 0x1f; /*clear all ints*/
			REG_DMIC_IMR &= ~(1 << 0);/*enable tri int*/
			REG_DMIC_IMR &= ~(1 << 4);/*enable tri int*/

			break;
		case NORMAL_RECORD:
			REG_DMIC_CR0 &= ~(3<<6);
			REG_DMIC_CR0 |= 1<<6;

			/*packen ,unpack disable*/
			REG_DMIC_CR0 |= 1<<8 | 1 << 12;
			//REG_DMIC_CR0 &= ~(1<<8 | 1 << 12);

			REG_DMIC_FCR |= 1 << 31 | 64;
			//REG_DMIC_IMR &= ~(0x1f);
			REG_DMIC_IMR |= 0x1f; /*mask all ints*/
			REG_DMIC_ICR |= 0x1f; /*mask all ints*/
			REG_DMIC_GCR = 9;

			REG_DMIC_CR0 |= 1<<2; /*hpf1en*/

			REG_DMIC_TRICR &= ~(0xf << 16);
			REG_DMIC_TRICR |= 0 << 16; /*trigger mode*/
			REG_DMIC_TRICR	|= 1 <<3; /*hpf2 en*/
			REG_DMIC_TRICR	|= 2 << 1; /* prefetch 16k*/

			break;
		default:
			break;

	}
	return 0;
}

int dmic_enable(void)
{
	REG_DMIC_CR0 |= 1 << 0; /*ENABLE DMIC*/

	dump_dmic_regs();
	return 0;
}

int dmic_disable_tri(void)
{
	REG_DMIC_CR0 &= ~(1<<1);
	REG_DMIC_ICR |= 0x1f;
	REG_DMIC_IMR |= 0x1f;

	return 0;
}
int dmic_disable(void)
{
	/*controller disable*/
	//REG_DMIC_CR0 &= ~(1<<1); /*DISABLE DMIC tri*/
	REG_DMIC_CR0 &= ~(1 << 0); /*DISABLE DMIC*/
	/*clear ints*/
	return 0;
}



void reconfig_thr_value()
{
	if(dmic_current_state == WAITING_TRIGGER) {
		/* calle by rtc timer, or last wakeup failed .*/
		if(dmic_recommend_thr != 0) {
			REG_DMIC_THRL = dmic_recommend_thr;
			dmic_recommend_thr = 0;
		} else {
			REG_DMIC_THRL = adjust_trigger_value(wakeup_failed_times, cur_thr_value);
		}
	} else if(dmic_current_state == WAITING_DATA) {
		/* called only by rtc timer, we can not change thr here.
		 * should wait dmic waiting trigger state.
		 * */
		dmic_recommend_thr = adjust_trigger_value(wakeup_failed_times+1, cur_thr_value);
	}
	wakeup_failed_times = 0;
	//serial_put_hex(REG_DMIC_THRL);
}

int dmic_set_samplerate(unsigned long rate)
{
	int ret;
	if(rate == 8000) {
		REG_DMIC_CR0 &= ~(3<<6);
		ret = 0;
	} else if(rate == 16000) {
		REG_DMIC_CR0 &= ~(3<<6);
		REG_DMIC_CR0 |= (1<<6);
		ret = 0;
	} else {
		printk("dmic does not support samplerate:%d\n", rate);
		ret = -1;
	}

	return ret;

}
int dmic_ioctl(int cmd, unsigned long args)
{
	switch (cmd) {
		case DMIC_IOCTL_SET_SAMPLERATE:
			dmic_set_samplerate(args);
			break;
		default:
			break;
	}

	return 0;
}

int dmic_handler(void)
{
	volatile int ret;
	/* we only handle wakeup and trigger ints */
	//REG_DMIC_CR0 |= 1<<6; /* set sample rate 16K*/
	//REG_DMIC_CR0 &= ~(1<<7); /* set sample rate 16K*/
	REG_DMIC_ICR |= 0x1f; /* CLEAR ALL INTS*/
	REG_DMIC_IMR |= 1<<0 | 1<<4; /* MASK TRIGGER, wakeup */

	last_dma_count = DMADTC(5);

	if(dmic_current_state == WAITING_TRIGGER) {
		dmic_current_state = WAITING_DATA;
		/*change trigger value to 0, make dmic wakeup cpu all the time*/
		REG_DMIC_THRL = 0;
	} else if (dmic_current_state == WAITING_DATA){

	}

	ret = process_dma_data_2();
	//serial_put_hex(ret);
	if(ret == SYS_WAKEUP_OK) {
		return SYS_WAKEUP_OK;
		/*just ok*/
	} else if(ret == SYS_NEED_DATA) {
		/*
		 * if voice process module need more data to anaylize,
		 * then, we keep dmic data path on, and also open dmic
		 * trigger logic.
		 * */

		REG_DMIC_CR0 |= 3 << 6;
		REG_DMIC_IMR &= ~( 1<<4 | 1<<0);

		last_dma_count = DMADTC(5);
		return SYS_NEED_DATA;
	} else if(ret == SYS_WAKEUP_FAILED) {
		/*
		 * if current wakeup operation failed. we need reconfig dmic
		 * to work at appropriate mode.
		 * */
		dmic_current_state = WAITING_TRIGGER;

		reconfig_thr_value();

		/* change trigger mode to > N times*/
		//REG_DMIC_TRICR |= 2 << 16;
		REG_DMIC_TRICR |= 1<<0; /*clear trigger*/

		REG_DMIC_CR0 |= 3<<6; /* disable data path*/

		REG_DMIC_ICR |= 0x1f;
		REG_DMIC_IMR &= ~(1<<0 | 1<<4);

		return SYS_WAKEUP_FAILED;
	}

	return SYS_WAKEUP_FAILED;
}
