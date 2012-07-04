
/*
 * I2C adapter for the INGENIC I2C bus access.
 *
 * Copyright (C) 2006 - 2009 Ingenic Semiconductor Inc.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/time.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/completion.h>
#include <linux/platform_device.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>

#include <mach/jzdma.h>

#define	I2C_CTRL		(0x00)
#define	I2C_TAR     		(0x04)
#define	I2C_SAR     		(0x08)
#define	I2C_DC      		(0x10)
#define	I2C_SHCNT		(0x14)
#define	I2C_SLCNT		(0x18)
#define	I2C_FHCNT		(0x1C)
#define	I2C_FLCNT		(0x20)
#define	I2C_INTST		(0x2C)
#define	I2C_INTM		(0x30)
#define I2C_RXTL		(0x38)
#define I2C_TXTL		(0x3c)
#define	I2C_CINTR		(0x40)
#define	I2C_CRXUF		(0x44)
#define	I2C_CRXOF		(0x48)
#define	I2C_CTXOF		(0x4C)
#define	I2C_CRXREQ		(0x50)
#define	I2C_CTXABRT		(0x54)
#define	I2C_CRXDONE		(0x58)
#define	I2C_CACT		(0x5C)
#define	I2C_CSTP		(0x60)
#define	I2C_CSTT		(0x64)
#define	I2C_CGC    		(0x68)
#define	I2C_ENB     		(0x6C)
#define	I2C_STA     		(0x70)
#define	I2C_TXABRT		(0x80)
#define I2C_DMACR            	(0x88)
#define I2C_DMATDLR          	(0x8c)
#define I2C_DMARDLR     	(0x90)
#define	I2C_SDASU		(0x94)
#define	I2C_ACKGC		(0x98)
#define	I2C_ENSTA		(0x9C)
#define I2C_SDAHD		(0xD0)

/* I2C Control Register (I2C_CTRL) */
#define I2C_CTRL_STPHLD		(1 << 7) /* Stop Hold Enable bit: when tx fifo empty, 0: send stop 1: never send stop*/
#define I2C_CTRL_SLVDIS		(1 << 6) /* after reset slave is disabled*/
#define I2C_CTRL_REST		(1 << 5)	
#define I2C_CTRL_MATP		(1 << 4) /* 1: 10bit address 0: 7bit addressing*/
#define I2C_CTRL_SATP		(1 << 3) /* 1: 10bit address 0: 7bit address*/
#define I2C_CTRL_SPDF		(2 << 1) /* fast mode 400kbps */
#define I2C_CTRL_SPDS		(1 << 1) /* standard mode 100kbps */
#define I2C_CTRL_MD		(1 << 0) /* master enabled*/

/* I2C Status Register (I2C_STA) */
#define I2C_STA_SLVACT		(1 << 6) /* Slave FSM is not in IDLE state */
#define I2C_STA_MSTACT		(1 << 5) /* Master FSM is not in IDLE state */
#define I2C_STA_RFF		(1 << 4) /* RFIFO if full */
#define I2C_STA_RFNE		(1 << 3) /* RFIFO is not empty */
#define I2C_STA_TFE		(1 << 2) /* TFIFO is empty */
#define I2C_STA_TFNF		(1 << 1) /* TFIFO is not full  */
#define I2C_STA_ACT		(1 << 0) /* I2C Activity Status */

/* I2C Transmit Abort Status Register (I2C_TXABRT) */
char *abrt_src[] = {
	"I2C_TXABRT_ABRT_7B_ADDR_NOACK",
	"I2C_TXABRT_ABRT_10ADDR1_NOACK",
	"I2C_TXABRT_ABRT_10ADDR2_NOACK",
	"I2C_TXABRT_ABRT_XDATA_NOACK",
	"I2C_TXABRT_ABRT_GCALL_NOACK",
	"I2C_TXABRT_ABRT_GCALL_READ",
	"I2C_TXABRT_ABRT_HS_ACKD",
	"I2C_TXABRT_SBYTE_ACKDET",
	"I2C_TXABRT_ABRT_HS_NORSTRT",
	"I2C_TXABRT_SBYTE_NORSTRT",
	"I2C_TXABRT_ABRT_10B_RD_NORSTRT",
	"I2C_TXABRT_ABRT_MASTER_DIS",
	"I2C_TXABRT_ARB_LOST",
	"I2C_TXABRT_SLVFLUSH_TXFIFO",
	"I2C_TXABRT_SLV_ARBLOST",
	"I2C_TXABRT_SLVRD_INTX",
};

