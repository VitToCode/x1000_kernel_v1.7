/* 
 * linux/drivers/misc/jz_x2d_core.c -- Ingenic EXtreme 2D driver
 *
 * Copyright (C) 2005-2012, Ingenic Semiconductor Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
//////////////////////////////////include files/////////////////////////////////////////////////////
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
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
#include <linux/module.h>
//#include <linux/time.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/major.h>
#include <linux/version.h>
#include <linux/io.h>
#include <linux/memory.h>
#include <linux/earlysuspend.h>
#include <linux/dma-mapping.h>

#include <asm/cacheflush.h>
#include <asm/mipsregs.h>
#include <asm/mipsmtregs.h>
#include <asm/pgtable.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/thread_info.h>

#include "jz_x2d.h"
#include "jz_x2d_reg.h"

//////////////////////////////////global parameters//////////////////////////////////////////////////
#define X2D_NAME        "x2d"

struct x2d_device{
	int irq;
	void __iomem *base;
	struct device *dev;
	struct resource * mem;
	struct miscdevice misc_dev;
	struct clk *x2d_clk;
	enum jz_x2d_state state;
	enum jz_x2d_errcode errcode;
	struct list_head proc_list;
	int proc_num;
	pid_t hold_proc;
	x2d_chain_info * chain_p;
	wait_queue_head_t set_wait_queue;
	struct  early_suspend early_suspend;	
	struct mutex x2d_lock;
	struct mutex compose_lock;
};

//////////////////////////////function decalre////////////////////////////////////////////////////
static int x2d_open(struct inode *inode, struct file *filp);
static int x2d_release(struct inode *inode, struct file *filp);
static ssize_t x2d_read(struct file *filp, char *buf, size_t size, loff_t *l);
static ssize_t x2d_write(struct file *filp, const char *buf, size_t size, loff_t *l);
static long x2d_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
//static int x2d_mmap(struct file *file, struct vm_area_struct *vma);
/////////////////////////////////internal interface/////////////////////////////////////////////////
static   unsigned long reg_read(struct x2d_device *jz_x2d,int offset)
{
	return readl(jz_x2d->base + offset); 
}

static   void reg_write(struct x2d_device *jz_x2d,int offset, unsigned long val)
{
	writel(val, jz_x2d->base + offset); 
}

static void bit_set(struct x2d_device *jz_x2d,int offset,int bit)
{
	int val = 0;
	val = reg_read(jz_x2d,offset);
	val |= (1<<bit);
	reg_write(jz_x2d,offset,val);
}
#if 0
static void bit_clr(struct x2d_device *jz_x2d,int offset,int bit)
{
	int val = 0;
	val = reg_read(jz_x2d,offset);
	val &= ~(1<<bit);
	reg_write(jz_x2d,offset,val);
}
#endif
static int get_srcfmt_bpp(int format)
{
	switch(format)
	{
		case X2D_INFORMAT_ARGB888:
			return 32;
		case X2D_INFORMAT_RGB555:
		case X2D_INFORMAT_RGB565:
			return 16;
		case X2D_INFORMAT_YUV420SP:
		case X2D_INFORMAT_TILE420:
		case X2D_INFORMAT_NV21:
		case X2D_INFORMAT_NV12:
			return 12;
		default:
			pr_err("Error!!unknow src format %d \n",format);
			return -1;
	}
}
static int get_dstfmt_bpp(int format)
{
	switch(format)
	{
		case X2D_OUTFORMAT_ARGB888:
		case X2D_OUTFORMAT_XARGB888:
			return 32;
		case X2D_OUTFORMAT_RGB565:
		case X2D_OUTFORMAT_RGB555:
			return 16;
		default:
			pr_err("Error!!unknow dst format %d\n",format);
			return -1;
	}
}
#if 0
int x2d_get_state(void)
{
	return jz_x2d->state;
}

int x2d_get_errcode(void)
{
	return jz_x2d->errcode;
}
#endif
static unsigned int get_phy_addr(unsigned int vaddr)  
{  
	unsigned int addr=vaddr & (PAGE_SIZE-1);
	pgd_t   *pgdir;
#ifdef CONFIG_PGTABLE_4
	pud_t	*pudir;
#endif
	pmd_t   *pmdir; 
	pte_t   *pte;  

	//printk("current task(%d)'s pgd is 0x%p\n",current->pid,current->mm->pgd);

	pgdir=pgd_offset(current->mm,vaddr);  
	if(pgd_none(*pgdir)||pgd_bad(*pgdir))  
		return   -EINVAL;  

#ifdef CONFIG_PGTABLE_4
	pudir=pud_offset(pgdir,vaddr);
	if(pud_none(*pudir)||pud_bad(*pudir))  
		return   -EINVAL;  

	pmdir=pmd_offset(pudir,vaddr);  
	if(pmd_none(*pmdir)||pmd_bad(*pmdir))  
		return   -EINVAL;  
#else
	pmdir=pmd_offset((pud_t *)pgdir,vaddr);  
	if(pmd_none(*pmdir)||pmd_bad(*pmdir))  
		return   -EINVAL;  
#endif

	pte=pte_offset(pmdir,vaddr);  

	if(pte_present(*pte))  
	{  
		return   addr | (pte_pfn(*pte)<<PAGE_SHIFT);  
		//pte_page(*pte);
	} 
	return   -EINVAL;  
}

/////////////////////////////////////tlb interface////////////////////////////////////////////////
int if_recorded_in_list(int vaddr,struct x2d_process_info* proc)
{
	int i =0;
	int find_flag = 0;
	for(i=0;i< proc->record_addr_num;i++)
	{
		if(vaddr == proc->record_addr_list[i]&& 1/*valid page*/)
		{
			find_flag = 1;
			break;
		}
	}
	return find_flag;
}
#define X2D_PAGE_SIZE 0x1000
#define X2D_PAGE_VALID_BIT 0x1//4	
void fill_tlb_address(int vaddr,int lenth,struct x2d_process_info* proc)
{

	int page_num,i,addr,paddr,tlb_pos,s_pos;
	if(if_recorded_in_list(vaddr,proc))
		return;
	
	proc->record_addr_list[proc->record_addr_num] = vaddr;
	proc->record_addr_num++;

	addr = (vaddr >> 12)<<12;
	page_num = ((vaddr - addr) + lenth + X2D_PAGE_SIZE - 1) >> 12;
	tlb_pos = (vaddr >> 12 ) << 2;

	s_pos = tlb_pos;

	for(i=0;i<page_num;i++)
	{
		int *p_tlb;
		//paddr = page_to_phys(vmalloc_to_page(addr));
		paddr = get_phy_addr(addr);
		p_tlb = (int *)(proc->tlb_base + tlb_pos);
		*p_tlb = paddr + X2D_PAGE_VALID_BIT;
		tlb_pos+=4;
		addr += X2D_PAGE_SIZE;
	}
	dma_cache_wback(proc->tlb_base + s_pos,page_num * 4);
	dma_cache_wback(vaddr,lenth);
}
void transfer_config_address(struct x2d_process_info* proc)
{
	int i = 0;
	int lenth = 0;
	/////////dst address transfer	
	lenth = proc->configs.dst_width * proc->configs.dst_height 
		* get_dstfmt_bpp(proc->configs.dst_format)/8;
	fill_tlb_address(proc->configs.dst_address, lenth,  proc);

	/////////src address transfer
	for(i=0;i< proc->configs.layer_num ;i++)
	{	
		lenth = proc->configs.lay[i].in_width * proc->configs.lay[i].in_height 
			* get_srcfmt_bpp(proc->configs.lay[i].format)/8;
		fill_tlb_address(proc->configs.lay[i].addr, lenth,  proc);
	}
}

