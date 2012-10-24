#include <linux/platform_device.h>
#include "nand_api.h"
#include <soc/gpio.h>
#include <asm/page.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/sched.h>

//#define NAND_DMA_CALC_TIME
#ifdef NAND_DMA_CALC_TIME
static long long time =0;
static inline  void b_time(void)
{
	time = sched_clock();
}
static inline void e_time(void)
{
	long long etime = sched_clock();
	printk("time = %llu ********",etime-time);
}
#endif

#define CLEAR_GPIO_FLAG(n)         \
	do {						\
		*(volatile unsigned int *)(0xB0010058+0x100*((n)/32)) = 0x1 << ((n)%32);        \
	} while (0)

#define GET_PHYADDR(a)											\
	({												\
	 unsigned int v;										\
	 if (unlikely((int)(a) & 0x40000000)) {							\
	 v = page_to_phys(vmalloc_to_page((const void *)(a))) | ((int)(a) & ~PAGE_MASK); \
	 } else											\
	 v = ((_ACAST32_((int)(a))) & 0x1fffffff);					\
	 v;		        								\
	 })

/*
 * DMA_TO_MBUF: copy databuf to nand_dma->data_buf
 * DMA_FROM_MBUF: copy nand_dma->data_buf to databuf
 */
enum buf_direction {
	DMA_TO_MBUF,
	DMA_FROM_MBUF,
};

static struct completion comp;
static volatile int mailbox_ret = 0;

static inline void enable_rb_irq(const NAND_API *pnand_api)
{
	int rb_irq, irq;
	rb_irq = pnand_api->vnand_base->rb_irq;
	irq = pnand_api->vnand_base->irq;
	CLEAR_GPIO_FLAG(irq);
	enable_irq(rb_irq);
}

static inline void disable_rb_irq(const NAND_API *pnand_api)
{
	int rb_irq;
	rb_irq = pnand_api->vnand_base->rb_irq;
	disable_irq_nosync(rb_irq);
}

static void data_complete_func(void *arg)
{
	complete(&comp);
}

static void mcu_complete_func(void *arg)
{
	int mailbox = *(int *)arg;
//	unsigned int times = (mailbox & (0xffff<<8))>>8;
//	if(times)
//		printk("^^^^^ the time of mcu operation is : %d, %08x\n",times,mailbox);
//	mailbox &= 0xff;
	switch (mailbox) {
		case MB_NAND_INIT_DONE:
			mailbox_ret = 0;
			break;
		case MB_NAND_READ_DONE:
			mailbox_ret = 0;
			break;
		case MB_NAND_UNCOR_ECC:
			mailbox_ret = -5;
			break;
		case MB_NAND_WRITE_DONE:
			mailbox_ret = 0;
			break;
		case MB_NAND_WRITE_FAIL:
			mailbox_ret = -1;
			break;
		case MB_NAND_WRITE_PROTECT:
			mailbox_ret = 111;
			break;
		case MB_NAND_ERASE_DONE:
			mailbox_ret = 0;
			break;
		case MB_NAND_ERASE_FAIL:
			mailbox_ret = -1;
			break;
		case MB_NAND_ALL_FF:
			mailbox_ret = -6;
			break;
		case MB_MOVE_BLOCK:
			mailbox_ret = 1;
			break;
	}
	complete(&comp);
}

static bool filter(struct dma_chan *chan, void *data)
{
	struct jznand_dma *nand_dma = data;
	return (void *)nand_dma->chan_type == chan->private;
}

static inline int do_select_chip(const NAND_API *pnand_api, unsigned int page)
{
	int chipnr = -1;
	unsigned int page_per_chip = pnand_api->nand_chip->ppchip;
	if (page > 0)
		chipnr = page / page_per_chip + 1;
	return chipnr;
}

/**
 * do_deselect_chip -
 */
static inline void do_deselect_chip(const NAND_API *pnand_api)
{
	//	pnand_api->nand_ctrl->chip_select(pnand_api->vnand_base,pnand_api->nand_io,-1);
}

