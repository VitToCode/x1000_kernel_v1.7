/*
 *  lib_nand/core/raw_nand/nand_ops.c
 *
 * Jz NAND Driver
 *
 *  NAND IC ops and NAND ops
 */

#include "nand_api.h"
#include "nandcommand.h"
#include "blocklist.h"
#include "ppartition.h"
#include <mach/jznand.h>
#include "jz4780_nand_def.h"
static JZ_NAND_CHIP *g_pnand_chip;
static NAND_CTRL *g_pnand_ctrl;
static JZ_IO *g_pnand_io;
static JZ_ECC *g_pnand_ecc;
extern void dump_buf(unsigned char *databuf,int len);
void do_nand_register(NAND_API *pnand_api)
{
	g_pnand_chip = pnand_api->nand_chip;
	g_pnand_ctrl = pnand_api->nand_ctrl;
	g_pnand_io = pnand_api->nand_io;
	g_pnand_ecc = pnand_api->nand_ecc;
}

/****************************************************************/
static unsigned char *g_poobbuf;  // the pointer of storing oob buffer
static unsigned int g_freesize;    // the area size in the page which cache ecc code
static unsigned int g_writesize;   // pagesize-freesize
static unsigned int g_oobsize;     // physical page size
static unsigned int g_eccpos;      // the place of ecc in the oob
static unsigned int g_eccsize;     // data bytes per Ecc step
static unsigned int g_eccbytes;    // ECC bytes per step
static unsigned int g_pagesize;    // physical page size
static unsigned char g_sizeshift;   // If g_eccsize is a power of 2 then the shift is stored in g_sizeshift
static unsigned int g_eccbit;      // the number of ECC checked bits
static int g_startblock;           // the first blockid of one partition
static int g_startpage;            // the first pageid of one partition
static unsigned int g_pagecount;    // the page number of one partition
static struct platform_nand_partition *g_pnand_pt;    // nand partition info
/************************************************
 *   nand_ops_parameter_init                    
 *   initialize global parameter                
 *   for example:g_pagesize,g_freesize...   
 */    
void nand_ops_parameter_init(void)
{
	g_oobsize = g_pnand_chip->oobsize;
	g_eccpos = g_pnand_ecc->eccpos;
	g_eccsize = g_pnand_ecc->eccsize;
	g_pagesize = g_pnand_chip->pagesize;
	switch(g_eccsize){
		case 512:
			g_sizeshift =9;
			break;
		case 1024 :
			g_sizeshift =10;
			break;
		default :
			g_sizeshift =0;
			break;
	}
	//	dprintf("_______________________   ops\n");
}

void nand_ops_parameter_reset(const PPartition *ppt)
{
	g_pnand_pt = (struct platform_nand_partition *)ppt->prData;
	g_writesize = ppt->byteperpage;
	g_freesize = g_pagesize-g_writesize;
	g_eccbit = g_pnand_pt->eccbit;
	g_eccbytes = __bch_cale_eccbytes(g_eccbit);
	g_startblock =ppt->startblockID;
	g_startpage =ppt->startPage;
	g_pagecount = ppt->PageCount;
	g_poobbuf = (unsigned char *)g_pnand_ecc->get_ecc_code_buffer(g_oobsize+g_freesize); /*cacl by byte*/
	//	printk("********************  planmum = %d  ***************\n",g_pnand_chip->planenum);
}

/*
 * Standard NAND command ops
 */
/**
 * send_read_page - do nand read standard
 * @column:	column address for NAND
 * @row:	row address for NAND
 */
static inline void send_read_page(int column, unsigned int row)
{
	int cycles = g_pnand_chip->row_cycles;

	g_pnand_io->send_cmd_norb(CMD_READ0);
	g_pnand_io->send_addr(column, (int)row, cycles);
	g_pnand_io->send_cmd_norb(CMD_READ_CONFIRM);
}

/**
 * send_read_random -
 */
static inline void send_read_random(int column)
{
	g_pnand_io->send_cmd_norb(CMD_RANDOM_READ);
	g_pnand_io->send_addr(column, -1, 0);
	g_pnand_io->send_cmd_norb(CMD_RANDOM_READ_CONFIRM);
}
static inline void send_read_2p_pageaddr(unsigned int first_row,unsigned int second_row)
{
	int cycles = g_pnand_chip->row_cycles;

	g_pnand_io->send_cmd_norb(CMD_2P_READ_START1);
	g_pnand_io->send_addr(-1, 0, cycles);
	g_pnand_io->send_cmd_norb(CMD_2P_READ_START1);
	g_pnand_io->send_addr(-1, (second_row | 0x080 ) , cycles);
	g_pnand_io->send_cmd_norb(CMD_2P_READ_CONFIRM1);
}
static inline int send_read_2p_random_output_withrb(int column,unsigned int row)
{
	int cycles = g_pnand_chip->row_cycles;
	int ret=0;
	ret =g_pnand_io->send_cmd_withrb(CMD_2P_READ_START2);
	if(ret <0)
		return ret;
	g_pnand_io->send_addr(0, 0, cycles);
	g_pnand_io->send_cmd_norb(CMD_2P_READ_RANDOM_OUTPUT);
	g_pnand_io->send_addr(column, -1, 0);
	g_pnand_io->send_cmd_norb(CMD_2P_READ_RANDOM_CONFIRM);
	return 0;
}

static inline void send_read_2p_random_output_norb(int column,unsigned int row)
{
	int cycles = g_pnand_chip->row_cycles;

	g_pnand_io->send_cmd_norb(CMD_2P_READ_START2);
	g_pnand_io->send_addr(0, 0x80, cycles);
	g_pnand_io->send_cmd_norb(CMD_2P_READ_RANDOM_OUTPUT);
	g_pnand_io->send_addr(column, -1, 0);
	g_pnand_io->send_cmd_norb(CMD_2P_READ_RANDOM_CONFIRM);
}

/**
 * send_prog_page - do nand programm standard
 * @column:	column address for NAND
 * @row:	row address for NAND
 */
static inline void send_prog_page(int column, unsigned int row)
{
	int cycles = g_pnand_chip->row_cycles;

	g_pnand_io->send_cmd_norb(CMD_WRITE);
	g_pnand_io->send_addr(column, (int)row, cycles);
}

/**
 * send_prog_confirm - do nand programm standard confirm
 */
