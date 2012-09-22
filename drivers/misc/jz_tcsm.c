/*
 * linux/drivers/misc/tcsm.c
 *
 * Virtual device driver with tricky appoach to manage TCSM
 *
 * Copyright (C) 2006  Ingenic Semiconductor Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/major.h>
#include <linux/string.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/spinlock.h>
#include <linux/wakelock.h>

#include <linux/syscalls.h>
#include <linux/platform_device.h>
#include <soc/base.h>
#include <soc/cpm.h>
#include <soc/irq.h>

#include <linux/clk.h>

#include "jz_tcsm.h"

#define REG_VPU_STAT      0x00034
#define VPU_STAT_ENDF    (0x1<<0)
#define VPU_STAT_BPF     (0x1<<1)
#define VPU_STAT_ACFGERR (0x1<<2)
#define VPU_STAT_TIMEOUT (0x1<<3)
#define VPU_STAT_JPGEND  (0x1<<4)
#define VPU_STAT_BSERR   (0x1<<7)
#define VPU_STAT_TLBERR  (0x1F<<10)
#define VPU_STAT_SLDERR  (0x1<<16)

#define REG_VPU_JPGC_STAT 0xE0008
#define JPGC_STAT_ENDF   (0x1<<31)

#define REG_VPU_SDE_STAT  0x90000
#define SDE_STAT_BSEND   (0x1<<1)

#define REG_VPU_DBLK_STAT 0x70070
#define DBLK_STAT_DOEND  (0x1<<0)

#define REG_VPU_AUX_STAT  0xA0010
#define AUX_STAT_MIRQP   (0x1<<0)


struct jz_tcsm {
	struct device	*dev;
	spinlock_t lock;
	int irq;
	void __iomem		*iomem;
	struct wake_lock tcsm_wake_lock;
	struct tcsm_mmap param;
	struct tcsm_sem tcsm_sem;
	struct completion tcsm_comp;
	struct miscdevice tcsm_mdev;
	struct clk *clk_vpu;
	struct clk *clk_cgu_vpu;
	unsigned int irq_status;
};
#define __tcsm_read(tcsm,offset) __raw_readl((tcsm)->iomem + offset)
#define __tcsm_write(tcsm,offset,value) __raw_writel((value), (tcsm)->iomem + offset)

#if 0
static void init_vpu_irq(void)
{
	__asm__ __volatile__ (
			"mfc0  $2, $12,  0   \n\t"
			"ori   $2, $2, 0x800 \n\t"
			"mtc0  $2, $12,  0   \n\t"
			"nop                 \n\t");
}
#endif

static void tcsm_sem_init(struct jz_tcsm *tcsm)
{
	sema_init(&(tcsm->tcsm_sem.sem),1);
	tcsm->tcsm_sem.tcsm_file_mode_pre = R_W;
}

static inline void update_file_state(struct jz_tcsm *tcsm, struct file *filp)
{
	tcsm->tcsm_sem.tcsm_file_mode_pre = tcsm->tcsm_sem.tcsm_file_mode_now;

	switch(filp->f_mode & 0b11) {
		case FMODE_READ:
			tcsm->tcsm_sem.tcsm_file_mode_now = R_ONLY;
			break;
		case (FMODE_READ | FMODE_WRITE):
			tcsm->tcsm_sem.tcsm_file_mode_now = R_W;
			break;
		case FMODE_WRITE:
			tcsm->tcsm_sem.tcsm_file_mode_now = W_ONLY;
			break;
		default:
			tcsm->tcsm_sem.tcsm_file_mode_now = UNOPENED;
			break;
	}
}

static inline enum tcsm_file_cmd tcsm_sem_get_cmd(struct jz_tcsm *tcsm)
{
	enum tcsm_file_cmd file_cmd = 0;

	switch((enum tcsm_file_mode)tcsm->tcsm_sem.tcsm_file_mode_now) {
		case R_ONLY:
		case R_W:
			file_cmd = BLOCK;
			break;

		case W_ONLY:
			switch((enum tcsm_file_mode)tcsm->tcsm_sem.tcsm_file_mode_pre) {
				case R_W:
				case W_ONLY:
					file_cmd = RETURN_NOW;
					break;
				case R_ONLY:
					file_cmd = BLOCK;
					break;
				default:
					file_cmd = RETURN_NOW;
					break;
			}
			break;
		default:
			return RETURN_NOW;
	}
	return file_cmd;
}

static int tcsm_open(struct inode *inode, struct file *filp)
{

	struct miscdevice *dev = filp->private_data;
	struct jz_tcsm *tcsm = container_of(dev, struct jz_tcsm,tcsm_mdev);

	if (tcsm->tcsm_sem.owner_pid == current->pid) {
		pr_info("In %s:%s-->pid[%d] can't open tcsm twice!\n", __FILE__, __func__, current->pid);
		return -EBUSY;
	}

	update_file_state(tcsm, filp);

	if (down_trylock(&tcsm->tcsm_sem.sem)) {
		switch (tcsm_sem_get_cmd(tcsm)) {
			case BLOCK:
				pr_info("In %s:%s-->block\n", __FILE__, __func__);
				if (down_interruptible(&tcsm->tcsm_sem.sem) != 0) {
					return -EBUSY;
				}
				break;
			case RETURN_NOW:
				pr_info("In %s:%s-->return now\n", __FILE__, __func__);
				return -EBUSY;
			default:
				pr_info("In %s:%s-->return now\n", __FILE__, __func__);
				pr_info("Error tcsm file state!\n");
				return -EBUSY;
		}
	}

	tcsm->tcsm_sem.owner_pid = current->pid;
	spin_lock_irq(&tcsm->lock);

	if(cpm_inl(CPM_OPCR) & OPCR_IDLE) {
		spin_unlock_irq(&tcsm->lock);
		up(&tcsm->tcsm_sem.sem);
		return -EBUSY;
	}
	cpm_set_bit(31,CPM_OPCR);

	tcsm->clk_vpu = clk_get(tcsm->dev,"vpu");
	if(!tcsm->clk_vpu) {
		pr_info("In %s:%s-->clk get vpu failed\n", __FILE__, __func__);
		return -ENODEV;
	}
	clk_enable(tcsm->clk_vpu);

	tcsm->clk_cgu_vpu = clk_get(tcsm->dev, "cgu_vpu");
	if (IS_ERR(tcsm->clk_cgu_vpu)) {
		pr_info("In %s:%s-->clk get cgu_vpu failed\n", __FILE__, __func__);
		return -ENODEV;
	}
	clk_set_rate(tcsm->clk_cgu_vpu,300000000);
	clk_enable(tcsm->clk_cgu_vpu);

	cpm_clear_bit(30,CPM_LCR);	//vpu power on

	__asm__ __volatile__ (
			"mfc0  $2, $16,  7   \n\t"
			"ori   $2, $2, 0x340 \n\t"
			"andi  $2, $2, 0x3ff \n\t"
			"mtc0  $2, $16,  7  \n\t"
			"nop                  \n\t");
	enable_irq(tcsm->irq);

	spin_unlock_irq(&tcsm->lock);
	//memcpy speed
	wake_lock(&tcsm->tcsm_wake_lock);
	return 0;
}

static int tcsm_release(struct inode *inode, struct file *filp)
{
	unsigned int dat;
	struct miscdevice *dev = filp->private_data;
	struct jz_tcsm *tcsm = container_of(dev, struct jz_tcsm,tcsm_mdev);

	pr_info("close tcsm\n");
	/*power down ahb1*/
	cpm_set_bit(30,CPM_LCR);

	dat = cpm_inl(CPM_CLKGR1);

	disable_irq_nosync(tcsm->irq);
	dat |= CLKGR1_VPU;
	pr_info("dat = 0x%08x\n",dat);
	cpm_outl(dat,CPM_CLKGR1);

	spin_lock_irq(&tcsm->lock);
	__asm__ __volatile__ (
			"mfc0  $2, $16,  7   \n\t"
			"andi  $2, $2, 0xbf \n\t"
			"mtc0  $2, $16,  7  \n\t"
			"nop                  \n\t");

	cpm_clear_bit(31,CPM_OPCR);
	spin_unlock_irq(&tcsm->lock);
	wake_unlock(&tcsm->tcsm_wake_lock);
	up(&tcsm->tcsm_sem.sem);
	tcsm->tcsm_sem.owner_pid = 0;
	return 0;
}