////////////////////////////////process functions/////////////////////////////////////////////////
static struct x2d_process_info* x2d_index_procinfo(struct x2d_device *jz_x2d,pid_t pid)
{
	struct x2d_process_info* p ;
	list_for_each_entry(p, &jz_x2d->proc_list, list)
	{
		if(p->pid == pid)
			return p;
	}
	dev_err(jz_x2d->dev,"cannot find the proc %d from chain\n",pid);
	return NULL;
}
#define X2D_TLB_TABLE_SIZE 0x00200000   // 2M
#define X2D_MAX_ADDR_RECORD_NUM 1024
#define DUMMY_TLB_ADDR 0x8f000001
static int create_tlb_table(struct x2d_process_info* proc)
{
	int *tlb_p = NULL;
	int *addr_list = NULL;
	int i = 0;
	tlb_p = kzalloc(X2D_TLB_TABLE_SIZE,GFP_KERNEL);//alloc 2M tlb mem;

	if(!tlb_p)
	{
		pr_err("malloc tlb table fail\n");
		return -1;
	}
	for(i=0;i<(X2D_TLB_TABLE_SIZE>>2);i++)//avoid x2d  hardware bug,must do it!
		*(tlb_p + i) = DUMMY_TLB_ADDR;

	addr_list = kzalloc(GFP_KERNEL,X2D_MAX_ADDR_RECORD_NUM*sizeof(int));
	if(!addr_list)
	{
		pr_err("malloc addr list fail\n");
		return -1;
	}

	proc->tlb_base = (int)tlb_p;
	proc->record_addr_list = addr_list;
	proc->record_addr_num = 0;

	return 0;
}
static int free_tlb_table(struct x2d_process_info* proc)
{
	kzfree((int *)proc->tlb_base);
	kzfree(proc->record_addr_list);
	proc->tlb_base = 0;
	proc->record_addr_list = NULL;
	return 0;
}