/* i2c interrupt status (I2C_INTST) */
#define I2C_INTST_IGC                   (1 << 11)
#define I2C_INTST_ISTT                  (1 << 10)
#define I2C_INTST_ISTP                  (1 << 9)
#define I2C_INTST_IACT                  (1 << 8)
#define I2C_INTST_RXDN                  (1 << 7)
#define I2C_INTST_TXABT                 (1 << 6)
#define I2C_INTST_RDREQ                 (1 << 5)
#define I2C_INTST_TXEMP                 (1 << 4)
#define I2C_INTST_TXOF                  (1 << 3)
#define I2C_INTST_RXFL                  (1 << 2)
#define I2C_INTST_RXOF                  (1 << 1)
#define I2C_INTST_RXUF                  (1 << 0)

/* i2c interrupt mask status (I2C_INTM) */
#define I2C_INTM_MIGC			(1 << 11)
#define I2C_INTM_MISTT			(1 << 10)
#define I2C_INTM_MISTP			(1 << 9)
#define I2C_INTM_MIACT			(1 << 8)
#define I2C_INTM_MRXDN			(1 << 7)
#define I2C_INTM_MTXABT			(1 << 6)
#define I2C_INTM_MRDREQ			(1 << 5)
#define I2C_INTM_MTXEMP			(1 << 4)
#define I2C_INTM_MTXOF			(1 << 3)
#define I2C_INTM_MRXFL			(1 << 2)
#define I2C_INTM_MRXOF			(1 << 1)
#define I2C_INTM_MRXUF			(1 << 0)

#define I2C_DC_READ    			(1 << 8)
#define I2C_DC_WRITE   			(0 << 8)

#define I2C_SDAHD_HDENB			(1 << 8)

#define I2C_ENB_I2CENB			(1 << 0) /* Enable the i2c */ 

/* I2C standard mode high count register(I2CSHCNT) */
#define I2CSHCNT_ADJUST(n)      (((n) - 8) < 6 ? 6 : ((n) - 8))
/* I2C standard mode low count register(I2CSLCNT) */
#define I2CSLCNT_ADJUST(n)      (((n) - 1) < 8 ? 8 : ((n) - 1))
/* I2C fast mode high count register(I2CFHCNT) */
#define I2CFHCNT_ADJUST(n)      (((n) - 8) < 6 ? 6 : ((n) - 8))
/* I2C fast mode low count register(I2CFLCNT) */
#define I2CFLCNT_ADJUST(n)      (((n) - 1) < 8 ? 8 : ((n) - 1))


#define I2C_FIFO_LEN 16
#define TIMEOUT	0xff
#if 0
struct jz_i2c_speed {
	unsigned int speed;
	unsigned char slave_addr;
};
#endif
struct i2c_sg_data {
	struct scatterlist *sg;
	int sg_len;
	int flag;
};

struct jz_i2c {
	void __iomem *iomem;
	int irq;
	struct clk *clk;
	struct i2c_adapter adap;
	struct completion complete;
	struct dma_chan *chan;
	struct dma_async_tx_descriptor  *tx_desc;
	struct jzdma_slave dma_slave;
	enum jzdma_type dma_type;
	struct i2c_sg_data *data;
};


static inline unsigned long i2c_readl(struct jz_i2c *i2c,int offset)
{
	return readl(i2c->iomem + offset);
}

static inline void i2c_writel(struct jz_i2c *i2c,int offset,int value)
{
	writel(value,i2c->iomem + offset);
}

static int jz_i2c_disable(struct jz_i2c *i2c)
{
	int timeout = TIMEOUT;
	i2c_writel(i2c,I2C_ENB,0);
	while((i2c_readl(i2c,I2C_ENSTA) & I2C_ENB_I2CENB) && (--timeout > 0))
		udelay(10);
	
	return timeout?0:1;
}

