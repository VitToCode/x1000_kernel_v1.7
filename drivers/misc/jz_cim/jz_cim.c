/*
 * linux/drivers/misc/cim.c -- Ingenic CIM driver
 *
 * Copyright (C) 2005-2010, Ingenic Semiconductor Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>

#include <mach/jz_cim.h>

#define CS_STOPED 	0
#define CS_RUNNING	0

static LIST_HEAD(sensor_list);

struct jz_cim_dma_desc {
	unsigned int next;
	unsigned int id;
	unsigned int buf;
	unsigned int cmd;
} __attribute__ ((aligned (16)));

struct jz_cim {
	int irq;
	void __iomem *iomem;
	struct device *dev;
	struct clk *clk;
	struct clk *mclk;

	wait_queue_head_t wait;

	struct list_head list;

	int state;
	int frm_id;

	int count;

	struct jz_cim_dma_desc *preview;
	struct jz_cim_dma_desc *capture;

	struct cim_sensor *desc;
	struct miscdevice misc_dev;

	void (*power_on)(void);
	void (*power_off)(void);

	spinlock_t lock;
};

int camera_sensor_register(struct cim_sensor *desc)
{
	if(!desc) return -EINVAL;
	desc->id = 0xffff;
	list_add_tail(&desc->list,&sensor_list);
	return 0;
}

void cim_scan_sensor(struct jz_cim *cim)
{
	struct cim_sensor *desc;

	cim->power_on();

	list_for_each_entry(desc, &sensor_list, list) {
		desc->power_on(desc->private);
		if(desc->probe(desc->private))
			list_del(&desc->list);
		desc->shutdown(desc->private);
	}

	list_for_each_entry(desc, &sensor_list, list) {
		if(desc->facing == CAMERA_FACING_BACK) {
			desc->id = cim->count;
			cim->count++;
			dev_info(cim->dev,"sensor_name:%s\t\tid:%d facing:%d\n",
					desc->name,desc->id,desc->facing);
		}
	}

	list_for_each_entry(desc, &sensor_list, list) {
		if(desc->facing == CAMERA_FACING_FRONT) {
			desc->id = cim->count;
			cim->count++;
			dev_info(cim->dev,"sensor_name:%s\t\tid:%d facing:%d\n",
					desc->name,desc->id,desc->facing);
		}
	}

	cim->desc = desc;

	cim->power_off();
}

static int cim_select_sensor(struct jz_cim *cim,int id)
{
	struct cim_sensor *desc;

	if(cim->state != CS_STOPED)
		return -EFAULT;
	
	list_for_each_entry(desc, &sensor_list, list) {
		if(desc->id == id) {
			cim->desc = desc;
			break;
		}
	}

	return cim->desc ? 0 : -EFAULT;
}


static irqreturn_t cim_irq_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

static int cim_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int cim_close(struct inode *inode, struct file *file)
{
	return 0;
}

static long cim_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	return 0;
}

static struct file_operations cim_fops = {
	.open 		= cim_open,
	.release 	= cim_close,
	.unlocked_ioctl = cim_ioctl,
};

void cim_dummy_power_on(void)
{
}

void cim_dummy_power_off(void)
{
}

static int cim_probe(struct platform_device *pdev)
{
	int ret;
	struct resource *r;
	struct jz_cim_platform_data *pdata;
	struct jz_cim *cim = kzalloc(sizeof(struct jz_cim), GFP_KERNEL);
	if(!cim) {
		dev_err(&pdev->dev,"no memory!\n");
		ret = -ENOMEM;
		goto no_mem;
	}

	cim->dev = &pdev->dev;

	pdata = pdev->dev.platform_data;

	if(pdata && pdata->power_on)
		cim->power_on = pdata->power_on;
	else
		cim->power_on = cim_dummy_power_on;

	if(pdata && pdata->power_off)
		cim->power_off = pdata->power_off;
	else
		cim->power_off = cim_dummy_power_off;

	cim_scan_sensor(cim);

	if(!cim->desc) {
		dev_err(&pdev->dev,"no sensor!\n");
		ret = -ENOMEM;
		goto no_desc;
	}

	cim->clk = clk_get(&pdev->dev,"cim");
	if(!cim->clk) {
		ret = -ENODEV;
		goto no_desc;
	}

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	cim->iomem = ioremap(r->start,resource_size(r));
	if (!cim->iomem) {
		ret = -ENODEV;
		goto io_failed;
	}

	cim->irq = platform_get_irq(pdev, 0);
	ret = request_irq(cim->irq, cim_irq_handler, IRQF_DISABLED,dev_name(&pdev->dev), cim);
	if(ret) {
		dev_err(&pdev->dev,"request irq failed!\n");
		goto irq_failed;
	}

	init_waitqueue_head(&cim->wait);

	cim->misc_dev.minor = MISC_DYNAMIC_MINOR;
	cim->misc_dev.name = "cim";
	cim->misc_dev.fops = &cim_fops;

	ret = misc_register(&cim->misc_dev);
	if(ret) {
		dev_err(&pdev->dev,"request misc device failed!\n");
		goto misc_failed;
	}

	platform_set_drvdata(pdev,cim);

	dev_info(&pdev->dev,"ingenic camera interface module registered.\n");

	return 0;

misc_failed:	
	free_irq(cim->irq,cim);
irq_failed:
	iounmap(cim->iomem);
io_failed:
	clk_put(cim->clk);
no_desc:
	kfree(cim);
no_mem:
	return ret;
}

static int __devexit cim_remove(struct platform_device *pdev)
{
	struct jz_cim *cim = platform_get_drvdata(pdev);
	kfree(cim);
	misc_deregister(&cim->misc_dev);
	return 0;
}

static struct platform_driver cim_driver = {
	.driver.name	= "jz-cim",
	.driver.owner	= THIS_MODULE,
	.probe		= cim_probe,
	.remove		= cim_remove,
};

static int __init cim_init(void)
{
	return platform_driver_register(&cim_driver);
}

static void __exit cim_exit(void)
{
	platform_driver_unregister(&cim_driver);
}

late_initcall(cim_init);
module_exit(cim_exit);

MODULE_AUTHOR("sonil<ztyan@ingenic.cn>");
MODULE_DESCRIPTION("Ingenic Camera interface module driver");
MODULE_LICENSE("GPL");