/*   get_physical_addr  */
static unsigned int get_physical_addr(const NAND_API *pnand_api, unsigned int vpage)
{
	unsigned int page = vpage + pnand_api->nand_dma->ppt->startPage;
	struct platform_nand_partition *pt = (struct platform_nand_partition *)(pnand_api->nand_dma->ppt->prData);
	unsigned int tmp =page / pnand_api->nand_chip->ppblock;
	unsigned int toppage = (tmp - (tmp % pnand_api->nand_chip->planenum)) * pnand_api->nand_chip->ppblock;
	if(pt->use_planes)
		page = ((page-toppage) / pnand_api->nand_chip->planenum) +
			(pnand_api->nand_chip->ppblock * ((page-toppage) %
											  pnand_api->nand_chip->planenum)) + toppage;

	return page;
}

static int wait_dma_finish(struct dma_chan *chan,struct dma_async_tx_descriptor *desc,
		void *callback, void *callback_param)
{
	unsigned int timeout = 0;
	dma_cookie_t cookie;
	desc->callback = callback;
	desc->callback_param = callback_param;

	cookie = dmaengine_submit(desc);
	if (dma_submit_error(cookie)) {
		printk("Failed: to do DMA submit\n");
		return -2;  // error memory
	}

	dma_async_issue_pending(chan);
	timeout = wait_for_completion_timeout(&comp,HZ);
	if(!timeout)
		return -4;  // this operation is timeout
	return 0;
}

/*  set_rw_msg  */
static void set_rw_msg(struct jznand_dma *nand_dma,int nand_cs, int rw, int phy_pageid, void *databuf)
{
	if(rw)
		nand_dma->msg->cmd = MSG_NAND_READ;
	else
		nand_dma->msg->cmd = MSG_NAND_WRITE;
	nand_dma->msg->info[MSG_NAND_BANK] = nand_cs;
	nand_dma->msg->info[MSG_DDR_ADDR] = GET_PHYADDR(databuf);
	nand_dma->msg->info[MSG_PAGEOFF] = phy_pageid;
}

static int send_msg_to_mcu(const NAND_API *pnand_api)
{
	int ret = 0;
	struct jznand_dma *nand_dma = pnand_api->nand_dma;
	struct device *nand_dev =nand_dma->mcu_chan->device->dev;
	unsigned long flags = DMA_PREP_INTERRUPT | DMA_CTRL_ACK;
	//enum dma_data_direction direction =DMA_TO_DEVICE;

	dma_sync_single_for_device(nand_dev,nand_dma->msg_phyaddr,sizeof(struct pdma_msg),
			DMA_TO_DEVICE);
	//	sg_init_one(nand_dma->sgl, &nand_dma->msg, sizeof(nand_dma->msg));
	/*
	   ret = dma_map_sg(nand_dev, &nand_dma->sg, 1, direction);
	   if(ret < 0) {
	   printk("Failed: to do dma map sgl\n");
	   ret = -1;
	   goto err_map;
	   }
	   printk("@@@@@@@@@ mcu dma chan start  @@@@@@@@@@\n");
	   nand_dma->desc = nand_dma->mcu_chan->device->device_prep_slave_sg(nand_dma->mcu_chan,
	   &nand_dma->sg, 1, direction, flags);
	 */
	nand_dma->desc = nand_dma->mcu_chan->device->device_prep_dma_memcpy(nand_dma->mcu_chan,CPHYSADDR(PDMA_MSG_TCSMVA),
			nand_dma->msg_phyaddr,sizeof(struct pdma_msg),flags);
	if(nand_dma->desc == NULL) {
		printk("Failed: nand mcu dma desc is NULL\n");
		ret = -1;
		goto err_desc;
	}

	//printk("@@nand dma ops@@ src = 0x%x @@@@@\n", CPHYSADDR(nand_dma->msg) );
	ret = wait_dma_finish(nand_dma->mcu_chan,nand_dma->desc, mcu_complete_func,
			&nand_dma->mailbox);
	if(ret < 0) {
		printk("Failed: mcu dma tran\n");
	} else {
		//printk("Success: mcu dma tran\n");
		ret = mailbox_ret;
		mailbox_ret = 0;
	}

err_desc:
	//	dma_unmap_sg(nand_dev, &nand_dma->sg, 1, direction);
	//err_map:
	return ret;
}