static ssize_t tcsm_read(struct file *filp, char *buf, size_t size, loff_t *l)
{
	pr_info("tcsm: read is not implemented\n");
	return -1;
}

static ssize_t tcsm_write(struct file *filp, const char *buf, size_t size, loff_t *l)
{
	pr_info("tcsm: write is not implemented\n");
	return -1;
}

static long tcsm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = filp->private_data;
	struct jz_tcsm *tcsm = container_of(dev, struct jz_tcsm,tcsm_mdev);

	switch (cmd) {
		case TCSM_TOCTL_WAIT_COMPLETE:
			return wait_for_completion_interruptible_timeout(&tcsm->tcsm_comp,msecs_to_jiffies(arg));

		case TCSM_TOCTL_SET_MMAP:
			if (copy_from_user(&tcsm->param, (void *)arg, sizeof(tcsm->param)))
				return -EFAULT;
			return 0;
	}
	pr_info(KERN_ERR "%s:cmd(0x%x) error !!!",__func__,cmd);
	return -1;
}

static int tcsm_mmap(struct file *file, struct vm_area_struct *vma)
{
	unsigned long off, start;
	u32 len;

	struct miscdevice *dev = file->private_data;
	struct jz_tcsm *tcsm = container_of(dev, struct jz_tcsm,tcsm_mdev);

	off = vma->vm_pgoff << PAGE_SHIFT;
	start = tcsm->param.start;
	len = PAGE_ALIGN(start & ~PAGE_MASK) + tcsm->param.len;
	start &= PAGE_MASK;
	if ((vma->vm_end - vma->vm_start + off) > len)
		return -EINVAL;
	off += start;
	vma->vm_pgoff = off >> PAGE_SHIFT;

	/* This is an IO map - tell maydump to skip this VMA */
	vma->vm_flags |= VM_IO;
	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);	// Uncacheable