static int x2d_create_procinfo(struct x2d_device *jz_x2d)
{
	int ret = 0;
	struct x2d_process_info* p = NULL;
	p = kmalloc(sizeof(struct x2d_process_info),GFP_KERNEL);
	if(p == NULL)
	{
		dev_err(jz_x2d->dev,"malloc for struct proc_info fail\n");
		return -1;
	}
	p->pid = current->pid;
	list_add_tail(&p->list,&jz_x2d->proc_list);
	ret = create_tlb_table(p);
	if(ret != 0)
	{
		dev_err(jz_x2d->dev,"creat tlb for proc %d  fail\n",p->pid);
		return -1;
	}
	jz_x2d->proc_num++;
	dev_info(jz_x2d->dev,"X2d has opened by %d processes\n",jz_x2d->proc_num);
	return 0;
}

static int x2d_free_procinfo(struct x2d_device *jz_x2d,pid_t pid)
{
	struct x2d_process_info* p = NULL;
	p = x2d_index_procinfo(jz_x2d,current->pid);
	if(!p)
	{
		dev_err(jz_x2d->dev,"free_tlb_table  cannot find proc %d\n",pid);
		return -1;
	}
	free_tlb_table(p);
	list_del(&p->list);
	kfree(p);
	p = NULL;
	jz_x2d->proc_num--;
	dev_info(jz_x2d->dev,"A process is freed,now x2d has %d process\n",jz_x2d->proc_num);
	return 0;
}

static int x2d_check_allproc_free (struct x2d_device *jz_x2d)
{
	int ret;
	ret = (jz_x2d->proc_num==0)? 1: 0;
	if(ret == 1)dev_info(jz_x2d->dev,"X2d ---no proc used!\n");
	return ret;
}

