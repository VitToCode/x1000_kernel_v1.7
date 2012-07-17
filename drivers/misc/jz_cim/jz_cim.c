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
#include <linux/dma-mapping.h>

#include <mach/jz_cim.h>

#define PDESC_NR	4
#define CDESC_NR	3

#define	CIM_CFG			(0x00)
#define	CIM_CTRL		(0x04)
#define	CIM_STATE		(0x08)
#define	CIM_IID			(0x0c)
#define	CIM_DA			(0x20)
#define	CIM_FA			(0x24)
#define	CIM_FID			(0x28)
#define	CIM_CMD			(0x2c)
#define	CIM_SIZE		(0x30)
#define	CIM_OFFSET		(0x34)
#define CIM_YFA			(0x38)
#define CIM_YCMD		(0x3c)
#define CIM_CBFA		(0x40)
#define CIM_CBCMD		(0x44)
#define CIM_CRFA		(0x48)
#define CIM_CRCMD		(0x4c)
#define CIM_CTRL2		(0x50)

/* CIM DMA Command Register (CIM_CMD) */
#define	CIM_CMD_SOFINT		(1 << 31) /* enable DMA start irq */
#define	CIM_CMD_EOFINT		(1 << 30) /* enable DMA end irq */
#define	CIM_CMD_EEOFINT		(1 << 29) /* enable DMA EEOF irq */
#define	CIM_CMD_STOP		(1 << 28) /* enable DMA stop irq */
#define CIM_CMD_OFRCV           (1 << 27) 

static LIST_HEAD(sensor_list);

enum cim_state {
	CS_IDLE,
	CS_PREVIEW,
	CS_CAPTURE,
};

#if 0
struct jz_cim_dma_desc {
	unsigned int next;
	unsigned int id;
	unsigned int yf_buf;
	unsigned int yf_cmd;
	unsigned int ycb_buf;
	unsigned int ycb_cmd;
	unsigned int ycr_buf;
	unsigned int ycr_cmd;
} __attribute__ ((aligned (16)));
#else
struct jz_cim_dma_desc {
	dma_addr_t next;
	unsigned int id;
	unsigned int buf;
	unsigned int cmd;
} __attribute__ ((aligned (16)));
#endif

struct jz_cim {
	int irq;
	void __iomem *iomem;
	struct device *dev;
	struct clk *clk;
	struct clk *mclk;

	wait_queue_head_t wait;

	struct list_head list;

	volatile int frm_id;
	enum cim_state state;

	int sensor_count;

	void *pdesc_vaddr;
	void *cdesc_vaddr;
	struct jz_cim_dma_desc *preview;
	struct jz_cim_dma_desc *capture;

	struct cim_sensor *desc;
	struct miscdevice misc_dev;

	void (*power_on)(void);
	void (*power_off)(void);

	spinlock_t lock;
	struct frm_size psize;
	struct frm_size csize;
};