static int mcu_reset(const NAND_API *pnand_api)
{
	struct jznand_dma *nand_dma =pnand_api->nand_dma;
	PPartition *ppt =nand_dma->ppt;
	struct platform_nand_partition *pt = (struct platform_nand_partition *)ppt->prData;
	nand_dma->msg->cmd = MSG_NAND_INIT;
	nand_dma->msg->info[MSG_NANDTYPE] = 0;
	nand_dma->msg->info[MSG_PAGESIZE] = ppt->byteperpage;
	nand_dma->msg->info[MSG_OOBSIZE]  = pnand_api->nand_chip->oobsize;
	nand_dma->msg->info[MSG_ROWCYCLE] = pnand_api->nand_chip->row_cycles;
	nand_dma->msg->info[MSG_ECCLEVEL] = pt->eccbit;
	nand_dma->msg->info[MSG_ECCSIZE]  = pnand_api->nand_ecc->eccsize;
	nand_dma->msg->info[MSG_ECCBYTES] = __bch_cale_eccbytes(pt->eccbit);
	nand_dma->msg->info[MSG_ECCSTEPS] = ppt->byteperpage / pnand_api->nand_ecc->eccsize ;
	nand_dma->msg->info[MSG_ECCTOTAL] = __bch_cale_eccbytes(pt->eccbit)  * nand_dma->msg->info[MSG_ECCSTEPS];
	nand_dma->msg->info[MSG_ECCPOS]   = pnand_api->nand_ecc->eccpos;

	return send_msg_to_mcu(pnand_api);
}

/*  databuf_between_dmabuf  */
static int databuf_between_dmabuf(struct jznand_dma *nand_dma,int offset, int bytes, void *databuf,enum buf_direction direction)
{
	int ret = 0;
	unsigned long flag = DMA_PREP_INTERRUPT | DMA_CTRL_ACK;
	struct dma_chan *data_chan = nand_dma->data_chan;
	dma_addr_t dma_dest;
	dma_addr_t dma_src;

	if(direction){
		dma_src = nand_dma->data_buf_phyaddr+offset;
		dma_dest = GET_PHYADDR((unsigned char *)databuf);
	}else{
		dma_dest = nand_dma->data_buf_phyaddr+offset;
		dma_src  = GET_PHYADDR((unsigned char *)databuf);
	}

	nand_dma->desc = data_chan->device->device_prep_dma_memcpy(data_chan,dma_dest,dma_src,bytes,flag);
	if(!(nand_dma->desc))
		ret = -2;  // error memory
	return ret;
}