static void x2d_dump_reg(struct x2d_device *jz_x2d,struct x2d_process_info* p)
{
	static int i = 0;
	int j =0;
	//if(i%10 == 0)
	{	
		dev_info(jz_x2d->dev,"pid is %d   current is %d\n",p->pid,current->pid);
		dev_info(jz_x2d->dev,"REG_X2D_GLB_CTRL %lx\n",reg_read(jz_x2d,REG_X2D_GLB_CTRL));
		dev_info(jz_x2d->dev,"REG_X2D_GLB_STATUS %lx\n",reg_read(jz_x2d,REG_X2D_GLB_STATUS));
		dev_info(jz_x2d->dev,"REG_X2D_GLB_TRIG %lx\n",reg_read(jz_x2d,REG_X2D_GLB_TRIG));
		dev_info(jz_x2d->dev,"REG_X2D_DHA %lx\n",reg_read(jz_x2d,REG_X2D_DHA));
		dev_info(jz_x2d->dev,"REG_X2D_TLB_BASE %lx\n",reg_read(jz_x2d,REG_X2D_TLB_BASE));
		dev_info(jz_x2d->dev,"REG_X2D_WDOG_CNT %lx\n",reg_read(jz_x2d,REG_X2D_WDOG_CNT));
		dev_info(jz_x2d->dev,"REG_X2D_LAY_GCTRL %lx\n",reg_read(jz_x2d,REG_X2D_LAY_GCTRL));
		dev_info(jz_x2d->dev,"REG_X2D_DST_BASE %lx\n",reg_read(jz_x2d,REG_X2D_DST_BASE));
		dev_info(jz_x2d->dev,"REG_X2D_DST_CTRL_STR %lx\n",reg_read(jz_x2d,REG_X2D_DST_CTRL_STR));
		dev_info(jz_x2d->dev,"REG_X2D_DST_GS %lx\n",reg_read(jz_x2d,REG_X2D_DST_GS));
		dev_info(jz_x2d->dev,"REG_X2D_DST_MSK_ARGB %lx\n",reg_read(jz_x2d,REG_X2D_DST_MSK_ARGB));
		dev_info(jz_x2d->dev,"REG_X2D_DST_FMT %lx\n",reg_read(jz_x2d,REG_X2D_DST_FMT));
		for(j=0;j<4;j++)
		{
			dev_info(jz_x2d->dev,"REG_X2D_LAY%d_CTRL  %lx\n",j,reg_read(jz_x2d,REG_X2D_LAY0_CTRL + j*0x1000));
			dev_info(jz_x2d->dev,"REG_X2D_LAY%d_Y_ADDR  %lx\n",j,reg_read(jz_x2d,REG_X2D_LAY0_Y_ADDR+ j*0x1000));
			dev_info(jz_x2d->dev,"REG_X2D_LAY%d_U_ADDR  %lx\n",j,reg_read(jz_x2d,REG_X2D_LAY0_U_ADDR+ j*0x1000));
			dev_info(jz_x2d->dev,"REG_X2D_LAY%d_V_ADDR  %lx\n",j,reg_read(jz_x2d,REG_X2D_LAY0_V_ADDR+ j*0x1000));
			dev_info(jz_x2d->dev,"REG_X2D_LAY%d_IN_FM_GS %lx\n",j,reg_read(jz_x2d,REG_X2D_LAY0_IN_FM_GS+ j*0x1000));
			dev_info(jz_x2d->dev,"REG_X2D_LAY%d_STRIDE  %lx\n",j,reg_read(jz_x2d,REG_X2D_LAY0_STRIDE+ j*0x1000));
			dev_info(jz_x2d->dev,"REG_X2D_LAY%d_OUT_GS  %lx\n",j,reg_read(jz_x2d,REG_X2D_LAY0_OUT_GS+ j*0x1000));
			dev_info(jz_x2d->dev,"REG_X2D_LAY%d_OOSFT  %lx\n",j,reg_read(jz_x2d,REG_X2D_LAY0_OOSFT+ j*0x1000));
			dev_info(jz_x2d->dev,"REG_X2D_LAY%d_RSZ_COEF  %lx\n",j,reg_read(jz_x2d,REG_X2D_LAY0_RSZ_COEF+ j*0x1000));
			dev_info(jz_x2d->dev,"REG_X2D_LAY%d_BK_ARGB %lx\n",j,reg_read(jz_x2d,REG_X2D_LAY0_BK_ARGB+ j*0x1000));	
		}
	}
	i++;
}
/////////////////////////////main function///////////////////////////////////////////////////////
#define JZ4780_X2D_WTHDOG_1S 0xa0000000
#define X2D_LAYER_OFFSET 0x1000