int camera_sensor_register(struct cim_sensor *desc)
{
	if(!desc) 
		return -EINVAL;
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
			desc->id = cim->sensor_count;
			cim->sensor_count++;
			dev_info(cim->dev,"sensor_name:%s\t\tid:%d facing:%d\n",
					desc->name,desc->id,desc->facing);
		}
	}

	list_for_each_entry(desc, &sensor_list, list) {
		if(desc->facing == CAMERA_FACING_FRONT) {
			desc->id = cim->sensor_count;
			cim->sensor_count++;
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
	if(cim->state != CS_IDLE)
		return -EBUSY;
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

static long cim_shutdown(struct jz_cim *cim)
{
	cim->state = CS_IDLE;
	return 0;
}

static long cim_start_preview(struct jz_cim *cim)
{
	cim->state = CS_PREVIEW;
	return 0;
}

static long cim_start_capture(struct jz_cim *cim)
{
	cim->state = CS_CAPTURE;
	return 0;
}

static long cim_set_param(struct jz_cim *cim, int arg)
{
	// used app should use this ioctl like this :
	// ioctl(fd, CIMIO_SET_PARAM, CPCMD_SET_BALANCE | WHITE_BALANCE_AUTO);
	int cmd,param_arg;
	cmd = arg & 0xffff0000;
	param_arg = arg & 0xffff;
	switch(cmd) {
		case CPCMD_SET_BALANCE:
			return cim->desc->set_balance(cim->desc,param_arg);
		case CPCMD_SET_EFFECT:
			return cim->desc->set_effect(cim->desc,param_arg);
		case CPCMD_SET_ANTIBANDING:
			return cim->desc->set_antibanding(cim->desc,param_arg);
		case CPCMD_SET_FLASH_MODE:
			return cim->desc->set_flash_mode(cim->desc,param_arg);
		case CPCMD_SET_SCENE_MODE:
			return cim->desc->set_scene_mode(cim->desc,param_arg);
		case CPCMD_SET_FOCUS_MODE:
			return cim->desc->set_focus_mode(cim->desc,param_arg);
		case CPCMD_SET_FPS:
			return cim->desc->set_fps(cim->desc,param_arg);
		case CPCMD_SET_NIGHTSHOT_MODE:
			return cim->desc->set_nightshot(cim->desc,param_arg);
		case CPCMD_SET_LUMA_ADAPTATION:
			return cim->desc->set_luma_adaption(cim->desc,param_arg);
		case CPCMD_SET_BRIGHTNESS:
			return cim->desc->set_brightness(cim->desc,param_arg);
		case CPCMD_SET_CONTRAST:
			return cim->desc->set_contrast(cim->desc,param_arg);
	}
	return 0;
}

static void cim_free_mem(struct jz_cim *cim)
{
	if(cim->pdesc_vaddr)
		dma_free_coherent(cim->dev, sizeof(*cim->preview) * PDESC_NR,
				cim->pdesc_vaddr, (dma_addr_t)cim->preview);
	if(cim->cdesc_vaddr)
		dma_free_coherent(cim->dev, sizeof(*cim->capture) * CDESC_NR,
				cim->cdesc_vaddr, (dma_addr_t)cim->capture);
}

static int cim_alloc_mem(struct jz_cim *cim)
{
	cim->pdesc_vaddr = dma_alloc_coherent(cim->dev,
			sizeof(*cim->preview) * PDESC_NR,(dma_addr_t *)&cim->preview, GFP_KERNEL);

	if (!cim->pdesc_vaddr)
		return -ENOMEM;

	cim->cdesc_vaddr = dma_alloc_coherent(cim->dev,
			sizeof(*cim->capture) * CDESC_NR,(dma_addr_t *)&cim->capture, GFP_KERNEL);

	if (!cim->cdesc_vaddr)
		return -ENOMEM;

	return 0;
}

static int cim_prepare_pdma(struct jz_cim *cim, dma_addr_t addr)
{
	int i;
	unsigned int preview_frmsize = cim->psize.w *  cim->psize.h * 3;

	if(cim->state != CS_IDLE)
		return -EBUSY;
	for(i=1;i<PDESC_NR;i++) {//desc0 used for swap buf,and others used for dma buf
		cim->preview[i].next 	= (dma_addr_t)&cim->preview[i+1];
		cim->preview[i].id 	= i;
		cim->preview[i].buf	= addr;
		cim->preview[i].cmd 	= preview_frmsize | CIM_CMD_EOFINT | CIM_CMD_OFRCV;
	}

	cim->preview[CDESC_NR-1].next 	= (dma_addr_t)&cim->preview[0];

	return 0;
}

static int cim_prepare_cdma(struct jz_cim *cim, dma_addr_t addr)
{
	int i;
	unsigned int capture_frmsize = cim->psize.w *  cim->psize.h * 3;

	if(cim->state != CS_IDLE)
		return -EBUSY;
	for(i=0;i<CDESC_NR;i++) {
		cim->capture[i].next 	= (dma_addr_t)&cim->capture[i+1];
		cim->capture[i].id 	= i;
		cim->capture[i].buf	= addr;
		cim->capture[i].cmd 	= capture_frmsize | CIM_CMD_OFRCV;
	}
	cim->capture[CDESC_NR-1].cmd 	|= CIM_CMD_STOP;
	return 0;
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
	struct miscdevice *dev = file->private_data;
	struct jz_cim *cim = container_of(dev, struct jz_cim, misc_dev);

	switch (cmd) {
		case CIMIO_SHUTDOWN:
			return cim_shutdown(cim);
		case CIMIO_START_PREVIEW:
			return cim_start_preview(cim);
		case CIMIO_START_CAPTURE:
			return cim_start_capture(cim);
		case CIMIO_GET_FRAME:	
			break;
		case CIMIO_GET_FIXED:
			break;
		case CIMIO_GET_VAR:
			break;
		case CIMIO_GET_SUPPORT_SIZE:
			break;
		case CIMIO_SET_PARAM:
			return cim_set_param(cim,arg);
		case CIMIO_SET_PREVIEW_MEM:
			return cim_prepare_pdma(cim,arg);
		case CIMIO_SET_CAPTURE_MEM:
			return cim_prepare_cdma(cim,arg);
		case CIMIO_SELECT_SENSOR:
			return cim_select_sensor(cim,arg);
		case CIMIO_SET_PREVIEW_SIZE:
			if (copy_from_user(&cim->psize, (void __user *)arg, sizeof(struct frm_size)))
				return -EFAULT;
			break;
		case CIMIO_SET_CAPTURE_SIZE:
			if (copy_from_user(&cim->csize, (void __user *)arg, sizeof(struct frm_size)))
				return -EFAULT;
		case CIMIO_DO_FOCUS:
			break;
		case CIMIO_AF_INIT:
			break;
	}

	return 0;
}

static struct file_operations cim_fops = {
	.open 		= cim_open,
	.release 	= cim_close,
	.unlocked_ioctl = cim_ioctl,
};

void cim_dummy_power(void) {}

static int cim_probe(struct platform_device *pdev)
{
	int ret = 0;
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

	cim->power_on = cim_dummy_power;
	cim->power_off = cim_dummy_power;

	if(pdata && pdata->power_on)
		cim->power_on = pdata->power_on;

	if(pdata && pdata->power_off)
		cim->power_off = pdata->power_off;

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

	cim->mclk = clk_get(&pdev->dev,"cim_mclk");
	if(!cim->mclk) {
		ret = -ENODEV;
		goto io_failed;
	}

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	cim->iomem = ioremap(r->start,resource_size(r));
	if (!cim->iomem) {
		ret = -ENODEV;
		goto io_failed1;
	}

	if(cim_alloc_mem(cim)) {
		dev_err(&pdev->dev,"request mem failed!\n");
		goto mem_failed;
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
	cim_free_mem(cim);
mem_failed:
	iounmap(cim->iomem);
io_failed1:
	clk_put(cim->mclk);
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