static int read_page_singlenode(const NAND_API *pnand_api,int pageid,int offset,int bytes,void *databuf)
{
	int ret =0,ret1=0,cs=0;
	int rw = NAND_DMA_READ;
	int phy_pageid;
	struct jznand_dma *nand_dma =pnand_api->nand_dma;
	struct device *nand_dev =nand_dma->data_chan->device->dev;
	int byteperpage =pnand_api->nand_dma->ppt->byteperpage;
#ifdef NAND_DMA_CALC_TIME
	b_time();
#endif
	if(bytes == 0 || (bytes + offset) > byteperpage){
		ret =-1;
		goto read_page_singlenode_error1;
	}
	phy_pageid =get_physical_addr(pnand_api,pageid);
	cs =do_select_chip(pnand_api,phy_pageid);
	dma_sync_single_for_device(nand_dev,GET_PHYADDR(databuf),bytes,DMA_TO_DEVICE);
#ifdef NAND_DMA_CALC_TIME
	e_time();
	printk("  %s  %d\n",__func__,__LINE__);
	b_time();
#endif
	if(bytes < byteperpage){
		set_rw_msg(nand_dma,cs,rw,phy_pageid,nand_dma->data_buf);
		ret =send_msg_to_mcu(pnand_api);
		if(ret<0)
			goto read_page_singlenode_error1;
		databuf_between_dmabuf(nand_dma,offset,bytes,databuf,DMA_FROM_MBUF);
		ret1 = wait_dma_finish(nand_dma->data_chan,nand_dma->desc,data_complete_func,NULL);
		if(ret1<0){
			ret =ret1;
			goto read_page_singlenode_error1;
		}
	}else{
		set_rw_msg(nand_dma,cs,rw,phy_pageid,databuf);
		ret =send_msg_to_mcu(pnand_api);
		if(ret<0)
			goto read_page_singlenode_error1;
	}
#ifdef NAND_DMA_CALC_TIME
	e_time();
	printk("  %s  %d\n",__func__,__LINE__);
	b_time();
#endif
//	dma_sync_single_for_device(nand_dev,GET_PHYADDR(databuf),bytes,DMA_FROM_DEVICE);
read_page_singlenode_error1:

#ifdef NAND_DMA_CALC_TIME
	e_time();
	printk("  %s  %d\n",__func__,__LINE__);
#endif
	do_deselect_chip(pnand_api);
	return ret;
}

int nand_dma_read_page(const NAND_API *pnand_api,int pageid,int offset,int bytes,void *databuf)
{
	int ret = 0;
	disable_rb_irq(pnand_api);
	//printk("\n@@ read page start @@\n   ..\n");
	ret =mcu_reset(pnand_api);
	if(ret < 0)
		goto nand_dma_read_page_error;
	ret = read_page_singlenode(pnand_api, pageid, offset, bytes, databuf);

	//printk("@@ read page ret = 0x%08x @@\n",ret);
	if (ret == 0)
		ret = bytes;
nand_dma_read_page_error:
	enable_rb_irq(pnand_api);
	return ret;
}

static int write_page_singlenode(const NAND_API *pnand_api,int pageid,int offset,int bytes,void *databuf)
{
	int ret =0,cs=0;
	int phy_pageid;
	struct jznand_dma *nand_dma =pnand_api->nand_dma;
	//struct platform_device *pdev = (struct platform_device *)pnand_api->pdev;
	struct device *nand_dev =nand_dma->data_chan->device->dev;
	int byteperpage =pnand_api->nand_dma->ppt->byteperpage;
	if(bytes == 0 || (bytes + offset) > byteperpage){
		ret =-1;
		goto write_page_singlenode_error1;
	}
	phy_pageid =get_physical_addr(pnand_api,pageid);
	cs =do_select_chip(pnand_api,phy_pageid);
#ifdef NAND_DMA_CALC_TIME
	b_time();
#endif
	if(bytes < byteperpage){
		memset(nand_dma->data_buf,0xff,byteperpage);
		dma_sync_single_for_device(nand_dev,CPHYSADDR(nand_dma->data_buf),byteperpage,       DMA_TO_DEVICE);
		dma_sync_single_for_device(nand_dev,GET_PHYADDR(databuf),bytes,DMA_TO_DEVICE);
		ret =databuf_between_dmabuf(nand_dma,offset,bytes,databuf,DMA_TO_MBUF);
		if(ret<0)
			goto write_page_singlenode_error1;
		ret =wait_dma_finish(nand_dma->data_chan,nand_dma->desc,data_complete_func,NULL);
		if(ret<0)
			goto write_page_singlenode_error1;
		set_rw_msg(nand_dma,cs,NAND_DMA_WRITE,phy_pageid,nand_dma->data_buf);
	}else{
		dma_sync_single_for_device(nand_dev,GET_PHYADDR(databuf),bytes,DMA_TO_DEVICE);
		set_rw_msg(nand_dma,cs,NAND_DMA_WRITE,phy_pageid,databuf);
	}
#ifdef NAND_DMA_CALC_TIME
	e_time();
	printk("  %s  %d\n",__func__,__LINE__);
	b_time();
#endif
	ret =send_msg_to_mcu(pnand_api);
#ifdef NAND_DMA_CALC_TIME
	e_time();
	printk("  %s  %d\n",__func__,__LINE__);
#endif
	do_deselect_chip(pnand_api);
write_page_singlenode_error1:
	return ret;
}