static inline void send_prog_confirm(void)
{
	g_pnand_io->send_cmd_norb(CMD_WRITE_CONFIRM);
}

static inline void send_prog_random(int column)
{
	g_pnand_io->send_cmd_norb(CMD_RANDOM_WRITE);
	g_pnand_io->send_addr(column, -1, 0);
}

static inline void send_prog_random_confirm(void)
{
	g_pnand_io->send_cmd_norb(CMD_RANDOM_WRITE_CONFIRM);
}

/**
 * send_prog_2p_page1 - do nand two-plane programm first write cmd and addr
 * @column:	column address for NAND
 * @row:	row address for NAND
 */
static inline void send_prog_2p_page1(int column, unsigned int row)
{
	int cycles = g_pnand_chip->row_cycles;

	g_pnand_io->send_cmd_norb(CMD_2P_PROGRAM_START1);
	g_pnand_io->send_addr(0, 0, cycles);
	//	g_pnand_io->send_addr(column, (int)row, cycles);
}

/**
 * send_prog_2p_confirm1 - do nand two-plane programm first confirm
 */
static inline void send_prog_2p_confirm1(void)
{
	g_pnand_io->send_cmd_norb(CMD_2P_PROGRAM_CONFIRM1);
	nand_wait_rb();
}

/**
 * send_prog_2p_page2 - do nand two-plane programm first write cmd and addr
 * @column:	column address for NAND
 * @row:	row address for NAND
 */
static inline void send_prog_2p_page2(int column, unsigned int row)
{
	int cycles = g_pnand_chip->row_cycles;

	g_pnand_io->send_cmd_norb(CMD_2P_PROGRAM_START2);
	g_pnand_io->send_addr(column, (int)(row | 0x80) , cycles);
	//	g_pnand_io->send_addr(column, (int)row, cycles);
}

/**
 * send_prog_2p_confirm2 - do nand two-plane programm second confirm
 */
static inline void send_prog_2p_confirm2(void)
{
	g_pnand_io->send_cmd_norb(CMD_2P_PROGRAM_CONFIRM2);
}

/**
 * send_erase_block - do nand single erase 
 * @row:	row address for NAND
 */
static inline void send_erase_block(unsigned int row)
{
	int cycles = g_pnand_chip->row_cycles;

	g_pnand_io->send_cmd_norb(CMD_ERASE);
	g_pnand_io->send_addr(-1, (int)row, cycles);
	g_pnand_io->send_cmd_norb(CMD_ERASE_CONFIRM);
}

/**
 * send_erase_block - do nand single erase 
 * @row:	row address for NAND
 */
static inline void send_erase_2p_block1(unsigned int row)
{
	int cycles = g_pnand_chip->row_cycles;

	g_pnand_io->send_cmd_norb(CMD_2P_ERASE_START1);
	g_pnand_io->send_addr(-1, 0, cycles);
}

/**
 * send_erase_block - do nand single erase 
 * @row:	row address for NAND
 */
static inline void send_erase_2p_block2(unsigned int row)
{
	int cycles = g_pnand_chip->row_cycles;

	g_pnand_io->send_cmd_norb(CMD_2P_ERASE_START2);
	g_pnand_io->send_addr(-1, (int)((row | 0x80) & ~0x7f) , cycles);
	g_pnand_io->send_cmd_norb(CMD_2P_ERASE_CONFIRM);
}

/**
 * send_read_status - read nand status
 * @status:	state for NAND status
 */
int send_read_status(unsigned char *status)
{
	int ret =0;
	ret =g_pnand_io->send_cmd_withrb(CMD_READSTATUS);
	if(ret)
		return ret;  // nand io_error
	g_pnand_io->read_data_norb(status, 1);
	return 0;
}

/**
 * send_get_nand_id
 * @nand_id: a buffer to save nand id, at least 5 bytes size
 */
static inline void send_get_nand_id(char *nand_id)
{
	g_pnand_io->send_cmd_norb(CMD_READID);
	g_pnand_io->send_addr(-1, 0x00, 1);

	/* Read manufacturer and device IDs */	
	g_pnand_io->read_data_norb(&nand_id[0], 5);
}

void nand_get_id(char *nand_id)
{
	send_get_nand_id(nand_id);
}


/**
 * nand-reset - reset NAND
 * @chip:	this operation will been done on which nand chip
 */
int nand_reset(void)
{
	g_pnand_io->send_cmd_norb(CMD_RESET);
	//	g_pnand_io->send_cmd_norb(0);

	return nand_wait_rb();
}

/* 
 * Standard NAND Operation
 */

/**
 * do_select_chip - 
 */
static inline void do_select_chip(NAND_BASE *host,unsigned int page)
{
	int chipnr = -1;
	unsigned int page_per_chip = g_pnand_chip->ppchip;

	if (page > 0)
		chipnr = page / page_per_chip;

	g_pnand_ctrl->chip_select(host,g_pnand_io,chipnr);
}

/**
 * do_deselect_chip - 
 */
static inline void do_deselect_chip(NAND_BASE *host)
{
	g_pnand_ctrl->chip_select(host,g_pnand_io,-1);
}
/**
 * Calculate physical page address
 * toppage :the startpageid of virtual nand block;
 (two physical block is one virtual block in two-plane model)
 */
static inline unsigned int get_physical_addr(unsigned int page)
{
	unsigned int tmp =page / g_pnand_chip->ppblock;
	unsigned int toppage = (tmp - (tmp % g_pnand_chip->planenum))*g_pnand_chip->ppblock; 
	if(g_pnand_pt->use_planes)
		page = ((page-toppage) / g_pnand_chip->planenum) + (g_pnand_chip->ppblock * ((page-toppage) % g_pnand_chip->planenum)) + toppage;

	//	printk("^^^^^^^^^^^^^^^^   phy page =0x%08x ^^^^^^^^^^",page);
	return page;
}
/**
 * nand_erase_block -
 */
int nand_erase_block(NAND_BASE *host,unsigned int block)
{
	unsigned char status;
	int ret;
	unsigned int page = (block+g_startblock) * g_pnand_chip->ppblock;
	//	printk("nand erase one block +++++++++++++++++++++++++++++++++++++++\n");
	printk("nand erase one block ; blockid =%d ++++++++++++++++++++++++++++++\n",block);
	//	page =get_physical_addr(page);
	do_select_chip(host,page);

	send_erase_block(page);
	ret =send_read_status(&status);

	do_deselect_chip(host);
	printk(" nand erase block: status =%d ; ret =%d \n",status,ret);
	if(ret)
		return ret;
	else
		return status & NAND_STATUS_FAIL ? -ENAND : 0;
}