int jz_x2d_start_compose(struct x2d_device *jz_x2d)
{
	struct x2d_process_info * p;
	int i = 0;
	mutex_lock(&jz_x2d->compose_lock);

	jz_x2d->state = x2d_state_calc;
	p = x2d_index_procinfo(jz_x2d,current->pid);
	jz_x2d->hold_proc = current->pid;

	__x2d_reset_trig();
	//udelay(1);
	__x2d_setup_default();
	__x2d_enable_dma();

	reg_write(jz_x2d,REG_X2D_TLB_BASE,(unsigned long)virt_to_phys((void *)p->tlb_base));
	reg_write(jz_x2d,REG_X2D_WDOG_CNT,JZ4780_X2D_WTHDOG_1S);
	reg_write(jz_x2d,REG_X2D_LAY_GCTRL,p->configs.layer_num);
	reg_write(jz_x2d,REG_X2D_DHA,virt_to_phys(jz_x2d->chain_p));

	memset(jz_x2d->chain_p,0,sizeof(x2d_chain_info));
	jz_x2d->chain_p->dst_addr = p->configs.dst_address;
	jz_x2d->chain_p->dst_ctrl_str = ((p->configs.dst_stride *get_dstfmt_bpp(p->configs.dst_format)/8)<< BIT_X2D_DST_STRIDE) \
					|(p->configs.dst_back_en << BIT_X2D_DST_BG_EN) \
					|(p->configs.dst_glb_alpha_en << BIT_X2D_DST_GLB_ALPHA_EN)\
					|(p->configs.dst_preRGB_en << BIT_X2D_DST_PREM_EN)\
					|(p->configs.dst_backpure_en << BIT_X2D_DST_MSK_EN)\
					|(p->configs.dst_alpha_val << BIT_X2D_DST_GLB_ALPHA_VAL);		
	jz_x2d->chain_p->dst_height = (uint16_t)p->configs.dst_height;
	jz_x2d->chain_p->dst_width = (uint16_t)p->configs.dst_width;	
	jz_x2d->chain_p->overlay_num = p->configs.layer_num;
	jz_x2d->chain_p->dst_tile_en  = 0;
	jz_x2d->chain_p->dst_fmt = (X2D_ALPHA_POSHIGH << BIT_X2D_DST_ALPHA_POS)\
				   |(p->configs.dst_format << BIT_X2D_DST_RGB_FORMAT)\
				   |(X2D_RGBORDER_RGB << BIT_X2D_DST_RGB_ORDER);	 
	jz_x2d->chain_p->dst_argb =  p->configs.dst_bcground;
	for(i=0;i<p->configs.layer_num;i++)
	{

		jz_x2d->chain_p->x2d_lays[i].lay_ctrl =(p->configs.lay[i].glb_alpha_en << BIT_X2D_LAY_GLB_ALPHA_EN)\
						       |(p->configs.lay[i].mask_en << BIT_X2D_LAY_MSK_EN)\
						       |((p->configs.lay[i].format > X2D_INFORMAT_RGB565) << BIT_X2D_LAY_CSCM_EN)\
						       |(p->configs.lay[i].preRGB_en << BIT_X2D_LAY_PREM_EN)\
						       |(p->configs.lay[i].format << BIT_X2D_LAY_INPUT_FORMAT);
		jz_x2d->chain_p->x2d_lays[i].lay_galpha =(uint8_t)p->configs.lay[i].global_alpha_val;
		jz_x2d->chain_p->x2d_lays[i].rom_ctrl = (uint8_t)p->configs.lay[i].transform;
		jz_x2d->chain_p->x2d_lays[i].RGBM =	0;//defualt
		jz_x2d->chain_p->x2d_lays[i].y_addr = (uint32_t)p->configs.lay[i].addr;
		jz_x2d->chain_p->x2d_lays[i].v_addr = (uint32_t)p->configs.lay[i].v_addr;
		jz_x2d->chain_p->x2d_lays[i].u_addr = (uint32_t)p->configs.lay[i].u_addr;
		jz_x2d->chain_p->x2d_lays[i].swidth = (uint16_t)p->configs.lay[i].in_width;
		jz_x2d->chain_p->x2d_lays[i].sheight = (uint16_t)p->configs.lay[i].in_height;
		jz_x2d->chain_p->x2d_lays[i].ystr = (uint16_t)p->configs.lay[i].y_stride *
											get_srcfmt_bpp(p->configs.lay[i].format)/8;
		jz_x2d->chain_p->x2d_lays[i].uvstr = (uint16_t)p->configs.lay[i].v_stride;
		jz_x2d->chain_p->x2d_lays[i].owidth = (uint16_t)p->configs.lay[i].out_width;
		jz_x2d->chain_p->x2d_lays[i].oheight= (uint16_t)p->configs.lay[i].out_height;
		jz_x2d->chain_p->x2d_lays[i].oxoffset= (uint16_t)p->configs.lay[i].out_w_offset;
		jz_x2d->chain_p->x2d_lays[i].oyoffset= (uint16_t)p->configs.lay[i].out_h_offset;
		jz_x2d->chain_p->x2d_lays[i].rsz_hcoef = (uint16_t)p->configs.lay[i].h_scale_ratio;
		jz_x2d->chain_p->x2d_lays[i].rsz_vcoef = (uint16_t)p->configs.lay[i].v_scale_ratio;
		jz_x2d->chain_p->x2d_lays[i].bk_argb = p->configs.lay[i].msk_val;

	}	
	dma_cache_wback((unsigned long)jz_x2d->chain_p,sizeof(x2d_chain_info));

	__x2d_enable_wthdog();
	__x2d_enable_irq();
	__x2d_start_trig();

	if(!interruptible_sleep_on_timeout(&jz_x2d->set_wait_queue,100*HZ))
	{
		__x2d_stop_trig();
		x2d_dump_reg(jz_x2d,p);
		mutex_unlock(&jz_x2d->compose_lock);
		dev_info(jz_x2d->dev,"wait queue time out  %lx\n",reg_read(jz_x2d,REG_X2D_GLB_STATUS));
		return -1;   
	}

	mutex_unlock(&jz_x2d->compose_lock);
	return 0;
}
int jz_x2d_set_config(struct x2d_device *jz_x2d,struct jz_x2d_config* config)
{
	struct x2d_process_info * p;
	mutex_lock(&jz_x2d->x2d_lock);
	p = x2d_index_procinfo(jz_x2d,current->pid);
	if (copy_from_user(&p->configs, (void *)config, sizeof(struct jz_x2d_config)))
		return -EFAULT;

	transfer_config_address(p);
	mutex_unlock(&jz_x2d->x2d_lock);
	return 0;
}

