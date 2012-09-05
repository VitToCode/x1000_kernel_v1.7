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
#include <linux/module.h>
#include <linux/err.h>
#include <linux/proc_fs.h>

#include <soc/cpm.h>
#include <soc/base.h>
#include <soc/extal.h>

void __init cpm_reset(void)
{
	unsigned long lcr = cpm_inl(CPM_LCR);
	
	cpm_outl(lcr | CPM_LCR_PD_MASK,CPM_LCR);
	while((cpm_inl(CPM_LCR) & (0x7<<24)) != (0x7<<24));

	cpm_outl(0,CPM_PSWC0ST);
	cpm_outl(8,CPM_PSWC1ST);
	cpm_outl(11,CPM_PSWC2ST);
	cpm_outl(0,CPM_PSWC3ST);
}

int __init setup_init(void)
{
	cpm_reset();

	return 0;
}
arch_initcall(setup_init);

