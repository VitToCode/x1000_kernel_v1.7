/*
 * lib_nand/nand_api.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) Ingenic Semiconductor Co., Ltd.
 */

#ifndef __NAND_API_H__
#define __NAND_API_H__

//#include <asm/jzsoc.h>
//#include "inc/jz4780_nand_def.h"
//#include "inc/pisces-nand.h"
#include "jz4780_nand_def.h"
#include <mach/jznand.h>
/*
 * jz debug
 */
#define JZ_DEBUG        1
#if JZ_DEBUG
/* In Linux Kernel */
#ifdef __KERNEL__
#define	dprintf(n,x...) printk(n,##x)
#define	eprintf(n,x...) printk(n,##x)
#else
/* In userspace or other platform */
#define dprintf(n, x...) printf(n, ##x)
#define	eprintf(n, x...) printf(1, ##x)
#endif
#else
#define	dprintf(n,x...)
#define	eprintf(n,x...)
#endif

#define dbg_ptr(ptr)	\
	dprintf("==>%s L%d: addr of " #ptr " = 0x%08x\n",	\
		__func__, __LINE__, (u32)(ptr))

#define dbg_line()	\
	dprintf("==>%s L%d\n", __func__, __LINE__)

#include "jz_nemc.h"
#include "nand_io.h"
#include "nand_ecc.h"

#include "nand_chip.h"
#ifdef CONFIG_MTD_NAND_DMA
#include "nand_dma.h"
#endif
#include "pagelist.h"
#include "blocklist.h"
#include "ppartition.h"

/*
 * NAND dump info
 */
/* In Linux Kernel */
#ifdef __KERNEL__
#include <linux/slab.h>
#endif
            
#define SUCCESS 0
#define ENAND -1        // common error      
#define DMA_AR -2
#define IO_ERROR -3     // nand status error
#define TIMEOUT -4
#define ECC_ERROR  -5   //uncorrectable ecc errror
#define ALL_FF -6       // bch data and parity  are 0xff

#define nand_malloc_buf(len)  kmalloc((len)*sizeof(unsigned char),GFP_KERNEL)  
#define nand_free_buf(p)      kfree(p)

/*   nand spl macro */
#define SPL_BCH_BLOCK 256
#define SPL_BCH_SIZE 112
#define SPL_BCH_BIT 64


typedef struct alignedlist{
	PageList *pagelist;
	struct alignedlist *next;
	unsigned int opsmodel;      // 31 ~ 24 : operation mode (one plane  or two plane) ; 23 ~ 0  :the number of pagelist 
}Aligned_List;

/*  nand wait rb */
int nand_wait_rb(void);

void dump_buf(u8 *buf, int len);
void init_buf(u8 *buf, int len);

/*nand debug funcs in test_nand.c*/
int buf_check(unsigned char *org, unsigned char *obj, int size);
int checkFF(unsigned char *org, int size);
void dump_data(unsigned char *buf);
void buf_init(unsigned char *buf, int len);
unsigned int *malloc_buf(void);
/* NAND Chip operations */
 
int nand_read_page(NAND_BASE *host,unsigned int pageid, unsigned int offset,unsigned int bytes,void * databuf);
int nand_read_pages(NAND_BASE *host,Aligned_List *aligned_list);
 
int nand_write_page(NAND_BASE *host,unsigned int pageid, unsigned int offset,unsigned int bytes,void * databuf);
int nand_write_pages(NAND_BASE *host,Aligned_List *aligned_list);
 
int nand_erase_blocks(NAND_BASE *host,BlockList *headlist);
 
int isbadblock(NAND_BASE *host,unsigned int blockid);
int markbadblock(NAND_BASE *host,unsigned int blockid);
int read_spl(NAND_BASE *host, Aligned_List *list);
int write_spl(NAND_BASE *host, Aligned_List *list);
 
/* copyback ops */
//void nand_cb_page(unsigned char *buf, int len, int srcpage, int destpage);
//void nand_cb_planes(unsigned char *buf, int len, int srcpage, int destpage);
 
void nand_get_id(char *nand_id);
int nand_reset(void);
int send_read_status(unsigned char *status);
void do_nand_register(NAND_API *pnand_api);
void nand_ops_parameter_init(void);
void nand_ops_parameter_reset(const PPartition *ppt);
#endif /* __NAND_API_H__ */
