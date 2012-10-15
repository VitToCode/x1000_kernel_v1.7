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
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/wait.h>
#include <linux/gpio.h>

#include <linux/regulator/consumer.h>

#include <mach/jz_cim.h>
#include "cim_reg.h"

#define PDESC_NR	4
#define CDESC_NR	3
#define DEG() printk("fuchao ---------------- %s %d\n",__FUNCTION__,__LINE__)
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
	/* only used when SEP = 1 */
	#if 1
	unsigned int cb_frame;
	unsigned int cb_len;
	unsigned int cr_frame;
	unsigned int cr_len;
	#endif
} __attribute__ ((aligned (32)));
#endif
struct cim_buf_info
{
	unsigned int paddr;
	unsigned int vaddr;
	int is_vbuf;
};
struct jz_cim {
	int irq;
	void __iomem *iomem;
	struct device *dev;
	struct clk *clk;
	struct clk *mclk;
	struct regulator * power;

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
	int tlb_flag;
	unsigned long tlb_base;
};

static   unsigned long inline reg_read(struct jz_cim *cim,int offset)
{
	return readl(cim->iomem+ offset); 
}
static   void inline reg_write(struct jz_cim *cim,int offset, unsigned long val)
{
	writel(val, cim->iomem + offset); 
}
static void inline bit_set(struct jz_cim *cim ,int offset,int bit)
{
	unsigned long val;
	val = reg_read(cim,offset);
	val |= bit;
	reg_write(cim,offset,val);
}
static void inline bit_clr(struct jz_cim *cim ,int offset,int bit)
{
	unsigned long val;
	val = reg_read(cim,offset);
	val &= ~bit;
	reg_write(cim,offset,val);
}
void  cim_enable(struct jz_cim *cim){bit_set(cim,CIM_CTRL,CIM_CTRL_ENA);}
void  cim_disable(struct jz_cim *cim){bit_clr(cim,CIM_CTRL,CIM_CTRL_ENA);}
void  cim_reset(struct jz_cim *cim){bit_set(cim,CIM_CTRL,CIM_CTRL_CIM_RST);}
void  cim_enable_dma(struct jz_cim *cim){bit_set(cim,CIM_CTRL,CIM_CTRL_DMA_EN);}
void  cim_disable_dma(struct jz_cim *cim){bit_clr(cim,CIM_CTRL,CIM_CTRL_DMA_EN);}
void  cim_clear_rfifo(struct jz_cim *cim){bit_set(cim,CIM_CTRL,CIM_CTRL_RXF_RST);bit_clr(cim,CIM_CTRL,CIM_CTRL_RXF_RST);}

void   cim_clear_state(struct jz_cim *cim){reg_write(cim,CIM_STATE,0);}
void   cim_enable_fsc_intr(struct jz_cim *cim){bit_clr(cim,CIM_IMR,CIM_IMR_FSEM);}
void   cim_enable_eof_intr(struct jz_cim *cim){bit_clr(cim,CIM_IMR,CIM_IMR_EOFM);}
void   cim_enable_rxfifo_overflow_intr(struct jz_cim *cim){bit_clr(cim,CIM_IMR,CIM_IMR_RFIFO_OFM);}
void   cim_enable_priority_control(struct jz_cim *cim){bit_set(cim,CIM_CTRL2,CIM_CTRL2_APM);}
void   cim_enable_emergency(struct jz_cim *cim){bit_set(cim,CIM_CTRL2,CIM_CTRL2_EME);}
void   cim_enable_tlb_error_intr(struct jz_cim *cim){bit_clr(cim,CIM_IMR,CIM_IMR_TLBEM);}


void   cim_set_da(struct jz_cim *cim,void * addr){reg_write(cim,CIM_DA,(unsigned long)addr);}
unsigned long  cim_get_iid(struct jz_cim *cim){return reg_read(cim,CIM_IID);}
unsigned long  cim_get_fid(struct jz_cim *cim){return reg_read(cim,CIM_FID);}
unsigned long  cim_read_state(struct jz_cim *cim){return reg_read(cim,CIM_STATE);}