/**
 * nand_erase_2p_block -
 */
int nand_erase_2p_block(NAND_BASE *host,unsigned int block)
{
	unsigned char status;
	int ret;
	unsigned int page = (block*g_pnand_chip->planenum + g_startblock) * g_pnand_chip->ppblock;
	printk("nand erase two block ; blockid =%d ; page =%d ++++++++++++++++++++++++++++++\n",block,page);

	do_select_chip(host,page);

	send_erase_2p_block1(page);
	page += g_pnand_chip->ppblock;
	send_erase_2p_block2(page);
	ret =send_read_status(&status);

	do_deselect_chip(host);
	if(ret)
		return ret;
	else
		return status & NAND_STATUS_FAIL ? -ENAND : 0;
}

/****************************************************************/
/*****************  nand operation  *****************************/
/****************************************************************/
/*****      nand read base operation        *****/

/******************************************************
 *   nand_read_1p_page_oob
 * @ p_pageid : physical page address                                  
 */
static inline int nand_read_1p_oob(unsigned int p_pageid)
{
	int ret;
	send_read_page(g_pagesize, p_pageid);                             // read oobsize		
	ret =g_pnand_io->read_data_withrb(g_poobbuf,g_oobsize);  // read oobsize
	if(ret < 0)
		return ret;   // return io_error
	//	while((ret =g_pnand_io->dma_nand_finish()) == DMA_RUNNING);       //wait for channel 1 finished
	if (g_freesize){
		send_read_random(g_writesize);
		g_pnand_io->read_data_norb(g_poobbuf+g_oobsize,g_freesize);  //read freesize
		//	while((ret =g_pnand_io->dma_nand_finish()) == DMA_RUNNING);
	}
	//	dump_buf(g_poobbuf,g_oobsize);
	return SUCCESS;  
}
/******************************************************
 *   nand_read_2p_page_oob
 * @ p_pageid : physical page address                                  
 * @ model    : 0  norb  ,  1   withrb 
 */
static inline int nand_read_2p_page_oob(unsigned int p_pageid,unsigned char model)
{
	int ret =0;
	if(model){
		ret =send_read_2p_random_output_withrb(g_pagesize, p_pageid);         // read oobsize 
		if(ret < 0)
			return ret;
	}
	else
		send_read_2p_random_output_norb(g_pagesize,p_pageid);  // read oobsize		
	g_pnand_io->read_data_norb(g_poobbuf,g_oobsize);    // read oobsize
	if (g_freesize){
		send_read_random(g_writesize);
		g_pnand_io->read_data_norb(g_poobbuf+g_oobsize,g_freesize);  //read freesize
	}
	return ret;
}
static inline int ecc_page_decode(NAND_BASE *host,unsigned int offset,unsigned int size,unsigned char *databuf)
{
	unsigned int steps = (size >>g_sizeshift);
	unsigned char *eccbuf =g_poobbuf+g_eccpos+(offset>>g_sizeshift)*g_eccbytes;
	return g_pnand_ecc->ecc_enable_decode(host,databuf, eccbuf, g_eccbit,steps);
}

static inline int ecc_complete_decode(void)
{
	return g_pnand_ecc->ecc_finish();
}

/******************************************************
 *   nand_read_page_data
 * @ offset :  offset in the page
 * @ steps  :  number of ECC steps per operation, steps =Bytes / g_eccsize
 * @ databuf : the buffer for reading  per operation      
 * return value  : uncorrectable ecc error -- -5 ; retval -- the largest number of corrective bits             */
static inline int nand_read_page_data(NAND_BASE *host,unsigned int offset,unsigned int size,unsigned char *databuf)
{
	int ret=0;
	send_read_random(offset);
	g_pnand_io->read_data_norb(databuf, size);
	//	dump_buf(databuf,512);
	//	databuf[0]=0x0f;
	ret =ecc_page_decode(host,offset,size,databuf);
	//	ret =ecc_complete_decode();
	//	if(ret <0)
	//		return ret;
	return ret;   // ALL_FF, Uncorrectable or the largest number of corrective bits
}

/***********************************************************
 * read_process
 * return value : < 0  error ; = 0  success
 */
static inline int read_process(NAND_BASE *host,PageList *templist,unsigned int temp)
{
	int ret=SUCCESS;
	struct singlelist *listhead=0;
	while(temp --)
	{
		printk("pageid = 0x%08x ; bytes =%d ; offsetbytes =%d ^^^^^^^^^^^^^\n",templist->startPageID,templist->Bytes,templist->OffsetBytes);
		if(templist->Bytes ==0 || (templist->Bytes + templist->OffsetBytes) > g_writesize)  //judge Bytes and OffsetBytes
		{
			templist->retVal =-1;
			ret =ENAND;
			break;
		}		
		ret =nand_read_page_data(host,templist->OffsetBytes,templist->Bytes,(unsigned char *)templist->pData);		
		if(ret <0){
			templist->retVal =ret;
			ret =ENAND;
			break;
		}
		else if(ret >= g_eccbit-1){
			templist->retVal =templist->Bytes | (1<<24);
		}
		else {
			templist->retVal =templist->Bytes;
		}
		listhead = (templist->head).next;
		templist = singlelist_entry(listhead,PageList,head);
		//		if(temp)
		//			send_read_random(templist->OffsetBytes);
		ret=SUCCESS;
	}
	return ret;
}

/******************************************************
 *   nand_read_1p_page                   
 * @ temp  : the node count of the same startPageID  
 */
static inline int nand_read_1p_page(NAND_BASE *host,Aligned_List * aligned_list)
{
	unsigned int temp;
	int ret=0;
	unsigned int p_pageid;   
	PageList * templist;
	temp = aligned_list->opsmodel & 0x00ffffff;  //the number of pagelist
	templist =aligned_list->pagelist;
	if((templist->startPageID < 0) || (templist->startPageID > g_pagecount))  //judge startPageID 
	{
		templist->retVal =-1;
		return ENAND;
	}	
	p_pageid =get_physical_addr(templist->startPageID+g_startpage);
	do_select_chip(host,p_pageid);
	ret =nand_read_1p_oob(p_pageid);
	if(ret){   //read-1p-page oob
		dprintf("\n\n**********nand_read_1p_page, read oob wrong: ret =%d************\n",ret);
		templist->retVal =ret;
		g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);   //reset poobbuf      
		do_deselect_chip(host);
		return ret;
	}
	ret =read_process(host,templist,temp);
	g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);   //reset poobbuf             
	do_deselect_chip(host);
	return ret;
}