#if  defined(CONFIG_MIPS32)
	pgprot_val(vma->vm_page_prot) &= ~_CACHE_MASK;
	pgprot_val(vma->vm_page_prot) |= _CACHE_UNCACHED;		/* Uncacheable */
#endif
	if (io_remap_pfn_range(vma,vma->vm_start, off >> PAGE_SHIFT,vma->vm_end - vma->vm_start,vma->vm_page_prot))
		return -EAGAIN;
	return 0;
}

/*
 * Module init and exit
 */

static irqreturn_t vpu_interrupt(int irq, void *dev)
{
	struct jz_tcsm *tcsm = dev;
	int vpu_stat;
	vpu_stat = __tcsm_read(tcsm,REG_VPU_STAT);
	tcsm->irq_status = vpu_stat;
#define CLEAR_TCSM_BIT(tcsm,offset,bm)				\
	do {							\
		unsigned int stat;				\
		stat = __tcsm_read(tcsm,REG_VPU_SDE_STAT);	\
		__tcsm_write(tcsm,REG_VPU_SDE_STAT,stat & ~(bm));	\
	}while(0)

	if(vpu_stat & VPU_STAT_TLBERR) {
		pr_info("[EYER] INTC: VPU TLB error!\n");
	} else if(vpu_stat & VPU_STAT_BSERR) {
		pr_info("[EYER] INTC: VPU BS error!\n");
	} else if(vpu_stat & VPU_STAT_ACFGERR) {
		pr_info("[EYER] INTC: VPU ACFG error!\n");
	}
	else if(vpu_stat & VPU_STAT_ENDF) {
		if(vpu_stat & VPU_STAT_JPGEND) {
			pr_debug("[EYER] INTC: VPU(JPG) successfully done!\n");
			CLEAR_TCSM_BIT(tcsm,REG_VPU_JPGC_STAT,JPGC_STAT_ENDF);
		} else {
			pr_debug("[EYER] INTC: VPU(SCH) successfully done!\n");
			CLEAR_TCSM_BIT(tcsm,REG_VPU_SDE_STAT,SDE_STAT_BSEND);
			CLEAR_TCSM_BIT(tcsm,REG_VPU_DBLK_STAT,DBLK_STAT_DOEND);
		}
	} else {
		if(__tcsm_read(tcsm,REG_VPU_AUX_STAT) & AUX_STAT_MIRQP) {
			pr_debug("[EYER] INTC: VPU(AUX) successfully done!\n");
			CLEAR_TCSM_BIT(tcsm,REG_VPU_AUX_STAT,AUX_STAT_MIRQP);
		} else {
			pr_info("[EYER] INTC: illegal interrupt happened!\n");
		}
	}

	complete(&tcsm->tcsm_comp);
	return IRQ_HANDLED;
}