static int jz_i2c_enable(struct jz_i2c *i2c)
{
	int timeout = TIMEOUT;
	i2c_writel(i2c,I2C_ENB,1);
	while(!(i2c_readl(i2c,I2C_ENSTA) & I2C_ENB_I2CENB) && (--timeout > 0))
		udelay(10);
	
	return timeout?0:1;
}

static irqreturn_t jz_i2c_irq(int irqno, void *dev_id)
{
	int tmp,intst;
	struct jz_i2c *i2c = dev_id;

	intst = i2c_readl(i2c,I2C_INTST);

	if(intst & I2C_INTST_RXFL) {
		tmp = i2c_readl(i2c,I2C_INTM);
		tmp &= ~(I2C_INTM_MRXFL);
		i2c_writel(i2c,I2C_INTM,tmp);
		complete(&i2c->complete);
	}

	if(intst & I2C_INTST_TXEMP) {
		tmp = i2c_readl(i2c,I2C_INTM);
		tmp &= ~I2C_INTM_MTXEMP;
		i2c_writel(i2c,I2C_INTM,tmp);
		complete(&i2c->complete);
	}

	if(intst & I2C_INTST_TXABT) {
		tmp = i2c_readl(i2c,I2C_INTM);
		tmp &= ~I2C_INTM_MTXABT;
		i2c_writel(i2c,I2C_INTM,tmp);
		complete(&i2c->complete);
	}

	return IRQ_HANDLED;
}

static void txabrt(int src)
{
	int i;
	for(i=0;i<16;i++) {
		if(src & (0x1 << i))
			printk("%s\n",abrt_src[i]);
	}
}

static void i2c_dma_complete(void *arg)
{
	struct jz_i2c *i2c = arg;
	dma_unmap_sg(NULL,i2c->data->sg,i2c->data->sg_len,i2c->data->flag);
	complete(&i2c->complete);
}

static inline int xfer_read(struct jz_i2c *i2c,char *buf,int len,int cnt,int idx)
{
	long i,tmp,timeout;
	struct i2c_sg_data data;
	struct dma_async_tx_descriptor *desc;

	if(idx < cnt - 1) {
		tmp = i2c_readl(i2c,I2C_CTRL);
		tmp |= I2C_CTRL_STPHLD;
		i2c_writel(i2c,I2C_CTRL,tmp);
	}

	if(len <= I2C_FIFO_LEN) {
		i2c_writel(i2c,I2C_RXTL,len - 1);
		tmp = i2c_readl(i2c,I2C_INTM);
		tmp |= I2C_INTM_MRXFL | I2C_INTM_MTXABT;
		i2c_writel(i2c,I2C_INTM,tmp);

		for(i=0;i<len;i++) {	//need wait txfifo is not full ???
			i2c_writel(i2c,I2C_DC,I2C_DC_READ);
		}

		if(idx == cnt - 1) {
			tmp = i2c_readl(i2c,I2C_CTRL);
			tmp &= ~I2C_CTRL_STPHLD;
			i2c_writel(i2c,I2C_CTRL,tmp);
		}
	
		//?????  conditional wait
		timeout = wait_for_completion_timeout(&i2c->complete,HZ);
		if(timeout) 
			return -EIO;

		tmp = i2c_readl(i2c,I2C_TXABRT);
		if(tmp) {
			txabrt(tmp);
			return -EIO;
		}

		while (len-- && ((i2c_readl(i2c,I2C_STA) & I2C_STA_RFNE)))
			*(buf++) = i2c_readl(i2c,I2C_DC) & 0xff;
	} else {
		/* use dma */
		dmaengine_device_control(i2c->chan, DMA_SLAVE_CONFIG,(unsigned long)&i2c->dma_slave);

//		dma_cache_sync(NULL, (void *)buf, len,DMA_FROM_DEVICE);
//		sg_dma_address(sg) = buf;
//		sg_dma_len(sg) = len;
		
		data.sg_len = 1;	
		data.sg = kzalloc(sizeof(struct scatterlist) * data.sg_len,GFP_KERNEL);
		data.flag = DMA_FROM_DEVICE;
		i2c->data = &data;

		sg_init_one(data.sg, buf, len);
		dma_map_sg(NULL,i2c->data->sg,i2c->data->sg_len,i2c->data->flag);

		desc  = i2c->chan->device->device_prep_slave_sg(i2c->chan,
				i2c->data->sg, i2c->data->sg_len, DMA_FROM_DEVICE,
				DMA_PREP_INTERRUPT | DMA_CTRL_ACK);

		desc->callback = i2c_dma_complete;
		desc->callback_param = i2c;

		 /* tx_submit */
		dmaengine_submit(desc);
		dma_async_issue_pending(i2c->chan);

		for(i=0;i<len;i++) {	//need wait txfifo is not full ???
			i2c_writel(i2c,I2C_DC,I2C_DC_READ);
		}

		if(idx == cnt - 1) {
			tmp = i2c_readl(i2c,I2C_CTRL);
			tmp &= ~I2C_CTRL_STPHLD;
			i2c_writel(i2c,I2C_CTRL,tmp);
		}
		
		timeout = wait_for_completion_timeout(&i2c->complete,HZ);
		if(timeout) 
			return -EIO;

		tmp = i2c_readl(i2c,I2C_TXABRT);
		if(tmp) {
			txabrt(tmp);
			return -EIO;
		}
	}

	return 0;
}