/******************************************************
 *   nand_read_2p_page_real ; It will be called when the nand supports two_plane operation        
 * @ temp  : the node count of the same startPageID  
 */
static inline int nand_read_2p_page_real(NAND_BASE *host,Aligned_List *aligned_list)
{
	unsigned int temp;
	int ret=0;
	unsigned int p_pageid;
	PageList * templist;

	templist =aligned_list->pagelist;    //first page
	if((templist->startPageID < 0) || (templist->startPageID > g_pagecount))  //judge startPageID 
	{
		templist->retVal =-1;
		return ENAND;
	}
	p_pageid = get_physical_addr(templist->startPageID+g_startpage);	
	do_select_chip(host,p_pageid);	

	send_read_2p_pageaddr(p_pageid,p_pageid+g_pnand_chip->ppblock);	
	temp = aligned_list->opsmodel & 0x00ffffff;   // 23 ~ 0 : the number of pagelist     		
	ret =nand_read_2p_page_oob(p_pageid,1);       //read oobsize of first page
	if(ret){	
		dprintf("\n\n**********nand_read_2p_page, read oob of first page wrong: ret =%d************\n",ret);
		templist->retVal =ret;
		g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);   //reset poobbuf      
		return ENAND;
	}
	ret =read_process(host,templist,temp);
	g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);   //reset poobbuf 	
	if(ret){
		do_deselect_chip(host);
		return ret;
	}
	templist =aligned_list->next->pagelist;        //second page
	if((templist->startPageID < 0) || (templist->startPageID > g_pagecount))  //judge startPageID 
	{
		templist->retVal =-1;
		do_deselect_chip(host);
		return ENAND;
	}
	temp = aligned_list->next->opsmodel & 0x00ffffff;   // 23 ~ 0 : the number of pagelist   
	ret =nand_read_2p_page_oob(p_pageid+g_pnand_chip->ppblock,0);    // read oobsize of second page
	if(ret){  
		dprintf("\n\n**********nand_read_2p_page, read oob of second page wrong: ret =%d************\n",ret);
		templist->retVal =ret;
		g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);   //reset poobbuf      
		return ENAND;
	}
	ret =read_process(host,templist,temp);
	g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);   //reset poobbuf             
	do_deselect_chip(host);
	return ret;
}

/******************************************************
 *   nand_read_2p_page_dummy ; It will be called when the nand doesn't support two_plane operation        
 */
static inline int nand_read_2p_page_dummy(NAND_BASE *host,Aligned_List *aligned_list)
{
	int ret;
	ret =nand_read_1p_page(host,aligned_list);    //first page
	if(ret <0)
		return ret;
	return nand_read_1p_page(host,aligned_list->next);  //second page
}

/******************************************************
 *   nand_read_2p_page ;       
 */
static inline int nand_read_2p_page(NAND_BASE *host,Aligned_List *aligned_list)
{
	if(g_pnand_pt->use_planes)
		return nand_read_2p_page_real(host,aligned_list);  // the nand supports two-plane operation
	else
		return nand_read_2p_page_dummy(host,aligned_list); // the nand supports one-plane operation only .
}

/*****     nand write base operation     *****/

static inline void ecc_page_encode(NAND_BASE *host,unsigned int offset, unsigned int size,unsigned char *databuf)
{		
	unsigned int steps = (size >>g_sizeshift);
	unsigned char *eccbuf =g_poobbuf+g_eccpos+(offset>>g_sizeshift)*g_eccbytes;
	g_pnand_ecc->ecc_enable_encode(host,databuf, eccbuf,g_eccbit,steps);
	//		dump_buf(g_poobbuf,g_oobsize);
}

/**
 * nand_write_page_data - just write page data and oob data
 *                        you can change the buf data bit intended to test ecc func
 */
static inline void nand_write_page_data(NAND_BASE *host,unsigned int offset,unsigned int size,unsigned char *databuf)
{
	ecc_page_encode(host,offset,size,databuf);
	send_prog_random(offset);
	g_pnand_io->write_data_norb(databuf, size);
}

/******************************************************
 *   nand_write_page_oob                                 
 */
static inline void nand_write_page_oob(void)
{
	//	dump_buf(g_poobbuf,g_oobsize);
	printk("eccbit =%d  ; freesize =%d \n",g_eccbit,g_freesize);
	send_prog_random(g_writesize);
	if (g_freesize){
		g_pnand_io->write_data_norb(g_poobbuf+g_oobsize,g_freesize);
	}
	g_pnand_io->write_data_norb(g_poobbuf,g_oobsize);
	g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);   //reset poobbuf 
}

/*********************************************************
 * write_process 
 * return value: <0 error  ; =0  success
 */
static inline int write_process(NAND_BASE *host,PageList *templist,unsigned int temp)
{
	int ret =SUCCESS;
	struct singlelist *listhead=0;
	while(temp --)
	{
		printk(" bytes =%d ; offsetbytes =%d ^^^^^^^^^^^^^\n",templist->Bytes,templist->OffsetBytes);
		if(templist->Bytes ==0 || (templist->Bytes + templist->OffsetBytes) > g_writesize)  //judge Bytes and OffsetBytes
		{
			templist->retVal =-1;
			ret =ENAND;
			break;
		}
		nand_write_page_data(host,templist->OffsetBytes,templist->Bytes,(unsigned char *)templist->pData);		
		templist->retVal =templist->Bytes;

		listhead = (templist->head).next;
		templist = singlelist_entry(listhead,PageList,head);
		//if(temp)
		//	send_prog_random(templist->OffsetBytes);
		ret =SUCCESS;
	}
	return ret;
}

/******************************************************
 *   nand_write_1p_page  
 * @ temp  : the node count of the same startPageID                                 
 */

