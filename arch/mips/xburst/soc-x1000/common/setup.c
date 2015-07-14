/*
 * JZSOC Clock and Power Manager
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2006 Ingenic Semiconductor Inc.
 */
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/proc_fs.h>
#include <linux/ioport.h>

#include <soc/cpm.h>
#include <soc/base.h>
#include <soc/extal.h>
#include <mach/jzcpm_pwc.h>
#include <mach/platform.h>

extern void reset_keep_power(void);


void __init cpm_reset(void)
{
	unsigned int cpm_clkgr;

	cpm_clkgr = cpm_inl(CPM_CLKGR);
	cpm_clkgr |= 0x47da3fff;
	cpm_clkgr = 0;
	/* cpm_clkgr &= (~(1 << 21)); */
	cpm_outl(cpm_clkgr, CPM_CLKGR);

        /* warn:when switch cpu freq ,the 23(sclka mux) bit should be set */
        /* cpm_set_bit(23,CPM_CPCCR); */
	mdelay(1);
}

int __init setup_init(void)
{
	cpm_reset();
#ifdef CONFIG_RESET_KEEP_POWER
	reset_keep_power();
#endif
	return 0;
}
void __cpuinit jzcpu_timer_setup(void);
void __cpuinit jz_clocksource_init(void);
void __init init_all_clk(void);
/* used by linux-mti code */
int coherentio;			/* init to 0, no DMA cache coherency */
int hw_coherentio;		/* init to 0, no HW DMA cache coherency */

void __init plat_mem_setup(void)
{
	/* jz mips cpu special */
	__asm__ (
		"li    $2, 0xa9000000 \n\t"
		"mtc0  $2, $5, 4      \n\t"
		"nop                  \n\t"
		::"r"(2));

	/* use IO_BASE, so that we can use phy addr on hard manual
	 * directly with in(bwlq)/out(bwlq) in io.h.
	 */
	set_io_port_base(IO_BASE);
	ioport_resource.start	= 0x00000000;
	ioport_resource.end	= 0xffffffff;
	iomem_resource.start	= 0x00000000;
	iomem_resource.end	= 0xffffffff;
	setup_init();
	init_all_clk();

	return;
}

void __init plat_time_init(void)
{
	jzcpu_timer_setup();
	jz_clocksource_init();
}