int nand_dma_write_page(const NAND_API *pnand_api,int pageid,int offset,int bytes,void *databuf)
{
	int ret = 0;
	disable_rb_irq(pnand_api);
	//printk("\n@@ write page start @@\n   ..\n");
	//	jzgpio_set_func(GPIO_PORT_A,GPIO_INPUT,0x00100000);
	ret = mcu_reset(pnand_api);
	if(ret < 0)
		goto nand_dma_write_page_error;

	ret = write_page_singlenode(pnand_api, pageid, offset, bytes, databuf);

	//printk("@@ write page ret = 0x%08x @@\n",ret);
	if (ret == 0)
		ret = bytes;
nand_dma_write_page_error:
	enable_rb_irq(pnand_api);
	return ret;
}

static int read_page_multinode(const NAND_API *pnand_api,PageList *pagelist,unsigned int temp)
{
	int ret = 0,ret1 =0,cs = 0;
	int num = 0;
	PageList *templist =pagelist;
	struct singlelist *listhead=0;
	int phy_pageid;
	struct jznand_dma *nand_dma =pnand_api->nand_dma;
	//struct platform_device *pdev = (struct platform_device *)pnand_api->pdev;
	struct device *nand_dev =nand_dma->data_chan->device->dev;
	int byteperpage =pnand_api->nand_dma->ppt->byteperpage;

	int pageid =templist->startPageID;
	phy_pageid =get_physical_addr(pnand_api,pageid);
	cs =do_select_chip(pnand_api,phy_pageid);
#ifdef NAND_DMA_CALC_TIME
	b_time();
#endif
	for(num = 0; num < temp; num++){
		dma_sync_single_for_device(nand_dev,GET_PHYADDR(templist->pData),templist->Bytes,DMA_TO_DEVICE);
		listhead = (templist->head).next;
		templist = singlelist_entry(listhead,PageList,head);
	}
	set_rw_msg(nand_dma,cs,NAND_DMA_READ,phy_pageid,nand_dma->data_buf);
	ret = send_msg_to_mcu(pnand_api);
#ifdef NAND_DMA_CALC_TIME
	e_time();
	printk("  %s  %d\n",__func__,__LINE__);
	b_time();
#endif
	if (ret < 0)
		goto read_page_node_error1;
	templist = pagelist;
	for (num = 0; num < temp; num++) {
		if (templist->Bytes == 0 || (templist->Bytes + templist->OffsetBytes) > byteperpage) {
			ret = -1;
			templist->retVal = ret;
			break;
		}
		ret1 = databuf_between_dmabuf(nand_dma, templist->OffsetBytes, templist->Bytes,
				templist->pData, DMA_FROM_MBUF);
		if (ret1) {
			printk("read_page_multinode databuf_between_dmabuf error.\n");
			templist->retVal = ret1;
			break;
		}
		listhead = (templist->head).next;
		templist = singlelist_entry(listhead,PageList,head);
	}
#ifdef NAND_DMA_CALC_TIME
	e_time();
	printk("  %s  %d\n",__func__,__LINE__);
#endif
	if (num > 0) {
		ret1 = wait_dma_finish(nand_dma->data_chan, nand_dma->desc, data_complete_func, NULL);
		if(ret1)
			ret =ret1;
		templist = pagelist;
		while (num--) {
			switch (ret) {
				case 0:
					templist->retVal = templist->Bytes;
					dma_sync_single_for_device(nand_dev, GET_PHYADDR(templist->pData),
							templist->Bytes, DMA_FROM_DEVICE);
					break;
				case 1:
					templist->retVal = templist->Bytes | (1<<16);
					dma_sync_single_for_device(nand_dev, GET_PHYADDR(templist->pData),
							templist->Bytes, DMA_FROM_DEVICE);
					break;
				default:
					templist->retVal = ret;
					break;
			}
			listhead = (templist->head).next;
			templist = singlelist_entry(listhead,PageList,head);
		}
	}
read_page_node_error1:
	do_deselect_chip(pnand_api);
	return ret;
}

