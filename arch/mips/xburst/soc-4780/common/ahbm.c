/*
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
#include <linux/delay.h>
#include <linux/syscore_ops.h>

#include <soc/ahbm.h>
#include <soc/base.h>
#include <soc/cpm.h>

static int msecs = 1;

static int ahbm_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	int len = 0;
	cpm_clear_bit(11,CPM_CLKGR1);

	outl(0x0,DDR_MC);
	outl(0x0,DDR_RESULT_1);
	outl(0x0,DDR_RESULT_2);
	outl(0x0,DDR_RESULT_3);
	outl(0x0,DDR_RESULT_4);
	outl(0x1,DDR_MC);

	ahbm_restart(CIM);
	ahbm_restart(AHB0);
	ahbm_restart(GPU);
	ahbm_restart(LCD);
	ahbm_restart(AHB2);

	msleep(msecs);

	ahbm_stop(CIM);
	ahbm_stop(AHB0);
	ahbm_stop(GPU);
	ahbm_stop(LCD);
	ahbm_stop(AHB2);

	PRINT("msecs = %d\n",msecs);

	outl(0x0,DDR_MC);
	PRINT("DDR(%x):%x\n",DDR_RESULT_1,inl(DDR_RESULT_1));
	PRINT("DDR(%x):%x\n",DDR_RESULT_2,inl(DDR_RESULT_2));
	PRINT("DDR(%x):%x\n",DDR_RESULT_3,inl(DDR_RESULT_3));
	PRINT("DDR(%x):%x\n",DDR_RESULT_4,inl(DDR_RESULT_4));

	PRINT_ARRAY(0,CIM);
	PRINT_ARRAY(1,AHB0);
	PRINT_ARRAY2(2,GPU);
	PRINT_ARRAY2(3,LCD);
	PRINT_ARRAY2(4,XXX);
	PRINT_ARRAY(5,AHB2);

	cpm_set_bit(11,CPM_CLKGR1);
	return len;
}

static int ahbm_write_proc(struct file *file, const char __user *buffer,
		unsigned long count, void *data)
{
	int ret;
	char buf[32];

	if (count > 32)
		count = 32;
	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	ret = sscanf(buf,"%d\n",&msecs);

	return count;
}

static int __init init_ahbm_proc(void)
{
	struct proc_dir_entry *res;

	res = create_proc_entry("ahbm", 0444, NULL);
	if (res) {
		res->read_proc = ahbm_read_proc;
		res->write_proc = ahbm_write_proc;
		res->data = NULL;
	}
	return 0;
}

module_init(init_ahbm_proc);