static inline int nand_write_1p_page(NAND_BASE *host,Aligned_List *aligned_list)
{
	unsigned char state;
	unsigned int temp;
	int ret=0;
	unsigned int p_pageid;
	PageList * templist;
	dprintf("\nDEBUG nand : nand_write_1p_page\n");
	temp = aligned_list->opsmodel & 0x00ffffff; // 23 ~ 0 :the number of pagelist
	templist =aligned_list->pagelist;

	//    dprintf("\nDEBUG nand : pageid =%d ; g_startpage =%d ; g_pagecount =%d \n",templist->startPageID,g_startpage,g_pagecount);
	if((templist->startPageID < 0) || (templist->startPageID > g_pagecount))  //judge startPageID 
	{
		templist->retVal =-1;
		return ENAND;
	}
	p_pageid =get_physical_addr(templist->startPageID+g_startpage);
	do_select_chip(host,p_pageid);

	send_prog_page(templist->OffsetBytes,p_pageid);
	ret =write_process(host,templist,temp);
	if(ret){
		g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);   //reset poobbuf      
		do_deselect_chip(host);
		return ret;
	}
	nand_write_page_oob();
	send_prog_confirm();
	ret =send_read_status(&state);
	do_deselect_chip(host);

	if(ret == 0)
		ret =(state & NAND_STATUS_FAIL ? IO_ERROR : SUCCESS);
	if(ret < 0)
		aligned_list->pagelist->retVal =ret;
	return ret;
}


/******************************************************
 *   nand_write_2p_page_real , It will be called when the nand supports two_plane operation       
 * @ temp  : the node count of the same startPageID                        
 */

static inline int nand_write_2p_page_real(NAND_BASE *host,Aligned_List *aligned_list)
{
	unsigned char state;    
	unsigned int temp;
	int ret=0;
	unsigned int p_pageid;
	PageList * templist;

	//   dprintf("\nDEBUG nand : nand_write_2p_page_real\n");
	//  first page	
	temp = aligned_list->opsmodel & 0x00ffffff; //23~0 : the number of pagelist
	templist =aligned_list->pagelist;
	printk("   go into nand_write_2p_page_real ^^^^^^^^^^^^^^^\n");
	if((templist->startPageID < 0) || (templist->startPageID > g_pagecount))  //judge startPageID 
	{
		templist->retVal =-1;
		return ENAND;
	}
	p_pageid =get_physical_addr(templist->startPageID+g_startpage);
	do_select_chip(host,p_pageid);

	send_prog_2p_page1(templist->OffsetBytes,p_pageid);   // first page
	ret =write_process(host,templist,temp);
	if(ret){
		g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);   //reset poobbuf      
		do_deselect_chip(host);
		return ret;
	}
	nand_write_page_oob();
	send_prog_2p_confirm1();

	//  second page 	
	templist =aligned_list->next->pagelist;
	if((templist->startPageID < 0) || (templist->startPageID > g_pagecount))  //judge startPageID 
	{
		templist->retVal =-1;
		return ENAND;
	}
	temp = aligned_list->next->opsmodel & 0x00ffffff;     //second page	

	send_prog_2p_page2(templist->OffsetBytes,p_pageid+g_pnand_chip->ppblock);
	ret =write_process(host,templist,temp);
	if(ret){
		g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);   //reset poobbuf      
		do_deselect_chip(host);
		return ret;
	}
	nand_write_page_oob();
	send_prog_2p_confirm2();
	ret =send_read_status(&state);
	do_deselect_chip(host);
	//	dprintf("DEBUG %s: State 0x%02X\n", __func__, state);
	if(ret == 0)
		ret =(state & NAND_STATUS_FAIL ? IO_ERROR : SUCCESS);
	if(ret < 0)
		aligned_list->pagelist->retVal =ret;
	return ret;
}


/**nd_write_2p_page_dummy ; It will be called when the nand doesn't support two_plane operation        
 */
static inline int nand_write_2p_page_dummy(NAND_BASE *host,Aligned_List *aligned_list)
{
	int ret;
	ret =nand_write_1p_page(host,aligned_list);    //first page
	if(ret <0)
		return ret;
	return nand_write_1p_page(host,aligned_list->next);  //second page
}

/******************************************************
 *   nand_write_2p_page ;        
 */
static inline int nand_write_2p_page(NAND_BASE *host,Aligned_List *aligned_list)
{
	if(g_pnand_chip->planenum == 2)
		return nand_write_2p_page_real(host,aligned_list);   // the nand supports two-plane operation and one-plane operation
	else
		return nand_write_2p_page_dummy(host,aligned_list);  // the nand supports one-plane operation only.
}


/*****      nand interface       *****/

/*
 * nand_read_page - read one page with Hardware ECC in cpu mode
 * @page:	page offset
 * @buf:	buffer for read
 * @pageid: the pageid is a id in the ppartition
 */
int nand_read_page(NAND_BASE *host,unsigned int pageid,unsigned int offset,unsigned int bytes,void *databuf)
{
	unsigned int p_pageid;
	int ret;
	if(bytes ==0 || (bytes + offset) > g_writesize)  //judge offset and bytes
		return ENAND;
	//	dump_buf(g_poobbuf,g_oobsize);
	p_pageid =get_physical_addr(pageid+g_startpage);
	do_select_chip(host,p_pageid);
	ret =nand_read_1p_oob(p_pageid);   //read-1p-page oob
	if(ret < 0)
		return ret;        //return  timeout error
	ret =nand_read_page_data(host,offset,bytes,(unsigned char *)databuf);
	g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);   //reset poobbuf             
	do_deselect_chip(host);
	return (ret < 0) ? ret : SUCCESS;           //return Uncorrectable error or success
}
/******************************************************
 *   nand_read_pages ;        
 */
int nand_read_pages(NAND_BASE *host,Aligned_List *aligned_list)
{
	unsigned int temp;
	int ret,i,j;
	Aligned_List *templist =aligned_list;
	i=j=0;
	while(templist)
	{
		temp = templist->opsmodel & (0xff<<24);  // 31~24 : operation mode
		printk(" nand read pages :  opsmodel = 0x%x ;temp =0x%x \n",templist->opsmodel,temp);
		if(temp){
			ret =nand_read_2p_page(host,templist);
			templist = templist->next->next;
			i+=2;
		}
		else{
			ret =nand_read_1p_page(host,templist);
			templist =templist->next;
			j++;
		}
		if(ret)
			return ret;
	}
	dprintf("nand_read_pages : two_plane  %d pages;one_plane %d pages\n",i,j);
	return SUCCESS;
}

