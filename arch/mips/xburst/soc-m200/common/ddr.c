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
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/syscore_ops.h>
#include <linux/vmalloc.h>
#include <linux/suspend.h>
#include <linux/cpufreq.h>

#include <soc/ddr.h>
#include <soc/base.h>
#include <soc/extal.h>
#include <jz_proc.h>

/*
 *  unit: kHz
 */

typedef int (*DUMP_CALLBACK)(char *, const char *, ...);

int dump_out_ddr(char *str, DUMP_CALLBACK dump_callback)
{
	int len = 0;
	int i = 0;
	len += dump_callback(str + len,"--------------------dump the DDRC1---------------\n");

	len += dump_callback(str + len,"DDRC_STATUS\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_STATUS),DDRC_BASE + DDRC_STATUS);
	len += dump_callback(str + len,"DDRC_CFG\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_CFG),DDRC_BASE + DDRC_CFG);
	len += dump_callback(str + len,"DDRC_CTRL\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_CTRL),DDRC_BASE + DDRC_CTRL);
	len += dump_callback(str + len,"DDRC_LMR\t:0x%08x\taddress\t:0x%08x\n",ddr_readl(DDRC_LMR),DDRC_BASE + DDRC_LMR);
	len += dump_callback(str + len,"DDRC_TIMING1\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_TIMING(1)),DDRC_BASE + DDRC_TIMING(1));
	len += dump_callback(str + len,"DDRC_TIMING2\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_TIMING(2)),DDRC_BASE + DDRC_TIMING(2));
	len += dump_callback(str + len,"DDRC_TIMING3\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_TIMING(3)),DDRC_BASE + DDRC_TIMING(3));
	len += dump_callback(str + len,"DDRC_TIMING4\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_TIMING(4)),DDRC_BASE + DDRC_TIMING(4));
	len += dump_callback(str + len,"DDRC_TIMING5\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_TIMING(5)),DDRC_BASE + DDRC_TIMING(5));
	len += dump_callback(str + len,"DDRC_TIMING6\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_TIMING(6)),DDRC_BASE + DDRC_TIMING(6));
	len += dump_callback(str + len,"DDRC_REFCNT\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_REFCNT),DDRC_BASE + DDRC_REFCNT);
	len += dump_callback(str + len,"DDRC_MMAP0\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_MMAP0),DDRC_BASE + DDRC_MMAP0);
	len += dump_callback(str + len,"DDRC_MMAP1\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_MMAP1),DDRC_BASE + DDRC_MMAP1);
	len += dump_callback(str + len,"DDRC_REMAP1\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_REMAP(1)),DDRC_BASE + DDRC_REMAP(1));
	len += dump_callback(str + len,"DDRC_REMAP2\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_REMAP(2)),DDRC_BASE + DDRC_REMAP(2));
	len += dump_callback(str + len,"DDRC_REMAP3\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_REMAP(3)),DDRC_BASE + DDRC_REMAP(3));
	len += dump_callback(str + len,"DDRC_REMAP4\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_REMAP(4)),DDRC_BASE + DDRC_REMAP(4));
	len += dump_callback(str + len,"DDRC_REMAP5\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_REMAP(5)),DDRC_BASE + DDRC_REMAP(5));
	len += dump_callback(str + len,"DDRC_AUTOSR_EN\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_AUTOSR_EN),DDRC_BASE + DDRC_AUTOSR_EN);

	len += dump_callback(str + len,"--------------------dump the DDRP---------------\n");

	len += dump_callback(str + len,"DDRP_PIR\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRP_PIR),DDRC_BASE + DDRP_PIR);
	len += dump_callback(str + len,"DDRP_PGCR\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRP_PGCR),DDRC_BASE + DDRP_PGCR);
	len += dump_callback(str + len,"DDRP_PGSR\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRP_PGSR),DDRC_BASE + DDRP_PGSR);
	len += dump_callback(str + len,"DDRP_PTR0\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRP_PTR0),DDRC_BASE + DDRP_PTR0);
	len += dump_callback(str + len,"DDRP_PTR1\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRP_PTR1),DDRC_BASE + DDRP_PTR1);
	len += dump_callback(str + len,"DDRP_PTR2\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRP_PTR2),DDRC_BASE + DDRP_PTR2);
	len += dump_callback(str + len,"DDRP_DSGCR\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRP_DSGCR),DDRC_BASE + DDRP_DSGCR);
	len += dump_callback(str + len,"DDRP_DCR\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRP_DCR),DDRC_BASE + DDRP_DCR);
	len += dump_callback(str + len,"DDRP_DTPR0\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRP_DTPR0),DDRC_BASE + DDRP_DTPR0);
	len += dump_callback(str + len,"DDRP_DTPR1\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRP_DTPR1),DDRC_BASE + DDRP_DTPR1);
	len += dump_callback(str + len,"DDRP_DTPR2\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRP_DTPR2),DDRC_BASE + DDRP_DTPR2);
	len += dump_callback(str + len,"DDRP_MR0\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRP_MR0),DDRC_BASE + DDRP_MR0);
	len += dump_callback(str + len,"DDRP_MR1\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRP_MR1),DDRC_BASE + DDRP_MR1);
	len += dump_callback(str + len,"DDRP_MR2\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRP_MR2),DDRC_BASE + DDRP_MR2);
	len += dump_callback(str + len,"DDRP_MR3\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRP_MR3),DDRC_BASE + DDRP_MR3);
	len += dump_callback(str + len,"DDRP_ODTCR\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRP_ODTCR),DDRC_BASE + DDRP_ODTCR);
	for(i=0;i<4;i++) {
		len += dump_callback(str + len,"DX%dGSR0 \t:0x%08x\taddress\t:0x%08x\n", i, ddr_readl(DDRP_DXGSR0(i)),DDRC_BASE + DDRP_DXGSR0(i));
		len += dump_callback(str + len,"@pas:DXDQSTR(%d)\t:0x%08x\taddress\t:0x%08x\n", i,ddr_readl(DDRP_DXDQSTR(i)),DDRC_BASE + DDRP_DXDQSTR(i));
	}
	return len;
}
static int ddr_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	int len = 0;
	len += dump_out_ddr(page,sprintf);
	return len;
}

static int __init init_ddr_proc(void)
{
	struct proc_dir_entry *res,*p;

#ifdef CONFIG_DDR_DEBUG
	register int bypassmode = 0;
	register int AutoSR_en = 0;

	bypassmode = ddr_readl(DDRP_PIR) & DDRP_PIR_DLLBYP;
	if(bypassmode) {
		printk("the ddr is in bypass mode\n");
	}else{
		printk("the ddr it not in bypass mode\n");
	}

	AutoSR_en = ddr_readl(DDRC_AUTOSR_EN) & DDRC_AUTOSR_ENABLE;
	if(AutoSR_en) {
		printk("the ddr self_refresh is enable\n");
	}else{
		printk("the ddr self_refrese is not enable\n");
	}

	printk("the ddr remap register is\n");
	printk("DDRC_REMAP1\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_REMAP(1)),DDRC_BASE + DDRC_REMAP(1));
	printk("DDRC_REMAP2\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_REMAP(2)),DDRC_BASE + DDRC_REMAP(2));
	printk("DDRC_REMAP3\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_REMAP(3)),DDRC_BASE + DDRC_REMAP(3));
	printk("DDRC_REMAP4\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_REMAP(4)),DDRC_BASE + DDRC_REMAP(4));
	printk("DDRC_REMAP5\t:0x%08x\taddress\t:0x%08x\n", ddr_readl(DDRC_REMAP(5)),DDRC_BASE + DDRC_REMAP(5));
#endif
	p = jz_proc_mkdir("ddr");
	if (!p) {
		pr_warning("create_proc_entry for common ddr failed.\n");
		return -ENODEV;
	}
	res = create_proc_entry("ddr_Register", 0444, p);
	if (res) {
		res->read_proc = ddr_read_proc;
		res->write_proc = NULL;
		res->data = NULL;
	}

	return 0;
}

module_init(init_ddr_proc);