int nand_dma_read_pages(const NAND_API *pnand_api, Aligned_List *list)
{
	Aligned_List *alignelist = list;
	PageList *templist;
	unsigned int opsmodel;
	int ret = 0,flag =0;
	disable_rb_irq(pnand_api);
	//printk("\n@@ read pages start @@\n   ..\n");
	ret = mcu_reset(pnand_api);
	if(ret < 0)
		goto dma_read_pages_error1;
	while(alignelist != NULL) {
		opsmodel = alignelist->opsmodel & 0x00ffffff;
		templist =alignelist->pagelist;
		if(opsmodel == 1){
			ret = read_page_singlenode(pnand_api,templist->startPageID,
					templist->OffsetBytes,templist->Bytes,templist->pData);
			switch(ret){
				case 0:
					templist->retVal = templist->Bytes;
					break;
				case 1:
					templist->retVal = templist->Bytes | (1<<16);
					break;
				default:
					templist->retVal = ret;
					break;
			}
		}else{
			ret = read_page_multinode(pnand_api,templist,opsmodel);
		}
		if(ret < 0){
			flag = 0;
			break;
		}
		if(ret == 1)
			flag = 1;
		alignelist = alignelist->next;
	}
	if(flag)
		ret=1;
dma_read_pages_error1:
	enable_rb_irq(pnand_api);
	//printk("@@ read pages ret = 0x%08x @@\n",ret);
	return ret;
}

static int write_page_multinode(const NAND_API *pnand_api,PageList *pagelist,unsigned int temp)
{
	int ret=0,cs=0;
	int num =0;
	PageList *templist = pagelist;
	struct singlelist *listhead=0;
	int phy_pageid;
	struct jznand_dma *nand_dma =pnand_api->nand_dma;
	//struct platform_device *pdev = (struct platform_device *)pnand_api->pdev;
	struct device *nand_dev =nand_dma->data_chan->device->dev;
	int byteperpage =pnand_api->nand_dma->ppt->byteperpage;
	int pageid = templist->startPageID;
	phy_pageid =get_physical_addr(pnand_api,pageid);
	cs =do_select_chip(pnand_api,phy_pageid);

	memset(nand_dma->data_buf,0xff,byteperpage);
	dma_sync_single_for_device(nand_dev,CPHYSADDR(nand_dma->data_buf),byteperpage,DMA_TO_DEVICE);

	for(num = 0; num < temp; num++){
		if (templist->Bytes == 0 || (templist->Bytes + templist->OffsetBytes)>byteperpage) {
			ret =-1;
			templist->retVal = ret;
			break;
		}
		dma_sync_single_for_device(nand_dev, GET_PHYADDR(templist->pData), templist->Bytes, DMA_TO_DEVICE);
		ret=databuf_between_dmabuf(nand_dma, templist->OffsetBytes, templist->Bytes,
				templist->pData, DMA_TO_MBUF);
		if (ret) {
			printk("write_page_multinode databuf_between_dmabuf error.\n");
			templist->retVal = ret;
			break;
		}
		listhead = (templist->head).next;
		templist = singlelist_entry(listhead,PageList,head);
	}
	if (num > 0) {
		ret = wait_dma_finish(nand_dma->data_chan, nand_dma->desc, data_complete_func, NULL);
		if(ret)
			goto write_multinode_error1;
		set_rw_msg(nand_dma, cs, NAND_DMA_WRITE, phy_pageid, nand_dma->data_buf);
		ret = send_msg_to_mcu(pnand_api);
write_multinode_error1:
		templist = pagelist;
		while (num--) {
			switch (ret) {
				case 0:
					templist->retVal = templist->Bytes;
					dma_sync_single_for_device(nand_dev, GET_PHYADDR(templist->pData),
							templist->Bytes, DMA_FROM_DEVICE);
					break;
				case 1:
					templist->retVal = templist->Bytes | (1<<16);
					dma_sync_single_for_device(nand_dev, GET_PHYADDR(templist->pData),
							templist->Bytes, DMA_FROM_DEVICE);
					break;
				default:
					templist->retVal = ret;
					break;
			}
			listhead = (templist->head).next;
			templist = singlelist_entry(listhead,PageList,head);
		}
	}

	do_deselect_chip(pnand_api);
	return ret;
}