/******************************************************
 *   nand_write_page ;        
 */

int nand_write_page(NAND_BASE *host,unsigned int pageid, unsigned int offset,unsigned int bytes,void * databuf)
{
	unsigned int p_pageid;
	unsigned char state;
	//	int ret;
	if(bytes ==0 || (bytes + offset) > g_writesize)  //judge offset and bytes
		return ENAND;

	p_pageid =get_physical_addr(pageid+g_startpage);
	do_select_chip(host,p_pageid);

	send_prog_page(offset,p_pageid);   
	nand_write_page_data(host,offset,bytes,(unsigned char *)databuf);

	nand_write_page_oob();
	send_prog_confirm();
	send_read_status(&state);
	do_deselect_chip(host);
	return state & NAND_STATUS_FAIL ? IO_ERROR : SUCCESS;
}
/******************************************************
 *   nand_write_pages ;        
 */
int nand_write_pages(NAND_BASE *host,Aligned_List *aligned_list)
{
	unsigned int temp;
	int ret;
	Aligned_List *templist =aligned_list;
	while(templist)
	{
		temp = templist->opsmodel & (0xff<<24);  // 31~24 :operation mode
		printk(" nand write pages :  opsmodel = 0x%x ;temp =0x%x \n",templist->opsmodel,temp);
		if(temp){
			ret =nand_write_2p_page(host,templist);
			templist = templist->next->next;
		}
		else{
			ret =nand_write_1p_page(host,templist);
			templist =templist->next;
		}
		if(ret)
			return ret;
	}
	return SUCCESS;
}

/******************************************************
 *   nand_erase_blocks ;        
 * templist->startBlock: the blockid is a opposite id in one ppartition
 */
int nand_erase_blocks(NAND_BASE *host,BlockList *headlist)
{
	int state=0;
	int startblock =0;
	int count =0;	
	struct singlelist *listhead=0;
	BlockList *templist =headlist;
	if(g_pnand_pt->use_planes)
	{
		while(templist)                                   
		{
			startblock =templist->startBlock;
			count =templist->BlockCount;
			while(count--)
			{
				state = nand_erase_2p_block(host,startblock);
				if (state < 0){
					templist->retVal =templist->BlockCount-count;
					return state;
				}
				startblock++;
			}
			listhead = (templist->head).next;
			if(!listhead)
				break;
			templist = singlelist_entry(listhead,BlockList,head);
		}
	}else{
		while(templist)                                   
		{
			startblock =templist->startBlock;
			count =templist->BlockCount;
			while(count--)
			{
				state = nand_erase_block(host,startblock);
				if (state < 0){
					templist->retVal =templist->BlockCount-count;
					return state;
				}			
				startblock++;
			}
			listhead = (templist->head).next;
			if(!listhead)
				break;
			templist = singlelist_entry(listhead,BlockList,head);
		}
	}
	return state;
}
/******************************************************
 *   isbadblock ;
 *   return value : 0 -- valid block , -1 -- invalid block        
 */
int isbadblock(NAND_BASE *host,unsigned int blockid)          //按cpu方式读写 
{
	unsigned char state;
	int ret =0;
	blockid += g_startblock;
	do_select_chip(host,(blockid +1)*g_pnand_chip->ppblock -1);
	send_read_page(g_pagesize+g_pnand_chip->badblockpos,((blockid +1)*g_pnand_chip->ppblock -1));
	ret =g_pnand_io->read_data_withrb(&state, 1);
	do_deselect_chip(host);
	if(ret)
		return ret;  // nand io_error
	if (state == 0x0)
		return ENAND;
	return SUCCESS;	
}
/******************************************************
 *   markbadblock ;
 *          
 */
int markbadblock(NAND_BASE *host,unsigned int blockid)          //按cpu方式读写 ， 
{
	unsigned char state =0x0;
	int pageid =0;
	int ret=0;
	if(g_pnand_pt->use_planes){
		blockid = blockid * g_pnand_chip->planenum + g_startblock;
		do_select_chip(host,(blockid +1)*g_pnand_chip->ppblock -1);
		pageid =(blockid +1)*g_pnand_chip->ppblock -1;
		send_prog_2p_page1(g_pagesize+g_pnand_chip->badblockpos,pageid);
		g_pnand_io->write_data_norb(&state, 1);
		send_prog_2p_confirm1();
		send_prog_2p_page2(g_pagesize+g_pnand_chip->badblockpos,pageid+g_pnand_chip->ppblock);
		g_pnand_io->write_data_norb(&state, 1);
		send_prog_2p_confirm2();
		ret =send_read_status(&state);
		do_deselect_chip(host);
	}else{
		blockid = blockid + g_startblock;
		do_select_chip(host,(blockid +1)*g_pnand_chip->ppblock -1);
		send_prog_page(g_pagesize+g_pnand_chip->badblockpos,((blockid +1)*g_pnand_chip->ppblock -1));
		g_pnand_io->write_data_norb(&state, 1);
		send_prog_confirm();
		ret =send_read_status(&state);
		do_deselect_chip(host);
	}
	if(ret)
		return ret; // nand io_error
	dprintf("DEBUG %s: State 0x%02X\n", __func__, state);
	return (state & NAND_STATUS_FAIL ? IO_ERROR : SUCCESS);	
}



/********************************************************
 *              spl opreation for nand                  *
 ********************************************************/