void cim_reset_tlb(struct jz_cim *cim){bit_set(cim,CIM_TC,CIM_TC_RST);bit_clr(cim,CIM_TC,CIM_TC_RST);}
void cim_enable_tlb(struct jz_cim *cim){bit_set(cim,CIM_TC,CIM_TC_ENA);}
int  cim_set_tlbbase(struct jz_cim *cim)
{
	unsigned long regval = 0;
	dev_info(cim->dev,"cim set tlb base is %lx\n",cim->tlb_base);
	if(cim->tlb_base & 0x3){//bit[0:1] must be 0.double word aligned
		dev_info(cim->dev,"cim tlb base is not valid address\n");
		return -1;
	}
	regval = reg_read(cim,CIM_TC);
	regval |= cim->tlb_base;
	reg_write(cim,CIM_TC,regval);
	return 0;
}

void cim_dump_reg(struct jz_cim *cim)
{
	dev_info(cim->dev,"REG_CIM_CFG \t= \t0x%08lx\n", reg_read(cim,CIM_CFG));
	dev_info(cim->dev,"REG_CIM_CTRL \t= \t0x%08lx\n", reg_read(cim,CIM_CTRL));
	dev_info(cim->dev,"REG_CIM_CTRL2 \t= \t0x%08lx\n", reg_read(cim,CIM_CTRL2));
	dev_info(cim->dev,"REG_CIM_STATE \t= \t0x%08lx\n", reg_read(cim,CIM_STATE));
	dev_info(cim->dev,"REG_CIM_IMR \t= \t0x%08lx\n", reg_read(cim,CIM_IMR));
	dev_info(cim->dev,"REG_CIM_IID \t= \t0x%08lx\n", reg_read(cim,CIM_IID));
	dev_info(cim->dev,"REG_CIM_DA \t= \t0x%08lx\n", reg_read(cim,CIM_DA));
	dev_info(cim->dev,"REG_CIM_FA \t= \t0x%08lx\n", reg_read(cim,CIM_FA));
	dev_info(cim->dev,"REG_CIM_FID \t= \t0x%08lx\n", reg_read(cim,CIM_FID));
	dev_info(cim->dev,"REG_CIM_CMD \t= \t0x%08lx\n", reg_read(cim,CIM_CMD));
	dev_info(cim->dev,"REG_CIM_WSIZE \t= \t0x%08lx\n", reg_read(cim,CIM_SIZE));
	dev_info(cim->dev,"REG_CIM_WOFFSET \t= \t0x%08lx\n", reg_read(cim,CIM_OFFSET));
	dev_info(cim->dev,"REG_CIM_FS \t= \t0x%08lx\n", reg_read(cim,CIM_FS));
	dev_info(cim->dev,"REG_CIM_YFA \t= \t0x%08lx\n", reg_read(cim,CIM_YFA));
	dev_info(cim->dev,"REG_CIM_YCMD \t= \t0x%08lx\n", reg_read(cim,CIM_YCMD));
	dev_info(cim->dev,"REG_CIM_CBFA \t= \t0x%08lx\n", reg_read(cim,CIM_CBFA));
	dev_info(cim->dev,"REG_CIM_CBCMD \t= \t0x%08lx\n", reg_read(cim,CIM_CBCMD));
	dev_info(cim->dev,"REG_CIM_CRFA \t= \t0x%08lx\n", reg_read(cim,CIM_CRFA));
	dev_info(cim->dev,"REG_CIM_CRCMD \t= \t0x%08lx\n", reg_read(cim,CIM_CRCMD));
	dev_info(cim->dev,"REG_CIM_TC \t= \t0x%08lx\n", reg_read(cim,CIM_TC));
	dev_info(cim->dev,"REG_CIM_TINX \t= \t0x%08lx\n", reg_read(cim,CIM_TINX));
	dev_info(cim->dev,"REG_CIM_TCNT \t= \t0x%08lx\n", reg_read(cim,CIM_TCNT));

}

void cim_power_on(struct jz_cim *cim)
{
	if(cim->clk)
		clk_enable(cim->clk);
	if(cim->mclk)
		clk_enable(cim->mclk);
	#if 1
	if(cim->power != NULL){
		printk(" -----cim power enable\n");
		regulator_enable(cim->power);
	}
	#else
	gpio_direction_output(GPIO_PB(27),1);
	gpio_set_value(GPIO_PB(27),1);
	#endif
	mdelay(10);
	//dev_info(cim->dev," ---probe get clk rete is %d\n",clk_get_rate(cim->mclk));
	clk_set_rate(cim->mclk, 24000000);

}
void cim_power_off(struct jz_cim *cim)
{
	#if 0
	if(cim->clk)
		clk_disable(cim->clk);
	if(cim->mclk)
		clk_disable(cim->mclk);
	
	#endif
	if(cim->power < 0)
		regulator_disable(cim->power);
}