static struct file_operations tcsm_misc_fops = {
		open:		tcsm_open,
		release:	tcsm_release,
		read:		tcsm_read,
		write:		tcsm_write,
		unlocked_ioctl:	tcsm_ioctl,
		mmap:           tcsm_mmap,
};

static int tcsm_probe(struct platform_device *dev)
{
	int ret;
	struct resource			*regs;
	struct jz_tcsm *tcsm = kzalloc(sizeof(struct jz_tcsm), GFP_KERNEL);
	if (!tcsm) {
		ret = -ENOMEM;
		goto no_mem;
	}
	regs = platform_get_resource(dev, IORESOURCE_MEM, 0);
	if (!regs) {
		dev_err(&dev->dev, "No iomem resource\n");
		ret = -ENXIO;
	}
	tcsm->dev = &dev->dev;
	tcsm->iomem = ioremap(regs->start, resource_size(regs));
	if (!tcsm->iomem) {
		ret = -ENXIO;
		pr_err("%s %d ioremap failed!",__FILE__,__LINE__);
		goto fail_resource;
	}
	tcsm->irq = platform_get_irq(dev, 0);
	if(tcsm->irq < 0) {
		ret = -ENXIO;
		pr_err("%s %d platform_get_irq failed!",__FILE__,__LINE__);
		goto fail_resource;
	}
	tcsm->tcsm_mdev.minor = TCSM_MINOR;
	tcsm->tcsm_mdev.name =  "jz-tcsm";
	tcsm->tcsm_mdev.fops = &tcsm_misc_fops;
	spin_lock_init(&tcsm->lock);
	ret = misc_register(&tcsm->tcsm_mdev);
	if (ret < 0) {
		pr_err("%s %d misc_register failed!\n",__FILE__,__LINE__);
		goto fail_register;
	}
	platform_set_drvdata(dev, tcsm);

	wake_lock_init(&tcsm->tcsm_wake_lock, WAKE_LOCK_SUSPEND, "tcsm");

	tcsm_sem_init(tcsm);

	init_completion(&tcsm->tcsm_comp);
	ret = request_irq(tcsm->irq,vpu_interrupt,IRQF_DISABLED,
			"vpu",tcsm);
	if (ret < 0) {
		ret = -ENODEV;
		pr_err("%s %d request_irq failed! %d\n",__FILE__,__LINE__,tcsm->irq);
		goto fail_irq;
	}
	disable_irq_nosync(tcsm->irq);

	pr_info("Virtual Driver of JZ TCSM registered\n");
	return 0;


fail_irq:
	misc_deregister(&tcsm->tcsm_mdev);
fail_register:
	iounmap(tcsm->iomem);
fail_resource:
	kfree(tcsm);
no_mem:
	return ret;
}

static int tcsm_remove(struct platform_device *dev)
{
	struct jz_tcsm *tcsm = platform_get_drvdata(dev);
	misc_deregister(&tcsm->tcsm_mdev);
	wake_lock_destroy(&tcsm->tcsm_wake_lock);
	free_irq(tcsm->irq,NULL);
	kfree(tcsm);
	return 0;
}

static struct platform_driver jz_tcsm_driver = {
	.probe		= tcsm_probe,
	.remove		= tcsm_remove,
	.driver		= {
		.name	= "jz-tcsm",
	},
};

static int __init tcsm_init(void)
{
	return platform_driver_register(&jz_tcsm_driver);
}

static void __exit tcsm_exit(void)
{
	platform_driver_unregister(&jz_tcsm_driver);
}

module_init(tcsm_init);
module_exit(tcsm_exit);

MODULE_AUTHOR("bcjia <bcjia@ingenic.cn>");
MODULE_DESCRIPTION("Virtual Driver of TCSM");
MODULE_LICENSE("GPL");