static int spl_write_nand(NAND_BASE *host, Aligned_List *list, unsigned char *bchbuf, int bchsize, int numblock)
{
	PageList *pagelist = NULL;
	struct singlelist *listhead = NULL;
	unsigned char *tmpbchbuf = NULL;
	unsigned int j, opsmodel, pageid, steps;
	unsigned char status;
	int ret = 0;
	unsigned int tmpeccsize = g_pnand_ecc->eccsize;
	g_pnand_ecc->eccsize = SPL_BCH_BLOCK;

	pagelist = list->pagelist;
	pageid = pagelist->startPageID * 2 + g_pnand_chip->ppblock * numblock;
	if(pageid > g_pagecount) {
		pagelist->retVal = -1;
		ret = ENAND;
		dprintf("\n\n**********spl_write_nand, pageid error: ret =%d************\n",ret);
		goto spl_write_err1;
	}

	opsmodel = list->opsmodel & 0x00ffffff;

	pageid += g_startpage;
	do_select_chip(host, pageid);

	send_prog_page(pagelist->OffsetBytes, pageid);

	for (j = 0; j < opsmodel; j++) {

		if(pagelist->Bytes == 0 || (pagelist->Bytes + pagelist->OffsetBytes) > g_pnand_chip->pagesize) {
			pagelist->retVal = -1;
			ret = ENAND;
			dprintf("\n\n**********spl_write_nand, pagelist bytes or offsetbyte wrong: ret =%d************\n",ret);
			goto spl_write_err2;
		}
		steps = (pagelist->Bytes / SPL_BCH_BLOCK);

		tmpbchbuf = bchbuf + (pagelist->OffsetBytes / SPL_BCH_BLOCK) * SPL_BCH_SIZE; 
		
		g_pnand_ecc->ecc_enable_encode(host, pagelist->pData, tmpbchbuf, SPL_BCH_BIT, steps);

		g_pnand_io->write_data_norb(pagelist->pData, pagelist->Bytes);

		listhead = (pagelist->head).next;
		pagelist = singlelist_entry(listhead,PageList,head);
		
		if (j < opsmodel - 1) {
			send_prog_random(pagelist->OffsetBytes);
		} else {
			send_prog_confirm();
			ret = send_read_status(&status);
		}
	}

	//do_deselect_chip(host);
	if(ret == 0) {
		ret = (status & NAND_STATUS_FAIL ? IO_ERROR : SUCCESS);
	} else if(ret < 0) {
		pagelist->retVal = ret;
		dprintf("\n\n**********spl_write_nand, write data IO_ERROR: ret =%d************\n",ret);
		goto spl_write_err2;
		//return ret;
	}

	//do_select_chip(host,pageid + 1);
	send_prog_page(0, pageid + 1);

	g_pnand_io->write_data_norb(bchbuf, bchsize);

	send_prog_confirm();
	ret = send_read_status(&status);
	if(ret == 0) {
		ret = (status & NAND_STATUS_FAIL ? IO_ERROR : SUCCESS);
	} else if(ret < 0) {
		pagelist->retVal = ret;
		dprintf("\n\n**********spl_write_nand, write ecc IO_ERROR: ret =%d************\n",ret);
	}
spl_write_err2:
	do_deselect_chip(host);
spl_write_err1:
	g_pnand_ecc->eccsize = tmpeccsize;
	return ret;
}

static inline int boot_write_nand(NAND_BASE *host,Aligned_List *list)
{
	PageList * pagelist = NULL;
	unsigned int pageid, opsmodel;
	unsigned char status;
	int ret = 0;


	opsmodel = list->opsmodel & 0x00ffffff;
	pagelist = list->pagelist;

	if(pagelist->startPageID > g_pnand_chip->ppchip) {
		pagelist->retVal = -1;
		dprintf("\n\n**********boot_write_nand, bytes or offsetbyte error: ret =%d************\n",ret);		
		return ENAND;
	}
	pageid = pagelist->startPageID;
	do_select_chip(host, pageid);

	send_prog_page(pagelist->OffsetBytes, pageid);
	ret = write_process(host, pagelist, opsmodel);
	if(ret) {
		g_pnand_ecc->free_bch_buffer(g_oobsize + g_freesize);      
		do_deselect_chip(host);
		dprintf("\n\n**********boot_write_nand, write process error: ret =%d************\n",ret);
		return ret;
	}
	nand_write_page_oob();
	send_prog_confirm();
	ret =send_read_status(&status);
	do_deselect_chip(host);

	if(ret == 0) {
		ret = (status & NAND_STATUS_FAIL ? IO_ERROR : SUCCESS);
	} else if(ret < 0) {
		list->pagelist->retVal = ret;
		dprintf("\n\n**********boot_write_nand, write ecc IO_ERROR: ret =%d************\n",ret);
	}
	return ret;
}

//int mmmm= 0;

static int spl_read_nand(NAND_BASE *host, Aligned_List *list, int times, unsigned char *bchbuf, int bchsize)
{
	PageList *pagelist = NULL;
	struct singlelist *listhead = NULL;
	unsigned char *tmpbchbuf = NULL;
	unsigned int pageid, opsmodel, steps, i;
	int ret = 0;
	unsigned int tmpeccsize = g_pnand_ecc->eccsize;
	g_pnand_ecc->eccsize = SPL_BCH_BLOCK;

	pagelist = list->pagelist;
	pageid = pagelist->startPageID * 2 + times;
	if(pageid > g_pagecount) {
		pagelist->retVal = -1;
		ret = ENAND;
		dprintf("\n\n**********spl_read_nand, pageid error: ret =%d************\n",ret);
		goto spl_read_err1;
	}	
	pageid += g_startpage;

	do_select_chip(host, pageid);

	send_read_page(0, pageid + 1);
	ret =g_pnand_io->read_data_withrb(bchbuf, bchsize);
	if(ret){
		dprintf("\n\n**********spl_read_nand, read ecc error: ret =%d************\n",ret);
		pagelist->retVal = ret;
		goto spl_read_err2;
	}

	opsmodel = list->opsmodel & 0x00ffffff;

	for (i=0; i < opsmodel; i++) {
		if(pagelist->Bytes == 0 || (pagelist->Bytes + pagelist->OffsetBytes) > g_pnand_chip->pagesize) {
			pagelist->retVal = -1;
			ret = ENAND;
			dprintf("\n\n**********spl_read_nand, bytes or offsetbyte error: ret =%d************\n",ret);
			goto spl_read_err2;
		}		
		steps = (pagelist->Bytes / SPL_BCH_BLOCK);
		tmpbchbuf = bchbuf + (pagelist->OffsetBytes / SPL_BCH_BLOCK) * SPL_BCH_SIZE; 

		if(i == 0) {
			send_read_page(pagelist->OffsetBytes, pageid);
			g_pnand_io->read_data_withrb((unsigned char *)pagelist->pData, pagelist->Bytes);
			/* test spl read first and second block error and the third block right*/
/*			*((unsigned char *)pagelist->pData + 10) = 0xef;
			if(mmmm % 2 && mmmm < 5)
			{
				memset((unsigned char *)pagelist->pData, 0xff,10);
			}
				mmmm++;	
*/		
			ret = g_pnand_ecc->ecc_enable_decode(host, (unsigned char *)pagelist->pData, tmpbchbuf, SPL_BCH_BIT, steps);
		}else {
			send_read_random(pagelist->OffsetBytes);
			g_pnand_io->read_data_norb((unsigned char *)pagelist->pData, pagelist->Bytes);
			ret = g_pnand_ecc->ecc_enable_decode(host, (unsigned char *)pagelist->pData, tmpbchbuf, SPL_BCH_BIT, steps);
		} 
		if(ret < 0) {
			pagelist->retVal = ret;
			dprintf("\n\n**********spl_read_nand, data ecc error: ret =%d************\n",ret);
			goto spl_read_err2;
		} else if(ret >= SPL_BCH_BIT - 1) {
			pagelist->retVal = pagelist->Bytes | ( 1 << 24);
			printk("$$$$$$$$$$$$$$$$$$$$$   ecc error bits is %d $$$$$$$$$$$$$$$$$$$$$\n",ret);
		} else {
			printk("#####################   ecc error bits is %d #####################\n",ret);
			pagelist->retVal = pagelist->Bytes;
		}
//		dump_buf((unsigned char *)pagelist->pData+512,512);
//		printk("@@@@@@@@@@@@@@@@@@@@@@   pagelist->retVal is 0x%08x  @@@@@@@@@@@@@@@@@@@@@@@@@@\n",pagelist->retVal);
		listhead = (pagelist->head).next;
		pagelist = singlelist_entry(listhead,PageList,head);
	}
spl_read_err2:
	do_deselect_chip(host);
spl_read_err1:
	g_pnand_ecc->eccsize = tmpeccsize;
	return ret;
}

