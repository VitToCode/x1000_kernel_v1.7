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
#include <linux/proc_fs.h>
#include <linux/kthread.h>

#include <soc/base.h>
#include <soc/cpm.h>
#include <soc/extal.h>
#include <soc/tcu.h>

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
#define REBOOT_SIGNATURE	(0x003535)
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
		cpm_outl(REBOOT_SIGNATURE,CPM_CPPSR);
		cpm_outl(0x0,CPM_CPSPPR);
	}


	outl((1<<3 | 1<<2),WDT_IOBASE + WDT_TCSR);
	outl(JZ_EXTAL/1000,WDT_IOBASE + WDT_TDR);

	outl(1 << 16,TCU_IOBASE + TCU_TSCR);
	outl(0,WDT_IOBASE + WDT_TCNT);
	outl(1,WDT_IOBASE + WDT_TCER);

	mdelay(200);

	while (1)
		printk("We should NOT come here, please check the WDT\n");
}
static void hibernate_restart(void) {
	uint32_t rtc_rtcsr,rtc_rtccr;
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
#ifdef CONFIG_HIBERNATE_RESET
void jz_hibernate_restart(char *command)
{
	local_irq_disable();

	if ((command != NULL) && !strcmp(command, "recovery")) {
		jz_wdt_restart(command);
	}

	hibernate_restart();
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

static char *reset_command[] = {"wdt","hibernate","recovery"};

static int reset_read_proc(char *page, char **start, off_t off,
			  int count, int *eof, void *data){
	int len = 0,i;
	for(i = 0;i < ARRAY_SIZE(reset_command);i++)
		len += sprintf(page+len,"%s\t",reset_command[i]);
	len += sprintf(page+len,"\n");
	return len;
}

static int reset_write_proc(struct file *file, const char __user *buffer,
			    unsigned long count, void *data) {
	int command = 0;
	int i;

	if(count == 0) return count;
	for(i = 0;i < ARRAY_SIZE(reset_command);i++) {
		if(!strncmp(buffer,reset_command[i],strlen(reset_command[i]))) {
			command++;
			break;
		}
	}
	if(command == 0) return count;
	local_irq_disable();
	switch(command) {
	case 1:
		jz_wdt_restart(NULL);
		break;
	case 2:
		hibernate_restart();
		break;
	case 3:
		jz_wdt_restart("recovery");
		break;

	}
	return count;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
struct wdt_reset {
	unsigned stop;
	unsigned msecs;
	struct task_struct *task;
};

static int reset_task(void *data) {
	struct wdt_reset *wdt = data;
	int time = JZ_EXTAL_RTC / 64 * (wdt->msecs) / 1000;
	set_user_nice(current, -5);

	outl(1 << 16,TCU_IOBASE + TCU_TSCR);
	outl(0,WDT_IOBASE + WDT_TCER);
	outl((3<<3 | 1<<1),WDT_IOBASE + WDT_TCSR);
	outl(0,WDT_IOBASE + WDT_TCNT);				//counter
	outl(time>65535?65535:time,WDT_IOBASE + WDT_TDR); 	//data
	outl(1,WDT_IOBASE + WDT_TCER);
#if 0
	while(1){
		outl(0,WDT_IOBASE + WDT_TCNT);				//counter
		msleep(100);
		printk("---- %d\n",inl(WDT_IOBASE + WDT_TCNT));
		outl(0,WDT_IOBASE + WDT_TCNT);				//counter
		msleep(1000);
		printk("---- %d\n",inl(WDT_IOBASE + WDT_TCNT));
	}
#endif
	while (!kthread_should_stop()) {
		outl(0,WDT_IOBASE + WDT_TCNT);
		msleep(wdt->msecs - 100);
	}

	outl(0,WDT_IOBASE + WDT_TCER);
	return 0;
}

static int wdt_control_read_proc(char *page, char **start, off_t off,
			  int count, int *eof, void *data){
	int len = 0;
	struct wdt_reset *wdt = data;
	len += sprintf(page+len,wdt->stop?">off<on\n":"off>on<\n");
	return len;
}

static int wdt_control_write_proc(struct file *file, const char __user *buffer,
			    unsigned long count, void *data) {
	static int i = 1;
	struct wdt_reset *wdt = data;
	if(!strncmp(buffer,"on",2) && (wdt->stop == 1)) {
		wdt->task = kthread_run(reset_task, wdt, "reset_task%d",i++);
		wdt->stop = 0;
	} else if(!strncmp(buffer,"off",3) && (wdt->stop == 0)) {
		outl(0,WDT_IOBASE + WDT_TCNT);
		outl(0,WDT_IOBASE + WDT_TCER);
		outl(1 << 16,TCU_IOBASE + TCU_TSSR);
		kthread_stop(wdt->task);
		wdt->stop = 1;
	}
	return count;
}
	
static int wdt_time_read_proc(char *page, char **start, off_t off,
			  int count, int *eof, void *data){
	int len = 0;
	struct wdt_reset *wdt = data;
	len += sprintf(page+len,"%d msecs\n",wdt->msecs);
	return len;
}

static int wdt_time_write_proc(struct file *file, const char __user *buffer,
			    unsigned long count, void *data) {
	int time;
	unsigned msecs= 0;
	struct wdt_reset *wdt = data;

	if(!wdt->stop)
		return -EBUSY;

	sscanf(buffer,"%d\n",&msecs);
	if(msecs < 1000) msecs = 1000;
	if(msecs > 30000) msecs = 30000;

	wdt->msecs = msecs;
	time = JZ_EXTAL_RTC / 64 * (wdt->msecs) / 1000;
	outl(0,WDT_IOBASE + WDT_TCNT);			//counter
	outl(time>65535?65535:time,WDT_IOBASE + WDT_TDR); 	//data
	return count;
}
	
static int __init init_reset_proc(void)
{
	struct wdt_reset *wdt;
	struct proc_dir_entry *p,*res;

	wdt = kmalloc(sizeof(struct wdt_reset),GFP_KERNEL);
	if(!wdt) {
		return -ENOMEM;
	}

	wdt->msecs = 3000;
	wdt->stop = 1;

	p = proc_mkdir("reset", 0);
	if (!p) {
		pr_warning("create_proc_entry for common reset failed.\n");
		return -ENODEV;
	}

	res = create_proc_entry("reset", 0444, p);
	if (res) {
		res->read_proc = reset_read_proc;
		res->write_proc = reset_write_proc;
		res->data = wdt;
	}

	res = create_proc_entry("wdt_control", 0444, p);
	if (res) {
		res->read_proc = wdt_control_read_proc;
		res->write_proc = wdt_control_write_proc;
		res->data = wdt;
	}
	
	res = create_proc_entry("wdt_time", 0444, p);
	if (res) {
		res->read_proc = wdt_time_read_proc;
		res->write_proc = wdt_time_write_proc;
		res->data = wdt;
	}

	return 0;
}

static int __init init_reset(void)
{
	return init_reset_proc();
}
module_init(init_reset);



