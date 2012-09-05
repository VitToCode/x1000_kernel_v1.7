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
	outl(value,(RTC_IOBASE + reg));
	while(!(inl(RTC_IOBASE + RTC_RTCCR) & RTCCR_WRDY));
}

#define HWFCR_WAIT_TIME(x) ((0x3ff-((0x3ff*(x))/2000)) << 5)

void jz_hibernate(void)
{
	/* Set minimum wakeup_n pin low-level assertion time for wakeup: 100ms */
	rtc_write_reg(RTC_HWFCR, HWFCR_WAIT_TIME(100));

	/* Set reset pin low-level assertion time after wakeup: must  > 60ms */
	rtc_write_reg(RTC_HRCR, (0x1 << 5));

	/* clear wakeup status register */
	rtc_write_reg(RTC_HWRSR, 0x0);

	/* Put CPU to hibernate mode */
	rtc_write_reg(RTC_HCR, 0x1);

	mdelay(200);

	while(1) 
		printk("We should NOT come here.\n");
}

int __init reset_init(void)
{
	pm_power_off = jz_hibernate;

	return 0;
}
arch_initcall(reset_init);

#if 0
void jz_hibernate_restart(char *command)
{
	uint32_t rtc_rtcsr;
	pm_sync_filesystem();
	if ((command != NULL) && !strcmp(command, "recovery")) {
		jz_wdt_restart(command);
	} else {
		cpm_set_scrpad(0);
	}

	rtc_rtcsr = rtc_read_reg(RTC_RTCSR);
	rtc_write_reg(RTC_RTCSAR,rtc_rtcsr + 2);
	rtc_set_reg(RTC_RTCCR,0x3<<2);

	/* Mask all interrupts */
	OUTREG32(INTC_ICMSR(0), 0xffffffff);
	OUTREG32(INTC_ICMSR(1), 0x7ff);

      	/* Clear reset status */
	CLRREG32(CPM_RSR, RSR_PR | RSR_WR | RSR_P0R);

	/* Set minimum wakeup_n pin low-level assertion time for wakeup: 100ms */
	rtc_write_reg(RTC_HWFCR, HWFCR_WAIT_TIME(100));

	/* Set reset pin low-level assertion time after wakeup: must  > 60ms */
	rtc_write_reg(RTC_HRCR, HRCR_WAIT_TIME(60));

	/* Scratch pad register to be reserved */
	rtc_write_reg(RTC_HSPR, HSPR_RTCV);

	/* clear wakeup status register */
	rtc_write_reg(RTC_HWRSR, 0x0);

	/* set wake up valid level as low  and disable rtc alarm wake up.*/
	rtc_write_reg(RTC_HWCR,0x9);

	/* Put CPU to hibernate mode */
	rtc_write_reg(RTC_HCR, HCR_PD);

	while (1)
		printk("We should NOT come here, please check the RTC\n");
}

void jz_wdt_restart(char *command)
{
	printk("Restarting after 4 ms\n");
	if ((command != NULL) && !strcmp(command, "recovery")) {
		while(cpm_get_scrpad() != RECOVERY_SIGNATURE) {
			printk("set RECOVERY_SIGNATURE\n");
			cpm_set_scrpad(RECOVERY_SIGNATURE);
			msleep(20);
		}
	} else {
		cpm_set_scrpad(0);
	}

	REG_WDT_WCSR = WCSR_PRESCALE4 | WCSR_CLKIN_EXT;
	REG_WDT_WCNT = 0;
	REG_WDT_WDR = JZ_EXTAL/1000;   /* reset after 4ms */
	REG_TCU_TSCR = TSCR_WDT; /* enable wdt clock */
	REG_WDT_WCER = WCER_TCEN;  /* wdt start */
	while (1)
		printk("We should NOT come here, please check the RTC\n");
}
#endif