static inline int boot_read_nand(NAND_BASE *host,Aligned_List * list)
{
	PageList * pagelist = NULL;
	unsigned int opsmodel, pageid;
	int ret=0;

	opsmodel = list->opsmodel & 0x00ffffff;
	pagelist = list->pagelist;
	if(pagelist->startPageID > g_pnand_chip->ppchip) {
		pagelist->retVal = -1;
		dprintf("\n\n**********boot_read_nand, pageid error: ret =%d************\n",ret);
		return ENAND;
	}	
	pageid = (pagelist->startPageID);
	do_select_chip(host,pageid);
	ret =nand_read_1p_oob(pageid);
	if(ret) {
		dprintf("\n\n**********boot_read_nand, read oob error: ret =%d************\n",ret);
		pagelist->retVal = ret;
		g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);     
		do_deselect_chip(host);
		return ret;
	}

	ret = read_process(host, pagelist, opsmodel);
	if(ret) {
		dprintf("\n\n**********boot_read_nand, read process error: ret =%d************\n",ret);
	}
	g_pnand_ecc->free_bch_buffer(g_oobsize + g_freesize);             
	do_deselect_chip(host);
	return ret;
}

int write_spl(NAND_BASE *host, Aligned_List *list)
{
	Aligned_List *alignelist = NULL;
	int i, bchsize;
	unsigned char *bchbuf = NULL;
	int ret = 0;
	bchsize = g_pagesize / SPL_BCH_BLOCK * SPL_BCH_SIZE;
	bchbuf = (unsigned char *)kzalloc(bchsize, GFP_KERNEL);
	if (!bchbuf) {
		dprintf("bchbuf for spl write nomem");
		kfree(bchbuf);
		return -1;
	}
	memset(bchbuf, 0xff, bchsize);

	alignelist = list;
	while(alignelist != NULL) {
		if(alignelist->pagelist->startPageID < g_pnand_chip->ppblock) {
			for(i=0; i < 3; i++) {
				ret = spl_write_nand(host, alignelist, bchbuf, bchsize, i);
				
				
				if(ret < 0) {
					dprintf("spl write error");
					kfree(bchbuf);
				
				
					return ret;
				}
			}
		} else {
			ret = nand_write_1p_page(host, alignelist);
			if(ret < 0) {
				dprintf("boot write error");
				kfree(bchbuf);
				return ret;
			}
		}
		alignelist = alignelist->next;
	}
	if(ret == 0) {
		dprintf("spl write success");
	}
	
	
	kfree(bchbuf);
	return ret;
}

int read_spl(NAND_BASE *host, Aligned_List *list)
{
	Aligned_List *alignelist = NULL;
	int ret = 0, badblockflag = 0, bchsize, times;
	unsigned char *bchbuf = NULL;

	bchsize = g_pagesize / SPL_BCH_BLOCK * SPL_BCH_SIZE;
	bchbuf = (unsigned char *)kzalloc(bchsize, GFP_KERNEL);
	if (!bchbuf) {
		dprintf("bchbuf for spl read nomem");
		kfree(bchbuf);
		return -1;
	}
	memset(bchbuf, 0xff, bchsize);

	alignelist = list;
	while(alignelist != NULL) {
		if(alignelist->pagelist->startPageID < g_pnand_chip->ppblock) {
			
			switch(badblockflag) {
				case 0:
					times = g_pnand_chip->ppblock * 0;
					ret = spl_read_nand(host, alignelist, times, bchbuf, bchsize);
					if(ret < 0) {
						alignelist = list;
						badblockflag = 1;
					} else {
						break;
					}
				case 1:
					times = g_pnand_chip->ppblock * 1;
					ret = spl_read_nand(host, alignelist, times, bchbuf, bchsize);
					if(ret < 0) {
						alignelist = list;
						badblockflag = 2;
					} else {
						break;
					}
				case 2:
					times = g_pnand_chip->ppblock * 2;
					ret = spl_read_nand(host, alignelist, times, bchbuf, bchsize);
					if(ret < 0) {
						alignelist = list;
						badblockflag = 3;
					} else {
						break;
					}
				case 3:
					dprintf("^^^^^^^^^^^^^^^^^^^^^^ boot read error ^^^^^^^^^^^^^^^^^\n");
					kfree(bchbuf);
					return ret;
			}
		} else {
			ret = nand_read_1p_page(host, alignelist);
			if(ret < 0) {
				dprintf("boot read error");
				kfree(bchbuf);
				return ret;
			}
		}
		alignelist = alignelist->next;
	}
	if(ret == 0) {
		dprintf("spl read success");
	}
	
	
	kfree(bchbuf);
	return ret;
}