static inline int xfer_write(struct jz_i2c *i2c,char *buf,int len,int cnt,int idx)
{
	long tmp,timeout;
	struct i2c_sg_data data;
	struct dma_async_tx_descriptor *desc;

	tmp = i2c_readl(i2c,I2C_CTRL);
	tmp |= I2C_CTRL_STPHLD;
	i2c_writel(i2c,I2C_CTRL,tmp);

	if(len <= I2C_FIFO_LEN) {
		while(len--)
			i2c_writel(i2c,I2C_DC,*buf++ | I2C_DC_WRITE );

		i2c_writel(i2c,I2C_TXTL,0);
		tmp = i2c_readl(i2c,I2C_INTM);
		tmp |= I2C_INTM_MTXEMP | I2C_INTM_MTXABT;
		i2c_writel(i2c,I2C_INTM,tmp);

		if(idx == cnt - 1) {
			tmp = i2c_readl(i2c,I2C_CTRL);
			tmp &= ~I2C_CTRL_STPHLD;
			i2c_writel(i2c,I2C_CTRL,tmp);
		}

		timeout = wait_for_completion_timeout(&i2c->complete,HZ);

		if(timeout) 
			return -EIO;
		tmp = i2c_readl(i2c,I2C_TXABRT);
		if (tmp) {
			txabrt(tmp);
			return -EIO;
		}
	} else {
		/* use dma */
		dmaengine_device_control(i2c->chan, DMA_SLAVE_CONFIG,(unsigned long)&i2c->dma_slave);

		data.sg_len = 1;	
		data.sg = kzalloc(sizeof(struct scatterlist) * data.sg_len,GFP_KERNEL);
		data.flag = DMA_TO_DEVICE;
		i2c->data = &data;

		sg_init_one(data.sg, buf, len);
		dma_map_sg(NULL,i2c->data->sg,i2c->data->sg_len,i2c->data->flag);

		desc = i2c->chan->device->device_prep_slave_sg(i2c->chan,
				i2c->data->sg, i2c->data->sg_len, DMA_TO_DEVICE,
				DMA_PREP_INTERRUPT | DMA_CTRL_ACK);

		desc->callback = i2c_dma_complete;
		desc->callback_param = i2c;

		 /* tx_submit */
		dmaengine_submit(desc);
		dma_async_issue_pending(i2c->chan);

		if(idx == cnt - 1) {
			tmp = i2c_readl(i2c,I2C_CTRL);
			tmp &= ~I2C_CTRL_STPHLD;
			i2c_writel(i2c,I2C_CTRL,tmp);
		}
	
		timeout = wait_for_completion_timeout(&i2c->complete,HZ);
		if(timeout) 
			return -EIO;
		
		tmp = i2c_readl(i2c,I2C_TXABRT);
		if(tmp) {
			txabrt(tmp);
			return -EIO;
		}
	}

	return 0;
}