int jz_x2d_get_proc_config(struct x2d_device *jz_x2d,struct jz_x2d_config* config)
{	
	struct x2d_process_info * p;
	p = x2d_index_procinfo( jz_x2d,current->pid);
	if (copy_to_user((void *)config, &p->configs, sizeof(struct jz_x2d_config)))
		return -EFAULT;
	return 0;
}

int jz_x2d_get_sysinfo(struct x2d_device *jz_x2d,struct jz_x2d_config* config)
{
	struct x2d_process_info * p;
	p = x2d_index_procinfo(jz_x2d,jz_x2d->hold_proc);
	if (copy_to_user((void *)config, &p->configs, sizeof(struct jz_x2d_config)))
		return -EFAULT;

	return 0;
}
int jz_x2d_stop_calc(struct x2d_device *jz_x2d)
{
	if(jz_x2d->hold_proc == current->pid)
	{	
		__x2d_stop_trig();
		dev_info(jz_x2d->dev,"proc %d stop x2d calculating\n",jz_x2d->hold_proc);
	}
	else
	{
		dev_err(jz_x2d->dev,"proc %d want to stop x2d ,hold proc is %d\n",current->pid,jz_x2d->hold_proc);
		return  -1;
	}
	return 0;
}

////////////////////////////////irq    handle /////////////////////////////////////////////////////
static irqreturn_t x2d_irq_handler(int irq, void *dev_id)
{
	struct x2d_device *jz_x2d = (struct x2d_device *)dev_id;
	unsigned long int status_reg = 0;
	status_reg = reg_read(jz_x2d,REG_X2D_GLB_STATUS);
	if(status_reg | X2D_WTDOG_ERR)
	{
		dev_info(jz_x2d->dev,"Error:x2d watch dog time out!!!!");
		jz_x2d->errcode = error_wthdog;
	}
	else if(!(status_reg | X2D_BUSY))
	{
		jz_x2d->errcode = error_none;
	}

	if(jz_x2d->state == x2d_state_suspend)
	{
		__x2d_stop_trig();
		__x2d_clear_irq();
		clk_disable(jz_x2d->x2d_clk);
		wake_up_interruptible(&jz_x2d->set_wait_queue);
	}
	jz_x2d->state = x2d_state_complete;

	__x2d_clear_irq();
	wake_up_interruptible(&jz_x2d->set_wait_queue); 
	return IRQ_HANDLED;
}
//////////////////////////////suspend  resume///////////////////////////////////////////////////
static void x2d_early_suspend(struct early_suspend *handler)
{
	struct x2d_device *jz_x2d = container_of(handler, struct x2d_device, early_suspend);
	jz_x2d->state = x2d_state_suspend;
}
static void x2d_early_resume(struct early_suspend *handler)
{
	struct x2d_device *jz_x2d = container_of(handler, struct x2d_device, early_suspend);
	jz_x2d->state = x2d_state_idle;
	clk_enable(jz_x2d->x2d_clk);
}

