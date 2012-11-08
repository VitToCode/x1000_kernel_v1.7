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

#define DDR_MC		0x130100E4
#define DDR_RESULT_1	0x130100D4
#define DDR_RESULT_2	0x130100D8
#define DDR_RESULT_3	0x130100DC
#define DDR_RESULT_4	0x130100E0

static int ahbm_read_proc(char *page, char **start, off_t off,
		int count, int *eof, void *data)
{
	int len = 0;
	cpm_clear_bit(11,CPM_CLKGR1);
#define PRINT(ARGS...) len += sprintf (page+len, ##ARGS)
#define PRINT_ARRAY(M) \
	do{					\
		PRINT("0x04 = %d\n",ahbm_read(M,0x04));	\
		PRINT("0x08 = %d\n",ahbm_read(M,0x08));	\
		PRINT("0x10 = %d\n",ahbm_read(M,0x10));	\
		PRINT("0x14 = %d\n",ahbm_read(M,0x14));	\
		PRINT("0x18 = %d\n",ahbm_read(M,0x18));	\
		PRINT("0x1c = %d\n",ahbm_read(M,0x1c));	\
		PRINT("0x20 = %d\n",ahbm_read(M,0x20));	\
		PRINT("0x24 = %d\n",ahbm_read(M,0x24));	\
		PRINT("0x28 = %d\n",ahbm_read(M,0x28));	\
	}while(0);

#define PRINT_ARRAY_GPU(M) \
	do{					\
		PRINT_ARRAY(M); \
		PRINT("0x2c = %d\n",ahbm_read(M,0x2c));	\
		PRINT("0x30 = %d\n",ahbm_read(M,0x30));	\
		PRINT("0x34 = %d\n",ahbm_read(M,0x34));	\
		PRINT("0x38 = %d\n",ahbm_read(M,0x38));	\
		PRINT("0x3c = %d\n",ahbm_read(M,0x3c));	\
	}while(0);

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
	PRINT("DDR:\n");
	PRINT("RESULT_1:%d\n",inl(DDR_RESULT_1));
	PRINT("RESULT_2:%d\n",inl(DDR_RESULT_2));
	PRINT("RESULT_3:%d\n",inl(DDR_RESULT_3));
	PRINT("RESULT_4:%d\n",inl(DDR_RESULT_4));

	PRINT("CIM\n");
	PRINT_ARRAY(CIM);
	PRINT("AHB0\n");
	PRINT_ARRAY(AHB0);
	PRINT("AHB2\n");
	PRINT_ARRAY(AHB2);

	PRINT("GPU\n");
	PRINT_ARRAY_GPU(GPU);
	PRINT("LCD\n");
	PRINT_ARRAY_GPU(LCD);

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