int nand_dma_write_pages(const NAND_API *pnand_api, Aligned_List *list)
{
	Aligned_List *alignelist = list;
	PageList *templist;
	unsigned int opsmodel;
	int ret = 0;
	disable_rb_irq(pnand_api);
	//printk("\n@@ write pages start @@\n   ..\n");
	ret = mcu_reset(pnand_api);
	if(ret < 0)
		goto dma_write_pages_error1;
	while(alignelist != NULL) {
		opsmodel = alignelist->opsmodel & 0x00ffffff;
		templist =alignelist->pagelist;
		if(opsmodel == 1){
			ret = write_page_singlenode(pnand_api,templist->startPageID,
					templist->OffsetBytes,templist->Bytes,templist->pData);
			templist->retVal = ret;
		}else{
			ret = write_page_multinode(pnand_api,templist,opsmodel);
		}
		if(ret<0)
			break;
		alignelist =alignelist->next;
	}
dma_write_pages_error1:
	enable_rb_irq(pnand_api);
	//printk("@@ write pages ret = 0x%08x @@\n",ret);
	return ret;
}

int nand_dma_init(NAND_API *pnand_api)
{
	int ret = 0;
	dma_cap_mask_t mask;
	struct resource *regs;
	struct platform_device *pdev = (struct platform_device *)pnand_api->pdev;

	struct jznand_dma *nand_dma =(struct jznand_dma *)nand_malloc_buf(sizeof(struct jznand_dma));
	if(!nand_dma){
		printk("Failed: nand_dma mallocs failed !\n");
		goto nand_dma_init_error1;
	}
	memset(nand_dma,0,sizeof(struct jznand_dma));

	/*  request message channel,which be used for sending message to mcu  */
	regs = platform_get_resource(pdev,IORESOURCE_DMA,0);
	if (!regs) {
		dev_err(&pdev->dev, "No nand dma resource 1\n");
		goto nand_dma_init_error2;
	}
	nand_dma->chan_type =regs->start;
	dma_cap_zero(mask);
	dma_cap_set(DMA_SLAVE, mask);
	nand_dma->mcu_chan = dma_request_channel(mask, filter, nand_dma);
	if(nand_dma->mcu_chan < 0) {
		printk("Failed: request mcu dma chan\n");
		goto nand_dma_init_error2;
	}
	/* request data channel,which be used for memcpy */
	regs = platform_get_resource(pdev,IORESOURCE_DMA,1);
	if (!regs) {
		dev_err(&pdev->dev, "No nand dma resource 2\n");
		goto nand_dma_init_error3;
	}
	nand_dma->chan_type =regs->start;
	dma_cap_zero(mask);
	dma_cap_set(DMA_MEMCPY, mask);
	nand_dma->data_chan = dma_request_channel(mask, filter, nand_dma);
	if(nand_dma->data_chan < 0) {
		printk("Failed: request data dma chan\n");
		goto nand_dma_init_error3;
	}
	/*  init sg of message channel */
	//nand_dma->msg =(struct pdma_msg *)nand_malloc_buf(sizeof(struct pdma_msg));
	nand_dma->msg = (struct pdma_msg *)dma_alloc_coherent(nand_dma->mcu_chan->device->dev,
			sizeof(struct pdma_msg),&nand_dma->msg_phyaddr, GFP_ATOMIC);
	if(!(nand_dma->msg)){
		printk("Failed: nand_dma->msg malloc failed\n");
		goto nand_dma_init_error4;
	}
	//sg_init_one(&(nand_dma->sg),nand_dma->msg,sizeof(struct pdma_msg));


	nand_dma->data_buf_len = pnand_api->nand_chip->pagesize;
	nand_dma->data_buf = (unsigned char *)dma_alloc_coherent(nand_dma->data_chan->device->dev,
			nand_dma->data_buf_len,&nand_dma->data_buf_phyaddr, GFP_ATOMIC);
	if(!(nand_dma->data_buf)) {
		printk("Failed: nand_dma_init data buf malloc failed !\n");
		goto nand_dma_init_error5;
	}
	/*
	   nand_dma->dma_config.direction = DMA_TO_DEVICE;
	   nand_dma->dma_config.dst_addr = CPHYSADDR(PDMA_MSG_TCSMVA);
	   nand_dma->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	   nand_dma->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	   nand_dma->dma_config.src_maxburst = 0;
	   nand_dma->dma_config.dst_maxburst = 0;
	   ret = dmaengine_slave_config(nand_dma->mcu_chan, &nand_dma->dma_config);
	   if(ret < 0){
	   printk("Failed: mcu chan config\n");
	   goto nand_dma_init_error6;
	   }

	   nand_dma->dma_config.direction = 0;
	   nand_dma->dma_config.src_addr = 0;
	   nand_dma->dma_config.dst_addr = nand_dma->data_buf_phyaddr;
	   nand_dma->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	   nand_dma->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	   nand_dma->dma_config.src_maxburst = 1;
	   nand_dma->dma_config.dst_maxburst = 1;
	   ret = dmaengine_slave_config(nand_dma->data_chan, &nand_dma->dma_config);
	   if(ret < 0){
	   kfree(nand_dma->sgl);
	   kfree(nand_dma->buf);
	   printk("Failed: mcu chan config\n");
	   return -1;
	   }
	 */

	init_completion(&comp);

	pnand_api->nand_dma =(void *)nand_dma;
	printk("@@@ %s is finish ! @@@\n",__func__);
	return ret;
	/*
nand_dma_init_error6:
dma_free_coherent(nand_dma->data_chan->device->dev, nand_dma->data_buf_len,
nand_dma->data_buf, nand_dma->data_buf_phyaddr);
	 */
nand_dma_init_error5:
	nand_free_buf(nand_dma->msg);
nand_dma_init_error4:
	dma_release_channel(nand_dma->data_chan);
nand_dma_init_error3:
	dma_release_channel(nand_dma->mcu_chan);
nand_dma_init_error2:
	nand_free_buf(nand_dma);
nand_dma_init_error1:
	return -1;
}

void nand_dma_deinit(struct jznand_dma *nand_dma)
{
	if(nand_dma->mcu_chan) {
		dmaengine_terminate_all(nand_dma->mcu_chan);
	}
	dma_release_channel(nand_dma->mcu_chan);
	if(nand_dma->data_chan) {
		dmaengine_terminate_all(nand_dma->data_chan);
	}
	dma_release_channel(nand_dma->data_chan);
	dma_free_coherent(nand_dma->data_chan->device->dev, nand_dma->data_buf_len,
			nand_dma->data_buf, nand_dma->data_buf_phyaddr);
	dma_free_coherent(nand_dma->mcu_chan->device->dev, sizeof(struct pdma_msg),
			nand_dma->msg, nand_dma->msg_phyaddr);
	nand_free_buf(nand_dma);
}