/////////////////////////////sys call functions////////////////////////////////////////////////////

static inline struct x2d_device *file_to_x2d(struct file *file) 
{
	struct miscdevice *dev = file->private_data;
	return container_of(dev, struct x2d_device, misc_dev);
}
static int x2d_open(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct x2d_device *jz_x2d = NULL;
	jz_x2d = file_to_x2d(filp);
	mutex_lock(&jz_x2d->x2d_lock);
	//if(x2d_check_allproc_free())//first open dev
	//clk_enable(jz_x2d->x2d_clk);	
	ret = x2d_create_procinfo(jz_x2d);
	mutex_unlock(&jz_x2d->x2d_lock);
	return ret;
}
static int x2d_release(struct inode *inode, struct file *filp)
{
	int ret = 0;
	struct x2d_device *jz_x2d = NULL;
	jz_x2d = file_to_x2d(filp);
	mutex_lock(&jz_x2d->x2d_lock);
	ret = x2d_free_procinfo(jz_x2d,current->pid);
	if(x2d_check_allproc_free(jz_x2d))
		clk_disable(jz_x2d->x2d_clk );///////stop x2d hardware
	mutex_unlock(&jz_x2d->x2d_lock);
	return 0;
}

/**************************
 *     IOCTL Handlers     *
 **************************/
static long x2d_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	struct x2d_device *jz_x2d = NULL;
	jz_x2d = file_to_x2d(filp);
	//void __user *argp = (void __user *)arg;
	switch (cmd)
	{
		case IOCTL_X2D_SET_CONFIG:
			retval = jz_x2d_set_config(jz_x2d,(struct jz_x2d_config*)arg);
			break;
		case IOCTL_X2D_START_COMPOSE:
			retval = jz_x2d_start_compose(jz_x2d);
			break;
		case IOCTL_X2D_GET_MY_CONFIG:
			retval = jz_x2d_get_proc_config(jz_x2d,(void *)arg);
			break;
		case IOCTL_X2D_GET_SYSINFO:
			retval = jz_x2d_get_sysinfo(jz_x2d,(void *)arg);
			break;
		case IOCTL_X2D_STOP:
			retval = jz_x2d_stop_calc(jz_x2d);
			break;
		case IOCTL_X2D_MAP_GRAPHIC_BUF:
			//retval = jz_x2d_map_graphic_buf();
			break;
		case IOCTL_X2D_FREE_GRAPHIC_BUF:
			//retval = jz_x2d_free_graphic_buf();
			break;
		default:
			printk("Not supported command: 0x%x\n", cmd);
			return -EINVAL;
	}
	
	return retval;
}

static ssize_t x2d_read(struct file *filp, char *buf, size_t size, loff_t *l)
{
	return -1;
}

static ssize_t x2d_write(struct file *filp, const char *buf, size_t size, loff_t *l)
{
	return -1;
}

static struct file_operations x2d_fops = 
{
			open:			x2d_open,
			release:		x2d_release,
			read:			x2d_read,
			write:			x2d_write,
			unlocked_ioctl:	x2d_ioctl,
			//mmap:			x2d_mmap,
};