static int i2c_jz_xfer(struct i2c_adapter *adap, struct i2c_msg *msg, int count)
{
	int i,ret,tmp;
	struct jz_i2c *i2c = adap->algo_data;

	tmp = i2c_readl(i2c,I2C_STA);
	if(!(tmp & I2C_STA_TFE) || (tmp & I2C_STA_MSTACT))
		return -EBUSY;

	if (msg->addr != i2c_readl(i2c,I2C_TAR)) {
		i2c_writel(i2c,I2C_TAR,msg->addr);
#if 0
		i2c_init_clk(i2c,msg->addr);
#endif
	}

	for (i=0;i<count;i++,msg++) {
		if (msg->flags & I2C_M_RD)
			ret = xfer_read(i2c,msg->buf,msg->len,count,i);
		else
			ret = xfer_write(i2c,msg->buf,msg->len,count,i);

		if (ret) return ret;
	}

	return i;
}

static u32 i2c_jz_functionality(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static const struct i2c_algorithm i2c_jz_algorithm = {
	.master_xfer	= i2c_jz_xfer,
	.functionality	= i2c_jz_functionality,
};

static bool filter(struct dma_chan *chan, void *data)
{
	struct jz_i2c *i2c = data;
	return (void *)i2c->dma_type == chan->private;
}

static int i2c_set_speed(struct jz_i2c *i2c,int rate)
{
	int dev_clk = clk_get_rate(i2c->clk);
	int cnt_high = 0;	/* HIGH period count of the SCL clock */
	int cnt_low = 0;	/* LOW period count of the SCL clock */
	int cnt_period = 0;	/* period count of the SCL clock */
	int setup_time = 0;
	int hold_time = 0;
	int tmp;

	if (rate <= 0 || rate > 400000)
		goto Set_speed_err;

	cnt_period = dev_clk / rate;
	if (rate <= 100000) {
		/* i2c standard mode, the min LOW and HIGH period are 4700 ns and 4000 ns */
		cnt_high = (cnt_period * 4000) / (4700 + 4000);
	} else {
		/* i2c fast mode, the min LOW and HIGH period are 1300 ns and 600 ns */
		cnt_high = (cnt_period * 600) / (1300 + 600);
	}

	cnt_low = cnt_period - cnt_high;

	if(jz_i2c_disable(i2c))
		printk("i2c not disable\n");

	if (rate <= 100000) {
		tmp = 0x43 | (1<<5);      /* standard speed mode*/
		i2c_writel(i2c,I2C_CTRL,tmp);
		i2c_writel(i2c,I2C_SHCNT,I2CSHCNT_ADJUST(cnt_high));
		i2c_writel(i2c,I2C_SLCNT,I2CSLCNT_ADJUST(cnt_low));
		setup_time = 400;
		hold_time = 400;
	} else {
		tmp = 0x45 | (1<<5);      /* fast speed mode*/
		i2c_writel(i2c,I2C_CTRL,tmp);
		i2c_writel(i2c,I2C_FHCNT,I2CFHCNT_ADJUST(cnt_high));
		i2c_writel(i2c,I2C_FLCNT,I2CFLCNT_ADJUST(cnt_low));
		setup_time = 400;
		hold_time = 0;
	}

	hold_time = (setup_time / (1000000000 / dev_clk) - 1);
	setup_time = (setup_time / (1000000000 / dev_clk) + 1);

	if (setup_time > 255)
		setup_time = 255;
	if (setup_time <= 0)
		setup_time = 1;
	if (hold_time > 255)
		hold_time = 255;

	printk("%s %d\n",__func__,__LINE__);
	i2c_writel(i2c,I2C_SDASU,setup_time & 0xff);
	
	if (hold_time <= 0) {
		tmp = i2c_readl(i2c,I2C_SDAHD);
		tmp &= ~(I2C_SDAHD_HDENB);	/*i2c hold time disable */
		i2c_writel(i2c,I2C_SDAHD,tmp);
	} else {
		tmp = hold_time & 0xff;
		tmp |= I2C_SDAHD_HDENB;		/*i2c hold time enable */
		i2c_writel(i2c,I2C_SDAHD,tmp);
	}

	return 0;

Set_speed_err:
	printk("Faild : i2c set sclk faild,rate=%d KHz,dev_clk=%dKHz.\n", rate, dev_clk);
	return -1;
}

static int i2c_jz_probe(struct platform_device *dev)
{
	int ret = 0;
	struct resource *r;
	dma_cap_mask_t mask;

	struct jz_i2c *i2c = kzalloc(sizeof(struct jz_i2c), GFP_KERNEL);
	if (!i2c) {
		ret = -ENOMEM;
		goto no_mem;
	}

	i2c->adap.owner   	= THIS_MODULE;
	i2c->adap.algo    	= &i2c_jz_algorithm;
	i2c->adap.retries 	= 6;
	i2c->adap.algo_data 	= i2c;
	i2c->adap.dev.parent 	= &dev->dev;
	i2c->adap.nr 		= dev->id;
	sprintf(i2c->adap.name, "i2c%u", dev->id);

	i2c->clk = clk_get(&dev->dev,i2c->adap.name);
	if(!i2c->clk) {
		ret = -ENODEV;
		goto clk_failed;
	}

	r = platform_get_resource(dev, IORESOURCE_MEM, 0);
	i2c->iomem = ioremap(r->start,resource_size(r));
	if (!i2c->iomem) {
		ret = -ENOMEM;
		goto io_failed;
	}

	i2c->irq = platform_get_irq(dev, 0);
	ret = request_irq(i2c->irq, jz_i2c_irq, IRQF_DISABLED,dev_name(&dev->dev), i2c);
	if(ret) {
		ret = -ENODEV;
		goto irq_failed;
	}

	i2c_set_speed(i2c,100000);

	i2c_writel(i2c,I2C_DMATDLR,0);	/*set trans fifo level*/
	i2c_writel(i2c,I2C_DMARDLR,0);	/*set recive fifo level*/
	i2c_writel(i2c,I2C_DMACR,1);	/*enable i2c dma*/
	
	init_completion(&i2c->complete);

	i2c->dma_slave.reg_width = 1;	//0-32bit,1-8bit 2-16bit
	i2c->dma_slave.max_tsz = 8;	//for device use FIFO, it's equal to half of FIFO size
	i2c->dma_slave.tx_reg = (unsigned long)(i2c->iomem + I2C_DC);
	i2c->dma_slave.rx_reg = (unsigned long)(i2c->iomem + I2C_DC);

	r = platform_get_resource(dev, IORESOURCE_DMA, 0);
	i2c->dma_type = r->start;

	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	i2c->chan = dma_request_channel(mask, filter, i2c);

	ret = i2c_add_numbered_adapter(&i2c->adap);
	if (ret < 0) {
		printk(KERN_INFO "I2C: Failed to add bus\n");
		goto adapt_failed;
	}

	platform_set_drvdata(dev, i2c);

	jz_i2c_enable(i2c);

	return 0;

adapt_failed:
	free_irq(i2c->irq,i2c);
irq_failed:
	iounmap(i2c->iomem);
io_failed:
	clk_put(i2c->clk);
clk_failed:
	kfree(i2c);
no_mem:
	return ret;
}

static int i2c_jz_remove(struct platform_device *dev)
{
	struct jz_i2c *i2c = platform_get_drvdata(dev);
	free_irq(i2c->irq,i2c);
	iounmap(i2c->iomem);
	clk_put(i2c->clk);
	i2c_del_adapter(&i2c->adap);
	kfree(i2c);
	return 0;
}

static struct platform_driver jz_i2c_driver = {
	.probe		= i2c_jz_probe,
	.remove		= i2c_jz_remove,
	.driver		= {
		.name	= "jz-i2c",
	},
};

static int __init jz4780_i2c_init(void)
{
	return platform_driver_register(&jz_i2c_driver);
}

static void __exit jz4780_i2c_exit(void)
{
	platform_driver_unregister(&jz_i2c_driver);
}

subsys_initcall(jz4780_i2c_init);
module_exit(jz4780_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ztyan<ztyan@ingenic.cn>");
MODULE_DESCRIPTION("i2c driver for JZ47XX SoCs");