void cim_set_default(struct jz_cim *cim)
{
	unsigned long cfg = 0;
	unsigned long ctrl = 0;
	unsigned long ctrl2 = 0;
	unsigned long fs = 0;
	int w = 0,h = 0;
	if(cim->state == CS_PREVIEW){
		w = cim->psize.w;
		h = cim->psize.h;
	}
	else if (cim->state == CS_CAPTURE){
		w = cim->csize.w;
		h = cim->csize.h;
	}
	cfg = cim->desc->cim_cfg |CIM_CFG_DMA_BURST_INCR32 |CIM_CFG_DF_YUV422;
	ctrl = CIM_CTRL_DMA_SYNC |CIM_CTRL_FRC_1;
	ctrl2 = CIM_CTRL2_APM | CIM_CTRL2_EME | CIM_CTRL2_OPE |
		(1 << CIM_CTRL2_OPG_BIT) | CIM_CTRL2_FSC | CIM_CTRL2_ARIF;
	fs = (w -1)<< CIM_FS_FHS_BIT | (h -1)<< CIM_FS_FVS_BIT | 1<< CIM_FS_BPP_BIT;

	reg_write(cim,CIM_CFG,cfg);
	reg_write(cim,CIM_CTRL,ctrl);
	reg_write(cim,CIM_CTRL2,ctrl2);
	reg_write(cim,CIM_FS,fs);
	//cim_enable_fsc_intr(cim);
	cim_enable_eof_intr(cim);
	cim_enable_rxfifo_overflow_intr(cim);
}



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
	static struct cim_sensor *tmpdesc = NULL;
	struct list_head *tmp;
	cim->sensor_count  = 0;
	cim_power_on(cim);
	
	list_for_each_entry(desc, &sensor_list, list) {
		//desc->power_on(desc);
		if(desc->probe(desc)){
			tmp = desc->list.prev;
			list_del(&desc->list);
			desc = list_entry(tmp, struct cim_sensor, list);
		}
		else
			tmpdesc = desc;
		//desc->shutdown(desc);
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
	cim_power_off(cim);
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

static long cim_set_capture_size(struct jz_cim *cim)
{
	int i =0;
	struct frm_size * p = cim->desc->capture_size;
	for(i=0;i<cim->desc->cap_resolution_nr;i++){	 
		if(cim->csize.w == p->w && cim->csize.h == p->h){
			dev_info(cim->dev,"Found the capture size %d * %d in sensor table\n",cim->csize.w,cim->csize.h);
			break;
		}
		p++;
	}
	
	if(i>= cim->desc->cap_resolution_nr){
		dev_info(cim->dev,"Cannot found the capture size %d * %d in sensor table\n",cim->csize.w,cim->csize.h);
		return -1;
	}
	if(cim->state == CS_CAPTURE)
		cim->desc->set_resolution(cim->desc,cim->csize.w,cim->csize.h);
	return 0;
}

static long cim_set_preview_size(struct jz_cim *cim)
{
	int i =0;
	struct frm_size * p = cim->desc->preview_size;
	for(i=0;i<cim->desc->prev_resolution_nr;i++){	 
		if(cim->psize.w == p->w && cim->psize.h == p->h){
			dev_info(cim->dev,"Found the preview size %d * %d in sensor table\n",cim->psize.w,cim->psize.h);
			break;
		}
		p++;
	}
	
	if(i>= cim->desc->prev_resolution_nr){
		dev_info(cim->dev,"Cannot found the preview size %d * %d in sensor table\n",cim->psize.w,cim->psize.h);
		return -1;
	}
	if(cim->state == CS_PREVIEW)
		cim->desc->set_resolution(cim->desc,cim->psize.w,cim->psize.h);
	return 0;
}
static irqreturn_t cim_irq_handler(int irq, void *data)
{
	struct jz_cim * cim = (struct jz_cim * )data;
	unsigned long state_reg = 0;
	unsigned long flags;
	static int wait_count = 0;
	state_reg = cim_read_state(cim);
	
	dev_dbg(cim->dev," -------------   cim irq  state reg %lx %ld\n",state_reg,reg_read(cim,CIM_IID));
	if (state_reg & CIM_STATE_RXF_OF){
		dev_err(cim->dev," ------- Rx FIFO OverFlow interrupt!\n");

			bit_clr(cim,CIM_STATE,CIM_STATE_RXF_OF);
			cim_disable(cim);
			cim_clear_rfifo(cim);
			cim_clear_state(cim);	// clear state register
			cim_enable(cim);
			return IRQ_HANDLED;
	}
	
	if(state_reg & CIM_STATE_TLB_ERR){
		
		dev_err(cim->dev," ------- TLB Error interrupt!\n");
		
		cim->state = CS_IDLE;
		cim_disable(cim);
		cim_disable_dma(cim);
		cim_reset_tlb(cim);	
		if(waitqueue_active(&cim->wait))
			wake_up_interruptible(&cim->wait);
		return IRQ_HANDLED;
	}

	if(state_reg & CIM_STATE_DMA_EOF){
		
		if(cim->state == CS_PREVIEW){
			dev_dbg(cim->dev,"irq eof preview fid %ld iid %ld\n",reg_read(cim,CIM_FID),reg_read(cim,CIM_IID));
			spin_lock_irqsave(&cim->lock,flags);
			cim->frm_id =  cim_get_iid(cim) - 1;
			if(cim->frm_id == -1)
				cim->frm_id  = PDESC_NR -1;
			spin_unlock_irqrestore(&cim->lock,flags);
			
			if(waitqueue_active(&cim->wait))
				wake_up_interruptible(&cim->wait);
			bit_clr(cim,CIM_STATE,CIM_STATE_DMA_EOF);
			return IRQ_HANDLED;
		}
		
		else if(cim->state == CS_CAPTURE){
			wait_count ++;
			//dev_info(cim->dev,"capture frame wait : %d\n",wait_count);
			dev_info(cim->dev,"capture frame wait : %d\n",wait_count);

			if( wait_count == 6)
			{
				wait_count = 0;
				cim_disable(cim);
				cim_clear_rfifo(cim);
				cim_clear_state(cim);
				wake_up_interruptible(&cim->wait);
				return IRQ_HANDLED;
			}

			cim_disable(cim);
			cim_clear_rfifo(cim);
			bit_clr(cim,CIM_STATE,CIM_STATE_DMA_EOF);
			cim_enable(cim);
			return IRQ_HANDLED;		
		}
	}
	
	return IRQ_HANDLED;
}

static long cim_shutdown(struct jz_cim *cim)
{
	if(cim->state == CS_IDLE)
		return 0;
	cim->state = CS_IDLE;
	dev_info(cim->dev," -----cim shut down\n");
	//cim->desc->shutdown(cim->desc);
	cim_disable(cim);
	cim_disable_dma(cim);
	
	cim_reset(cim);
	cim_clear_state(cim);	// clear state register
	cim_clear_rfifo(cim);	// resetting rxfifo
	//cim_dump_reg(cim);
	
	wake_up_interruptible(&cim->wait);
	cim_power_off(cim);
	return 0;
}

static long cim_start_preview(struct jz_cim *cim)
{
	cim->state = CS_PREVIEW;
	cim->frm_id = -1;

	cim_disable(cim);
	cim_set_default(cim);
	cim_power_on(cim);
	cim->desc->power_on(cim->desc);
	cim->desc->reset(cim->desc);
	cim->desc->init(cim->desc);
	cim_set_preview_size(cim);
	cim->desc->set_antibanding(cim->desc,cim->desc->para.antibanding);
	cim->desc->set_balance(cim->desc,cim->desc->para.balance);
	cim->desc->set_effect(cim->desc,cim->desc->para.effect);
	cim->desc->set_flash_mode(cim->desc,cim->desc->para.flash_mode);
	cim->desc->set_focus_mode(cim->desc,cim->desc->para.focus_mode);
	cim->desc->set_fps(cim->desc,cim->desc->para.fps);
	cim->desc->set_scene_mode(cim->desc,cim->desc->para.scene_mode);
	cim->desc->set_preivew_mode(cim->desc);

	if(cim->tlb_flag) {
		cim_reset_tlb(cim);
		cim_enable_tlb_error_intr(cim);
		cim_set_tlbbase(cim);
		cim_enable_tlb(cim);
	}
	cim_set_da(cim,cim->preview);
	cim_clear_state(cim);	// clear state register
	cim_enable_dma(cim);
	cim_clear_rfifo(cim);	// resetting rxfifo
	cim_enable(cim);
	//cim_dump_reg(cim);
	return 0;
}

static long cim_start_capture(struct jz_cim *cim)
{
	struct jz_cim_dma_desc * dmadesc = (struct jz_cim_dma_desc *)cim->cdesc_vaddr;
	static int wait_count = 0;
	cim->state = CS_CAPTURE;
	wait_count = 0;
	cim_disable(cim);
	cim_clear_state(cim);	// clear state register
	cim_set_da(cim,cim->capture);
	cim_clear_rfifo(cim);	// resetting rxfifo
	cim_set_default(cim);
//	dev_info(cim->dev,"%s, %s, %d\n", __FILE__, __FUNCTION__, __LINE__);

	cim->desc->set_capture_mode(cim->desc);
	cim_set_capture_size(cim);
	if(cim->tlb_flag) {
		cim_reset_tlb(cim);
		cim_enable_tlb_error_intr(cim);
		cim_set_tlbbase(cim);
		cim_enable_tlb(cim);
	}
	cim_enable_dma(cim);
	cim_clear_rfifo(cim);	// resetting rxfifo
	cim_enable(cim);

	if(!interruptible_sleep_on_timeout(&cim->wait,msecs_to_jiffies(15000))) {
			cim_dump_reg(cim);
			dev_info(cim->dev,"cim ---------------capture wait timeout\n");
			cim_disable(cim);
			cim_clear_rfifo(cim);
			cim_clear_state(cim);
			return 0;
	}
	cim->state = CS_IDLE;
	return dmadesc[CDESC_NR-1].buf;
}
static unsigned long cim_get_preview_buf(struct jz_cim *cim)
{
	unsigned long addr ;
	unsigned long flags;

	struct jz_cim_dma_desc * desc = (struct jz_cim_dma_desc *)cim->pdesc_vaddr;
	while(cim->frm_id == -1) {
			if(cim->state  != CS_PREVIEW){
				dev_info(cim->dev,"cim state is not CS_PREVIEW,so return\n");
				return 0;
			}
			if(!interruptible_sleep_on_timeout(&cim->wait,msecs_to_jiffies(5000))){
				dev_info(cim->dev,"wait preview queue timeout!\n");
				cim_dump_reg(cim);
				return 0;
			}
	}
	spin_lock_irqsave(&cim->lock,flags);
	addr =  desc[cim->frm_id].buf;
	dev_dbg(cim->dev," -------------  frm id %d %08lx\n",cim->frm_id,addr);
	cim->frm_id = -1;
	spin_unlock_irqrestore(&cim->lock,flags);

	return addr;
}

void cim_get_sensor_info(struct jz_cim *cim, struct sensor_info *info)
{
	memset(info,0,sizeof(struct sensor_info));	
	info->sensor_id = cim->desc->id;
	strcpy(info->name,cim->desc->name);	
	info->facing = cim->desc->facing;
	info->orientation = cim->desc->orientation;
	info->cap_resolution_nr = cim->desc->cap_resolution_nr;
	info->prev_resolution_nr = cim->desc->prev_resolution_nr;
	memcpy(&info->modes,&cim->desc->modes,sizeof(struct mode_bit_map));
}
#if 0
static long cim_get_support_psize(struct jz_cim *cim,void __user *arg)//get preview resolutions
{
	long ret = 0;
	ret =  copy_to_user(arg,cim->desc->preview_size,
			(sizeof(struct frm_size) * cim->desc->prev_resolution_nr));
	return ret;
}

static long cim_get_support_csize(struct jz_cim *cim,void __user *arg)//get capture resolutions
{
	long ret = 0;
	ret =  copy_to_user(arg,cim->desc->capture_size,
			(sizeof(struct frm_size) * cim->desc->cap_resolution_nr));
	return ret;
}
#endif
static long cim_set_param(struct jz_cim *cim, int arg)
{
	// used app should use this ioctl like this :
	// ioctl(fd, CIMIO_SET_PARAM, CPCMD_SET_BALANCE | WHITE_BALANCE_AUTO);
	int  cmd,param_arg;
	cmd = arg & 0xffff0000;
	param_arg = arg & 0xffff;

	switch(cmd) {
		case CPCMD_SET_BALANCE:
			cim->desc->para.balance = (unsigned short)param_arg;
			if(cim->state == CS_IDLE)
				break;
			return cim->desc->set_balance(cim->desc,param_arg);
		case CPCMD_SET_EFFECT:
			cim->desc->para.effect= (unsigned short)param_arg;
			if(cim->state == CS_IDLE)
				break;
			return cim->desc->set_effect(cim->desc,param_arg);
		case CPCMD_SET_ANTIBANDING:
			cim->desc->para.antibanding= (unsigned short)param_arg;
			if(cim->state == CS_IDLE)
				break;
			return cim->desc->set_antibanding(cim->desc,param_arg);
		case CPCMD_SET_FLASH_MODE:
			cim->desc->para.flash_mode= (unsigned short)param_arg;
			if(cim->state == CS_IDLE)
				break;
			return cim->desc->set_flash_mode(cim->desc,param_arg);
		case CPCMD_SET_SCENE_MODE:
			cim->desc->para.scene_mode= (unsigned short)param_arg;
			if(cim->state == CS_IDLE)
				break;
			return cim->desc->set_scene_mode(cim->desc,param_arg);
		case CPCMD_SET_FOCUS_MODE:
			cim->desc->para.focus_mode= (unsigned short)param_arg;
			if(cim->state == CS_IDLE)
				break;
			return cim->desc->set_focus_mode(cim->desc,param_arg);
		case CPCMD_SET_FPS:
			cim->desc->para.fps= (unsigned short)param_arg;
			if(cim->state == CS_IDLE)
				break;
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

	///cim->preview = kzalloc(sizeof(struct jz_cim_dma_desc) * PDESC_NR,GFP_KERNEL);
	if (!cim->preview)
		return -ENOMEM;

	cim->cdesc_vaddr = dma_alloc_coherent(cim->dev,
			sizeof(*cim->capture) * CDESC_NR,(dma_addr_t *)&cim->capture, GFP_KERNEL);
	//cim->capture = kzalloc(sizeof(struct jz_cim_dma_desc) * CDESC_NR,GFP_KERNEL);

	if (!cim->capture)
		return -ENOMEM;

	return 0;
}

static int cim_prepare_pdma(struct jz_cim *cim, unsigned long addr)
{
	int i;
	unsigned int preview_frmsize = cim->psize.w *  cim->psize.h * 2;
	struct jz_cim_dma_desc * desc = (struct jz_cim_dma_desc *) cim->pdesc_vaddr;
	
	if(cim->state != CS_IDLE)
		return -EBUSY;
	
	for(i=0;i<PDESC_NR;i++) {
		desc[i].next = (dma_addr_t)(&cim->preview[i+1]);
		desc[i].id 	= i;
		desc[i].buf	= addr + i * preview_frmsize;
		desc[i].cmd = (preview_frmsize>>2) | CIM_CMD_EOFINT | CIM_CMD_OFRCV;	
		dev_info(cim->dev,"cim set preview buffer phys is %d  %x %x\n",i,desc[i].buf,desc[i].next);
	}

	desc[PDESC_NR-1].next = (dma_addr_t)cim->preview;
	//dma_cache_wback((unsigned long)(&cim->preview[0]), sizeof(struct jz_cim_dma_desc) *PDESC_NR);
	return 0;
}

static int cim_prepare_cdma(struct jz_cim *cim, unsigned long addr)
{
	int i;
	unsigned int capture_frmsize = cim->csize.w *  cim->csize.h * 2;
	struct jz_cim_dma_desc * desc = (struct jz_cim_dma_desc *) cim->cdesc_vaddr;
	//if(cim->state != CS_IDLE)
	//	return -EBUSY;
	for(i=0;i<CDESC_NR;i++) {
		desc[i].next = (dma_addr_t)(&cim->capture[i+1]);
		desc[i].id 	= i;
		desc[i].buf	= addr + i* capture_frmsize;
		desc[i].cmd = (capture_frmsize>>2) | CIM_CMD_OFRCV |CIM_CMD_EOFINT;
		//dev_info(cim->dev,"cim set capture buffer phys is %d  %x\n",i,cim->capture[i].buf);
		dev_info(cim->dev,"cim set capture buffer phys is %d  %x %x\n",i,desc[i].buf,desc[i].next);	
	}
	desc[CDESC_NR-1].next = (dma_addr_t)cim->capture;
	//cim->capture[CDESC_NR-1].cmd |= CIM_CMD_STOP;
	desc[CDESC_NR-1].cmd = (capture_frmsize>>2) | CIM_CMD_OFRCV |CIM_CMD_EOFINT;
	//dma_cache_wback((unsigned long)(&cim->capture[0]), sizeof(struct jz_cim_dma_desc) *CDESC_NR);
	return 0;
}

static int cim_open(struct inode *inode, struct file *file)
{	
	return 0;
}

static int cim_close(struct inode *inode, struct file *file)
{
	struct miscdevice *dev = file->private_data;
	struct jz_cim *cim = container_of(dev, struct jz_cim, misc_dev);

	cim->state = CS_IDLE;
	cim->tlb_flag = 0;
	cim->tlb_base = 0;
	cim->psize.h = 0;
	cim->psize.w = 0;
	cim->csize.w = 0;
	cim->csize.h = 0;
	return 0;
}

static long cim_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = file->private_data;
	struct jz_cim *cim = container_of(dev, struct jz_cim, misc_dev);
	void __user *argp = (void __user *)arg;
	long ret = 0;
	//dev_info(cim->dev," -------------------ioctl %x\n",cmd);
	switch (cmd) {
		case CIMIO_SHUTDOWN:
			return cim_shutdown(cim);
		case CIMIO_START_PREVIEW:
			return cim_start_preview(cim);
		case CIMIO_START_CAPTURE:
			return cim_start_capture(cim);
		case CIMIO_GET_FRAME:
			return cim_get_preview_buf(cim);
			break;
		case CIMIO_GET_SENSORINFO:
			{
				struct sensor_info info;
				cim_get_sensor_info(cim,&info);
				return copy_to_user(argp,&info,sizeof(struct sensor_info));
			}
		case CIMIO_GET_VAR:
			break;
		case CIMIO_GET_SUPPORT_PSIZE:
			return copy_to_user(argp,cim->desc->preview_size,
					(sizeof(struct frm_size) * cim->desc->prev_resolution_nr));
		case CIMIO_GET_SUPPORT_CSIZE:
			return copy_to_user(argp,cim->desc->capture_size,
					(sizeof(struct frm_size) * cim->desc->cap_resolution_nr));	
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
			ret = cim_set_preview_size(cim);
			break;
		case CIMIO_SET_CAPTURE_SIZE:
			if (copy_from_user(&cim->csize, (void __user *)arg, sizeof(struct frm_size)))
				return -EFAULT;
			ret = cim_set_capture_size(cim);
		case CIMIO_DO_FOCUS:
			break;
		case CIMIO_AF_INIT:
			break;
		case CIMIO_SET_VIDEO_MODE:
			ret = cim->desc->set_video_mode(cim->desc);
			break;
		case CIMIO_SET_TLB_BASE:
			cim->tlb_flag = 1;
			cim->tlb_base = arg;
			break;
		case CIMIO_GET_SENSOR_COUNT:
			return cim->sensor_count;
	}

	return ret;
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

	
	cim->clk = clk_get(&pdev->dev,"cim");
	if(IS_ERR(cim->clk)) {
		ret = -ENODEV;
		goto no_desc;
	}

	cim->mclk = clk_get(&pdev->dev,"cgu_cimmclk");
	if(IS_ERR(cim->mclk)) {
		ret = -ENODEV;
		goto io_failed;
	}
	#if 1
	cim->power = regulator_get(&pdev->dev, "vcim");
	if(IS_ERR(cim->power)){
		printk("fuchao -------------power get fail\n");
		ret = -ENODEV;
		goto mem_failed;
	}
	#endif

 	cim_scan_sensor(cim); 
	
	if(!cim->desc) {
		dev_err(&pdev->dev,"no sensor!\n");
		ret = -ENOMEM;
		goto io_failed1;
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
	spin_lock_init(&cim->lock);

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
	iounmap(cim->iomem);
	clk_put(cim->mclk);
	clk_put(cim->clk);
	free_irq(cim->irq,cim);
	misc_deregister(&cim->misc_dev);
	kfree(cim);
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