static int __devinit x2d_probe(struct platform_device *pdev)
{
	int ret = 0;
	unsigned short x2d_id = 0;
	struct resource *mem = NULL;
	struct x2d_device *jz_x2d = NULL;

	jz_x2d = kzalloc(sizeof(struct x2d_device),GFP_KERNEL);
	if(jz_x2d == NULL)
	{
		dev_err(&pdev->dev, "Error alloc jz_x2d no memory!\n");
		return -ENOMEM;
	}

	jz_x2d->misc_dev.minor     = MISC_DYNAMIC_MINOR;
	jz_x2d->misc_dev.name      = "x2d";
	jz_x2d->misc_dev.fops      = &x2d_fops;
	jz_x2d->dev = &pdev->dev;

	jz_x2d->chain_p = kzalloc(sizeof(x2d_chain_info), GFP_KERNEL);

	mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem) {
		dev_err(&pdev->dev, "Failed to get register memory resource\n");
		ret = -ENXIO;
		goto err_exit;
	}
	mem = request_mem_region(mem->start, resource_size(mem), pdev->name);
	if (!mem) {
		dev_err(&pdev->dev, "Failed to request register memory region\n");
		ret = -EBUSY;
		goto err_exit;
	}
	jz_x2d->mem = mem;
	jz_x2d->base = ioremap(mem->start, resource_size(mem));
	if (!jz_x2d->base) {
		dev_err(&pdev->dev, "Failed to ioremap register memory region\n");
		ret = -EBUSY;
		goto err_exit;
	}

	jz_x2d->x2d_clk = clk_get(&pdev->dev, "x2d");
	if (IS_ERR(jz_x2d->x2d_clk )) {
		ret = PTR_ERR(jz_x2d->x2d_clk );
		dev_err(&pdev->dev, "Failed to get x2d clock: %d\n", ret);
		goto err_exit;
	}

	//jz_x2d->platdev = pdev; 
	//clk_enable(jz_x2d->x2d_clk );

	x2d_id = __x2d_read_devid();
	if(x2d_id != X2D_ID)
	{
		dev_err(&pdev->dev, "invalid x2d ID 0x%x\n",x2d_id);
		ret = -EINVAL;
		goto err_exit;
	}
	jz_x2d->irq = platform_get_irq(pdev, 0);
	if (request_irq(jz_x2d->irq, x2d_irq_handler, IRQF_SHARED,pdev->name, jz_x2d)) {
		dev_err(&pdev->dev,  "request_irq return error, ret=%d\n", ret);
		dev_err(&pdev->dev,  "X2d could not get IRQ\n");
		ret = -EINVAL;
		goto err_exit;
	}

	dev_set_drvdata(&pdev->dev, jz_x2d);
	ret = misc_register(&jz_x2d->misc_dev);
	if (ret < 0) {
		goto err_exit;;
	}

	jz_x2d->state = x2d_state_idle;
	jz_x2d->errcode = error_none;
	INIT_LIST_HEAD(&jz_x2d->proc_list);
	jz_x2d->proc_num = 0;
	init_waitqueue_head(&jz_x2d->set_wait_queue);
	mutex_init(&jz_x2d->compose_lock);
	mutex_init(&jz_x2d->x2d_lock);
//#ifndef CONFIG_HAS_EARLYSUSPEND
	jz_x2d->early_suspend.suspend = x2d_early_suspend;
	jz_x2d->early_suspend.resume = x2d_early_resume;
	jz_x2d->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&jz_x2d->early_suspend);
//#endif

	//clk_disable(jz_x2d->x2d_clk );  
	dev_info(&pdev->dev, "Virtual Driver of JZ X2D registered\n");
	return 0;

err_exit:
	if (!IS_ERR(jz_x2d->x2d_clk )) clk_put(jz_x2d->x2d_clk);
	if(mem) release_mem_region(mem->start, resource_size(mem));
	if(jz_x2d->irq) free_irq(jz_x2d->irq, jz_x2d);
	if(jz_x2d->base) iounmap(jz_x2d->base);
	if(jz_x2d->chain_p) kzfree(jz_x2d->chain_p);
	if(jz_x2d) kzfree(jz_x2d);
	return ret;
}

static int __devexit x2d_remove(struct platform_device *pdev)
{
	struct x2d_device *jz_x2d = platform_get_drvdata(pdev);
	iounmap(jz_x2d->base);
	free_irq(jz_x2d->irq,jz_x2d);
	release_mem_region(jz_x2d->mem->start, resource_size(jz_x2d->mem));
	unregister_early_suspend(&jz_x2d->early_suspend);
	kzfree(jz_x2d->chain_p);
	kzfree(jz_x2d);
	misc_deregister(&jz_x2d->misc_dev);
	return 0;
}

static struct platform_driver x2d_driver = {
	.driver.name	= "x2d",
	.driver.owner	= THIS_MODULE,
	.probe		= x2d_probe,
	.remove		= x2d_remove,
};

static int __init x2d_init(void)
{
	return platform_driver_register(&x2d_driver);
}

static void __exit x2d_exit(void)
{
	platform_driver_unregister(&x2d_driver);
}

late_initcall(x2d_init);// x2d driver should not be inited before PMU driver inited.
module_exit(x2d_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("cheer_fu<cfu@ingenic.cn>");
MODULE_DESCRIPTION("SOC Jz478x EXtreme 2D Module Driver");

