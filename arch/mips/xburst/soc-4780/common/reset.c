/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/irq.h>
#include <linux/tty.h>
#include <linux/delay.h>

#include <soc/base.h>
#include <soc/cpm.h>
#include <soc/extal.h>

#include <asm/reboot.h>

#define RTC_RTCCR		(0x00)	/* rw, 32, 0x00000081 */
#define RTC_RTCSR		(0x04)	/* rw, 32, 0x???????? */
#define RTC_RTCSAR		(0x08)	/* rw, 32, 0x???????? */
#define RTC_RTCGR		(0x0c)	/* rw, 32, 0x0??????? */
#define RTC_HCR			(0x20)  /* rw, 32, 0x00000000 */
#define RTC_HWFCR		(0x24)  /* rw, 32, 0x0000???0 */
#define RTC_HRCR		(0x28)  /* rw, 32, 0x00000??0 */
#define RTC_HWCR		(0x2c)  /* rw, 32, 0x00000008 */
#define RTC_HWRSR		(0x30)  /* rw, 32, 0x00000000 */
#define RTC_HSPR		(0x34)  /* rw, 32, 0x???????? */
#define RTC_WENR		(0x3c)  /* rw, 32, 0x00000000 */
#define RTC_CKPCR		(0x40)  /* rw, 32, 0x00000010 */
#define RTC_OWIPCR		(0x44)  /* rw, 32, 0x00000010 */
#define RTC_PWRONCR		(0x48)  /* rw, 32, 0x???????? */

#define WDT_TCSR		(0x0c)  /* rw, 32, 0x???????? */
#define WDT_TCER		(0x04)  /* rw, 32, 0x???????? */
#define WDT_TDR			(0x00)  /* rw, 32, 0x???????? */
#define WDT_TCNT		(0x08)  /* rw, 32, 0x???????? */

#define RTCCR_WRDY		BIT(7)
#define WENR_WEN                BIT(31)

#define RECOVERY_SIGNATURE	(0x001a1a)
#define UNMSAK_SIGNATURE	(0x7c0000)//do not use these bits

static void inline rtc_write_reg(int reg,int value)
{
	while(!(inl(RTC_IOBASE + RTC_RTCCR) & RTCCR_WRDY));
	outl(0xa55a,(RTC_IOBASE + RTC_WENR));
	while(!(inl(RTC_IOBASE + RTC_RTCCR) & RTCCR_WRDY));
	while(!(inl(RTC_IOBASE + RTC_WENR) & WENR_WEN));
	while(!(inl(RTC_IOBASE + RTC_RTCCR) & RTCCR_WRDY));
	outl(value,(RTC_IOBASE + reg));
	while(!(inl(RTC_IOBASE + RTC_RTCCR) & RTCCR_WRDY));
}

#define HWFCR_WAIT_TIME(x) ((0x3ff-((0x3ff*(x))/2000)) << 5)

void jz_hibernate(void)
{
	local_irq_disable();
	/* Set minimum wakeup_n pin low-level assertion time for wakeup: 100ms */
	rtc_write_reg(RTC_HWFCR, HWFCR_WAIT_TIME(100));

	/* Set reset pin low-level assertion time after wakeup: must  > 60ms */
	rtc_write_reg(RTC_HRCR, (60 << 5));

	/* clear wakeup status register */
	rtc_write_reg(RTC_HWRSR, 0x0);

	rtc_write_reg(RTC_HWCR, 0x8);

	/* Put CPU to hibernate mode */
	rtc_write_reg(RTC_HCR, 0x1);

	mdelay(200);

	while(1) 
		printk("We should NOT come here.%08x\n",inl(RTC_IOBASE + RTC_HCR));
}

void jz_wdt_restart(char *command)
{
	printk("Restarting after 4 ms\n");
	if ((command != NULL) && !strcmp(command, "recovery")) {
		while(cpm_inl(CPM_CPPSR) != RECOVERY_SIGNATURE) {
			printk("set RECOVERY_SIGNATURE\n");
			cpm_outl(0x5a5a,CPM_CPSPPR);
			cpm_outl(RECOVERY_SIGNATURE,CPM_CPPSR);
			cpm_outl(0x0,CPM_CPSPPR);
			udelay(100);
		}
	} else {
		cpm_outl(0x5a5a,CPM_CPSPPR);
		cpm_outl(0x0,CPM_CPPSR);
		cpm_outl(0x0,CPM_CPSPPR);
	}

	*((volatile unsigned int *)(0xb000203c)) |= 1<<16;
	outl((1<<3 | 1<<2),WDT_IOBASE + WDT_TCSR);
	outl(JZ_EXTAL/1000,WDT_IOBASE + WDT_TDR);
	outl(0,WDT_IOBASE + WDT_TCNT);
	outl(1,WDT_IOBASE + WDT_TCER);

	mdelay(200);

	while (1)
		printk("We should NOT come here, please check the WDT\n");
}

#ifdef CONFIG_HIBERNATE_RESET
void jz_hibernate_restart(char *command)
{
	uint32_t rtc_rtcsr,rtc_rtccr;

	local_irq_disable();

	if ((command != NULL) && !strcmp(command, "recovery")) {
		jz_wdt_restart(command);
	}

	while(!(inl(RTC_IOBASE + RTC_RTCCR) & RTCCR_WRDY));
	rtc_rtcsr = inl(RTC_IOBASE + RTC_RTCSR);
	rtc_rtccr = inl(RTC_IOBASE + RTC_RTCCR);
	rtc_write_reg(RTC_RTCSAR,rtc_rtcsr + 5);
	rtc_write_reg(RTC_RTCCR,rtc_rtccr | 0x3<<2);

      	/* Clear reset status */
	cpm_outl(0,CPM_RSR);

	/* Set minimum wakeup_n pin low-level assertion time for wakeup: 100ms */
	rtc_write_reg(RTC_HWFCR, HWFCR_WAIT_TIME(100));

	/* Set reset pin low-level assertion time after wakeup: must  > 60ms */
	rtc_write_reg(RTC_HRCR, (60 << 5));

	/* clear wakeup status register */
	rtc_write_reg(RTC_HWRSR, 0x0);

	rtc_write_reg(RTC_HWCR, 0x9);

	/* Put CPU to hibernate mode */
	rtc_write_reg(RTC_HCR, 0x1);

	mdelay(200);

	while(1) 
		printk("We should NOT come here.%08x\n",inl(RTC_IOBASE + RTC_HCR));

}
#endif

int __init reset_init(void)
{
	pm_power_off = jz_hibernate;
#ifdef CONFIG_HIBERNATE_RESET
	_machine_restart = jz_hibernate_restart;
#else
	_machine_restart = jz_wdt_restart;
#endif
	return 0;
}
arch_initcall(reset_init);



