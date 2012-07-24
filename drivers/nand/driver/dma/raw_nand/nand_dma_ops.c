/*
 *  lib_nand/core/raw_nand/nand_ops.c
 *
 * Jz NAND Driver
 *
 *  NAND IC ops and NAND ops
 */

#include "nand_api.h"
#include "nandcommand.h"
#include "../../include/blocklist.h"
extern JZ_IO jz_nand_io;
extern JZ_ECC jz_default_ecc;
JZ_NAND_CHIP *g_pnand_chip;
NAND_CTRL *g_pnand_ctrl;
JZ_IO *g_pnand_io;
JZ_ECC *g_pnand_ecc;

void do_nand_register(NAND_API *pnand_api)              //修改nand_api.c 中的nand_init（）函数
{
	g_pnand_chip = pnand_api->nand_chip;
	g_pnand_ctrl = pnand_api->nand_ctrl;
	g_pnand_io = pnand_api->nand_io;
	g_pnand_ecc = pnand_api->nand_ecc;
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
	g_pnand_io->send_addr(-1, first_row, cycles);
	g_pnand_io->send_cmd_norb(CMD_2P_READ_START1);
	g_pnand_io->send_addr(-1, second_row, cycles);
	g_pnand_io->send_cmd_norb(CMD_2P_READ_CONFIRM1);
}
static inline void send_read_2p_random_output_withrb(int column,unsigned int row)
{
	int cycles = g_pnand_chip->row_cycles;
	
	g_pnand_io->send_cmd_withrb(CMD_2P_READ_START2);
	g_pnand_io->send_addr(0, row, cycles);
	g_pnand_io->send_cmd_norb(CMD_2P_READ_RANDOM_OUTPUT);
	g_pnand_io->send_addr(column, -1, 0);
	g_pnand_io->send_cmd_norb(CMD_2P_READ_RANDOM_CONFIRM);
}

static inline void send_read_2p_random_output_norb(int column,unsigned int row)
{
	int cycles = g_pnand_chip->row_cycles;
	
	g_pnand_io->send_cmd_norb(CMD_2P_READ_START2);
	g_pnand_io->send_addr(0, row, cycles);
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
	g_pnand_io->send_addr(column, (int)row, cycles);
}

/**
 * send_prog_2p_confirm1 - do nand two-plane programm first confirm
 */
static inline void send_prog_2p_confirm1(void)
{
	g_pnand_io->send_cmd_norb(CMD_2P_PROGRAM_CONFIRM1);
}

/**
 * send_prog_2p_page2 - do nand two-plane programm first write cmd and addr
 * @column:	column address for NAND
 * @row:	row address for NAND
 */
static inline void send_prog_2p_page2(int column, unsigned int row)
{
	int cycles = g_pnand_chip->row_cycles;

	g_pnand_io->send_cmd_withrb(CMD_2P_PROGRAM_START2);  // change into withrb
	g_pnand_io->send_addr(column, (int)row, cycles);
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
	g_pnand_io->send_addr(-1, (int)row, cycles);
}

/**
 * send_erase_block - do nand single erase 
 * @row:	row address for NAND
 */
static inline void send_erase_2p_block2(unsigned int row)
{
	int cycles = g_pnand_chip->row_cycles;

	g_pnand_io->send_cmd_norb(CMD_2P_ERASE_START2);
	g_pnand_io->send_addr(-1, (int)row, cycles);
	g_pnand_io->send_cmd_norb(CMD_2P_ERASE_CONFIRM);
}

/**
 * send_read_status - read nand status
 * @status:	state for NAND status
 */
 void send_read_status(unsigned char *status)
{
	g_pnand_io->send_cmd_withrb(CMD_READSTATUS);
	jz_nand_io.read_data_norb(status, 1);
//	dprintf("DEBUG nand:*******  status =0x%0x\n",*status);
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
	jz_nand_io.read_data_norb(&nand_id[0], 5);
}

void nand_get_id(char *nand_id)
{
	send_get_nand_id(nand_id);
}


/**
 * nand-reset - reset NAND
 * @chip:	this operation will been done on which nand chip
 */
void nand_reset(void)
{
	g_pnand_io->send_cmd_norb(CMD_RESET);

	g_pnand_io->wait_ready();
}

/* 
 * Standard NAND Operation
 */

/**
 * do_select_chip - 
 */
static inline void do_select_chip(unsigned int page)
{
	int chipnr = -1;
	unsigned int page_per_chip = g_pnand_chip->ppchip;

	if (page > 0)
		chipnr = page / page_per_chip;
	g_pnand_ctrl->chip_select(g_pnand_io,chipnr);
}

/**
 * do_deselect_chip - 
 */
static inline void do_deselect_chip(void)
{
	g_pnand_ctrl->chip_select(g_pnand_io,-1);
}

/**
 * Calculate physical page address
*/
static inline unsigned int get_physical_addr(unsigned int page)
{
	/*if (page % planenum) page = page / planenum + g_pnand_chip->ppblock, else page = page / planenum*/
	page = (page / g_pnand_chip->planenum) + (g_pnand_chip->ppblock * (page % g_pnand_chip->planenum));
	return page;
}

/**
 * nand_erase_block -
 */
int nand_erase_block(unsigned int block)
{
	unsigned char status;
	unsigned int page;
	page = get_physical_addr(block*g_pnand_chip->ppblock);	

	do_select_chip(page);

	dprintf("DEBUG nand:go into nand_erase_block\n");
	send_erase_block(page);
	send_read_status(&status);
	do_deselect_chip();
	dprintf("DEBUG nand:go out nand_erase_block\n");

	return status & NAND_STATUS_FAIL ? ENAND : 0;
}

/**
 * nand_erase_2p_block -
 */
int nand_erase_2p_block(unsigned int block)
{
	unsigned char status;
	unsigned int page;

	page = get_physical_addr(block*g_pnand_chip->ppblock);	
	do_select_chip(page);

	dprintf("DEBUG nand:go into nand_erase_2p_block\n");
	send_erase_2p_block1(page);
	page += g_pnand_chip->ppblock;
	send_erase_2p_block2(page);
	send_read_status(&status);

	do_deselect_chip();
	dprintf("DEBUG nand:go out nand_erase_2p_block\n");

	return status & NAND_STATUS_FAIL ? ENAND : 0;
}

/****************************************************************/
/***************** new driver code  *****************************/
/****************************************************************/
static unsigned char *g_poobbuf;  // the pointer of storing oob buffer
static LINKED_LIST *g_pdmabuf;  // the pointer of storing data buffer per step
static unsigned int g_freesize;    // the area size in the page which cache ecc code
static unsigned int g_writesize;   // pagesize-freesize
static unsigned int g_oobsize;     // physical page size
static unsigned int g_eccpos;      // the place of ecc in the oob
static unsigned int g_eccsize;     // data bytes per Ecc step
//static unsigned int g_eccsteps;    // number of ECC steps per page
static unsigned int g_eccbytes;    // ECC bytes per step
static unsigned int g_errssize;    // store BHINT register and BHERR0~11 register per step, cacl by word
static unsigned int g_pagesize;    // physical page size
static unsigned char g_parameter;   // the number of the power of 2 
static unsigned int g_eccbit;      // the number of ECC checked bits

/************************************************
*   nand_ops_parameter_init                    
*   initialize global parameter                
*   for example:g_pagesize,g_freesize...   
*/    
void nand_ops_parameter_init(void)
{
	g_freesize = g_pnand_chip->freesize;
	g_writesize = g_pnand_chip->writesize;
	g_oobsize = g_pnand_chip->oobsize;
	g_eccpos = g_pnand_ecc->eccpos;
	g_eccsize = g_pnand_ecc->eccsize;
//	g_eccsteps = g_pnand_ecc->eccsteps;
	g_eccbytes = g_pnand_ecc->eccbytes;
	g_errssize = g_pnand_ecc->errssize;  /*cacl by word*/
	g_pagesize = g_pnand_chip->pagesize;
	g_eccbit = g_pnand_ecc->eccbit;
	if (g_freesize)
		g_poobbuf = (unsigned char *)g_pnand_ecc->get_ecc_code_buffer(g_oobsize+g_freesize); /*cacl by byte*/
	else
		g_poobbuf = (unsigned char *)g_pnand_ecc->get_ecc_code_buffer(g_oobsize); /*cacl by byte*/
	g_pdmabuf = (LINKED_LIST *)g_pnand_ecc->get_pdma_databuf_pointer();
	switch(g_eccsize){
		case 512:
			g_parameter =9;
			break;
		case 1024 :
			g_parameter =10;
			break;
		default :
			g_parameter =0;
			break;
	}
}

/******************************************************
*   nand_read_1p_page_oob
* @ p_pageid : physical page address                                  
*/
static inline int nand_read_1p_oob(unsigned int p_pageid)
{
	int ret;
	send_read_page(g_pagesize, p_pageid);                             // read oobsize		
	jz_nand_io.wait_ready();
	g_pnand_io->read_data_withrb(g_poobbuf,g_oobsize);  // read oobsize
	while((ret =g_pnand_io->dma_nand_finish()) == DMA_RUNNING);       //wait for channel 1 finished
	if (g_freesize && (!ret)){
		send_read_random(g_writesize);
		g_pnand_io->read_data_norb(g_poobbuf+g_oobsize,g_freesize);  //read freesize
		while((ret =g_pnand_io->dma_nand_finish()) == DMA_RUNNING);
	}
	return ret;
}
/******************************************************
*   nand_read_2p_page_oob
* @ p_pageid : physical page address                                  
* @ model    : 0  norb  ,  1   withrb 
*/
static inline int nand_read_2p_page_oob(unsigned int p_pageid,unsigned char model)
{
	int ret;
	if(model)
		send_read_2p_random_output_withrb(g_pagesize, p_pageid);         // read oobsize 
	else
		send_read_2p_random_output_norb(g_pagesize,p_pageid);  // read oobsize		
	g_pnand_io->read_data_norb(g_poobbuf,g_oobsize);    // read oobsize
	while((ret =g_pnand_io->dma_nand_finish()) == DMA_RUNNING);              //wait for channel 1 finished
	if (g_freesize && (!ret)){
		send_read_random(g_writesize);
		g_pnand_io->read_data_norb(g_poobbuf+g_oobsize,g_freesize);  //read freesize
		while((ret =g_pnand_io->dma_nand_finish()) == DMA_RUNNING);
	}
//	ret =-2;
	return ret;
}
/******************************************************
*   nand_read_page_data
* @ offset :  offset in the page
* @ steps  :  number of ECC steps per operation, steps =Bytes / g_eccsize
* @ databuf : the buffer for reading  per operation      
* return value  : address error -- -2 ; uncorrectable ecc error -- -5 ; retval -- the largest number of corrective bits                          
*/
static inline int nand_read_page_data(unsigned short offset, unsigned short steps, unsigned char *databuf)
{
	LINKED_LIST *temp1,*temp2;
	int i,j,ret0,ret1,retval=0;
	i =j =ret1 =0;
	ret0 =1;
	temp1 =temp2 =g_pdmabuf;
	send_read_random(offset);
	while(1){
		switch(ret0){
			case 1:
				g_pnand_io->read_data_norb(temp1->pbuf,g_eccsize); //first sector
				i++;
				ret0 = g_pnand_io->dma_nand_finish();
				break;
			case DMA_TERMINATE:
				temp1->mark =1;
				temp1 =temp1->next;
				if(i < steps)
				{
					if(!temp1->mark){
						g_pnand_io->read_data_norb(temp1->pbuf,g_eccsize);
						i++;
					}
				}
			case DMA_RUNNING:
				ret0 =g_pnand_io->dma_nand_finish();
				break;
			case DMA_AR:
				goto label_0;
		} 
		if(temp2->mark)
		{
			ret1 =g_pnand_ecc->dma_ecc_decode_stage1(temp2->pbuf,g_poobbuf+g_eccpos+(j+(offset>>g_parameter))*g_eccbytes,temp2->perrs);
			retval = (ret1 > retval)? ret1 :retval;
			if(ret1 < 0){
				dprintf("\n\n**********dma_ecc_decode_stagel wrong: ret1 =%d************\n",ret1);
				break;
				}
			ret1 =g_pnand_ecc->dma_ecc_decode_stage2(temp2->pbuf,databuf+j*g_eccsize);
			if(ret1< 0){
				dprintf("\n\n**********dma_ecc_decode_stage2 wrong: ret1 =%d************\n",ret1);
				break;
				}
			j++;
			temp2->mark =0;
			temp2 =temp2->next;
			if(j == steps)
				break;
		}
	}
label_0 :
	if(ret1 == -1)
		return -5;    // uncorrectable ECC error
	if((ret0 == DMA_AR) || (ret1 == DMA_AR))
		return DMA_AR;    // address error
//	retval =4;
	return retval;        //success
}


/***********************************************************
* read_process
* return value : < 0  error ; = 0  success
*/
static inline int read_process(PageList *templist,unsigned char temp)
{
	int ret=SUCCESS;
	struct singlelist *listhead=0;
	while(temp --)
	{
		if(templist->Bytes ==0 || (templist->Bytes + templist->OffsetBytes) > g_pnand_chip->writesize)  //judge Bytes and OffsetBytes
		{
			templist->retVal =-1;
			ret =ENAND;
			break;
		}		
		ret =nand_read_page_data(templist->OffsetBytes,templist->Bytes>>g_parameter,(unsigned char *)templist->pData);		
		if(ret <0){
			templist->retVal =ret;
			ret =ENAND;
			break;
			}
		else if(ret >= g_eccbit-1){
			templist->retVal =templist->Bytes | (1<<16);
		    }
		else {
			templist->retVal =templist->Bytes;
			}
		listhead = (templist->head).next;
		templist = singlelist_entry(listhead,PageList,head);
		if(temp)
			send_read_random(templist->OffsetBytes);
		ret=SUCCESS;
	}
	return ret;
}

/******************************************************
*   nand_read_1p_page                   
* @ temp  : the node count of the same startPageID  
*/
static inline int nand_read_1p_page(Aligned_List * aligned_list)
{
	unsigned char temp;
	int ret=0;
	unsigned int p_pageid;
	PageList * templist;
	temp = aligned_list->opsmodel & 0x07f;
	templist =aligned_list->pagelist;
	if(templist->startPageID > g_pnand_chip->ppchip)  //judge startPageID 
	{
		templist->retVal =-1;
		return ENAND;
	}	
	p_pageid =get_physical_addr(templist->startPageID);
	do_select_chip(p_pageid);
	ret =nand_read_1p_oob(p_pageid);
	if(ret){   //read-1p-page oob
		dprintf("\n\n**********nand_read_1p_page, read oob wrong: ret =%d************\n",ret);
		templist->retVal =ret;
		g_pnand_ecc->free_bch_buffer(g_oobsize);   //reset poobbuf      
		return ret;
	}
	ret =read_process(templist,temp);
	g_pnand_ecc->free_bch_buffer(g_oobsize);   //reset poobbuf             
	do_deselect_chip();
	return ret;
}
/******************************************************
*   nand_read_2p_page_real ; It will be called when the nand supports two_plane operation        
* @ temp  : the node count of the same startPageID  
*/
static inline int nand_read_2p_page_real(Aligned_List *aligned_list)
{
	unsigned char temp;
	int ret=0;
	unsigned int p_pageid;
	PageList * templist;
	
	templist =aligned_list->pagelist;    //first page
	if(templist->startPageID > g_pnand_chip->ppchip)  //judge startPageID 
	{
		templist->retVal =-1;
		return ENAND;
	}
	p_pageid = get_physical_addr(templist->startPageID);	
	do_select_chip(p_pageid);	
	
	send_read_2p_pageaddr(p_pageid,p_pageid+g_pnand_chip->ppblock);	
	temp = aligned_list->opsmodel & 0x07f;     		
	ret =nand_read_2p_page_oob(p_pageid,1);       //read oobsize of first page
	if(ret){	
		dprintf("\n\n**********nand_read_2p_page, read oob of first page wrong: ret =%d************\n",ret);
		templist->retVal =ret;
		g_pnand_ecc->free_bch_buffer(g_oobsize);   //reset poobbuf      
		return ENAND;
	}
	ret =read_process(templist,temp);
	g_pnand_ecc->free_bch_buffer(g_oobsize);   //reset poobbuf 	
	if(ret){
		do_deselect_chip();
		return ret;
 	}
	templist =aligned_list->next->pagelist;        //second page
	if(templist->startPageID > g_pnand_chip->ppchip)  //judge startPageID 
	{
		templist->retVal =-1;
		do_deselect_chip();
		return ENAND;
	}
	temp = aligned_list->next->opsmodel & 0x07f;  
	ret =nand_read_2p_page_oob(p_pageid+g_pnand_chip->ppblock,0);    // read oobsize of second page
	if(ret){  
		dprintf("\n\n**********nand_read_2p_page, read oob of second page wrong: ret =%d************\n",ret);
		templist->retVal =ret;
		g_pnand_ecc->free_bch_buffer(g_oobsize);   //reset poobbuf      
		return ENAND;
	}
	ret =read_process(templist,temp);
	g_pnand_ecc->free_bch_buffer(g_oobsize);   //reset poobbuf             
	do_deselect_chip();
	return ret;
}

/******************************************************
*   nand_read_2p_page_dummy ; It will be called when the nand doesn't support two_plane operation        
*/
static inline int nand_read_2p_page_dummy(Aligned_List *aligned_list)
{
	int ret;
	ret =nand_read_1p_page(aligned_list);    //first page
	if(ret <0)
		return ret;
	return nand_read_1p_page(aligned_list->next);  //second page
}

/******************************************************
*   nand_read_2p_page ;       
*/
static inline int nand_read_2p_page(Aligned_List *aligned_list)
{
	if(g_pnand_chip->planenum ==2)
		return nand_read_2p_page_real(aligned_list);  // the nand supports two-plane operation and one-plane operation
	else
		return nand_read_2p_page_dummy(aligned_list); // the nand supports one-plane operation only .
}


/******************************************************
*   nand_write_page_data
* @ offset :  offset in the page
* @ steps  :  number of ECC steps per operation, steps =Bytes / g_eccsize
* @ databuf : the buffer for writting  per operation      
* return value  : address error -- -2 ; 0 -- success                          
*/
static inline int nand_write_page_data(unsigned short offset, unsigned short steps, unsigned char *databuf)
{
	int i,j,ret0,ret1;
	LINKED_LIST *ptemp1,*ptemp2;
	ptemp1 =ptemp2 =g_pdmabuf;
	
	i =j =ret0 =0;
	ret1 =1;
	while(1)
	{
		if(i < steps)
		{
			if(!ptemp1->mark)
			{
				ret0 = g_pnand_ecc->dma_ecc_encode_stage1(databuf+i*g_eccsize,ptemp1->pbuf);
				if(ret0 < 0){
				dprintf("\n\n**********dma_ecc_encode_stagel wrong:************\n");
				break;
				}
				ret0 = g_pnand_ecc->dma_ecc_encode_stage2(ptemp1->pbuf,g_poobbuf+g_eccpos+(i+(offset>>g_parameter))*g_eccbytes);
				if(ret0 < 0){
				dprintf("\n\n**********dma_ecc_encode_stage2 wrong:************\n");
				break;
				}
				i++;
				ptemp1->mark = 1;
				ptemp1 =ptemp1->next;
			}
		}
		switch(ret1){
			case 1:
				g_pnand_io->write_data_norb(ptemp2->pbuf,g_eccsize);
				j++;
				ret1 =g_pnand_io->dma_nand_finish();
				break;
			case DMA_TERMINATE:
				ptemp2->mark =0;
				ptemp2 =ptemp2->next;
				if(j < steps)
				{
					if(ptemp2->mark){
						g_pnand_io->write_data_norb(ptemp2->pbuf,g_eccsize);
						j++;
					}
				}
				else goto label_1;
			case DMA_RUNNING:
				ret1 =g_pnand_io->dma_nand_finish();
				break;
			case DMA_AR:
				goto label_1;
		}
	}
label_1:
	if(ret0 || ret1)
		return  DMA_AR;    // address error
	return SUCCESS;
}
/******************************************************
*   nand_write_page_oob                                 
*/

static inline int nand_write_page_oob(void)
{
	int ret;
	send_prog_random(g_writesize);
	if (g_freesize){
		g_pnand_io->write_data_norb(g_poobbuf+g_oobsize,g_freesize);
		while((ret =g_pnand_io->dma_nand_finish()) == DMA_RUNNING);    //wait for channel 1 finished
		if(ret)
			return ret;
		}
	g_pnand_io->write_data_norb(g_poobbuf,g_oobsize);
	while((ret =g_pnand_io->dma_nand_finish()) == DMA_RUNNING);
	g_pnand_ecc->free_bch_buffer(g_oobsize);   //reset poobbuf 
	return ret;
}

/*********************************************************
* write_process 
* return value: <0 error  ; =0  success
*/
static inline int write_process(PageList *templist,unsigned char temp)
{
	int ret =SUCCESS;
	struct singlelist *listhead=0;
	while(temp --)
	{
		if(templist->Bytes ==0 || (templist->Bytes + templist->OffsetBytes) > g_pnand_chip->writesize)  //judge Bytes and OffsetBytes
		{
			templist->retVal =-1;
			ret =ENAND;
			break;
		}
		ret =nand_write_page_data(templist->OffsetBytes,templist->Bytes>>g_parameter,(unsigned char *)templist->pData);		
		if(ret <0){
			templist->retVal =ret;
			ret =ENAND;
			break;
			}
		else {
			templist->retVal =templist->Bytes;
			}			
		listhead = (templist->head).next;
//		if(!listhead)
//			return;
		templist = singlelist_entry(listhead,PageList,head);
//		templist = templist->Next;
		if(temp)
			send_prog_random(templist->OffsetBytes);
		ret =SUCCESS;
	}
	return ret;
}

/******************************************************
*   nand_write_1p_page  
* @ temp  : the node count of the same startPageID                                 
*/

static inline int nand_write_1p_page(Aligned_List *aligned_list)
{
	unsigned char temp,state;
	int ret=0;
	unsigned int p_pageid;
	PageList * templist;
//    dprintf("\nDEBUG nand : nand_write_1p_page\n");
	temp = aligned_list->opsmodel & 0x07f;
	templist =aligned_list->pagelist;
	if(templist->startPageID > g_pnand_chip->ppchip)  //judge startPageID 
	{
		templist->retVal =-1;
		return ENAND;
	}
	p_pageid =get_physical_addr(templist->startPageID);
	do_select_chip(p_pageid);
	
	send_prog_page(templist->OffsetBytes,p_pageid);
	ret =write_process(templist,temp);
	if(ret){
		g_pnand_ecc->free_bch_buffer(g_oobsize);   //reset poobbuf      
		do_deselect_chip();
		return ret;
	}
	ret =nand_write_page_oob();
	if(ret){
		dprintf("\n\n**********nand_write_1p_page, write oob wrong: ret =%d************\n",ret);
		aligned_list->pagelist->retVal =ret;
		return ret;
	}	
	send_prog_confirm();
	send_read_status(&state);
	do_deselect_chip();

	ret =(state & NAND_STATUS_FAIL ? IO_ERROR : SUCCESS);
//	ret =IO_ERROR;
	if(ret <0)
		aligned_list->pagelist->retVal =ret;
	return ret;
}

/******************************************************
*   nand_write_2p_page_real , It will be called when the nand supports two_plane operation       
* @ temp  : the node count of the same startPageID                        
*/

static inline int nand_write_2p_page_real(Aligned_List *aligned_list)
{
	unsigned char temp,state;    
	int ret=0;
	unsigned int p_pageid;
	PageList * templist;
	
//   dprintf("\nDEBUG nand : nand_write_2p_page_real\n");
//  first page	
	temp = aligned_list->opsmodel & 0x07f;
	templist =aligned_list->pagelist;
	if(templist->startPageID > g_pnand_chip->ppchip)  //judge startPageID 
	{
		templist->retVal =-1;
		return ENAND;
	}
	p_pageid =get_physical_addr(templist->startPageID);
	do_select_chip(p_pageid);
	
	send_prog_2p_page1(templist->OffsetBytes,p_pageid);   // first page
	ret =write_process(templist,temp);
	if(ret){
		g_pnand_ecc->free_bch_buffer(g_oobsize);   //reset poobbuf      
		do_deselect_chip();
		return ret;
	}
	ret =nand_write_page_oob();
	if(ret){
		dprintf("\n\n**********nand_write_2p_page, write oob of first page wrong: ret =%d************\n",ret);
		aligned_list->pagelist->retVal =ret;
		return ret;
	}	
	send_prog_2p_confirm1();
	
//  second page 	
	templist =aligned_list->next->pagelist;
	if(templist->startPageID > g_pnand_chip->ppchip)  //judge startPageID 
	{
		templist->retVal =-1;
		return ENAND;
	}
	temp = aligned_list->next->opsmodel & 0x07f;     //second page	
	
	send_prog_2p_page2(templist->OffsetBytes,p_pageid+g_pnand_chip->ppblock);
	ret =write_process(templist,temp);
	if(ret){
		g_pnand_ecc->free_bch_buffer(g_oobsize);   //reset poobbuf      
		do_deselect_chip();
		return ret;
	}
	ret =nand_write_page_oob();
	if(ret){
		dprintf("\n\n**********nand_write_2p_page, write oob of second page wrong: ret =%d************\n",ret);
		aligned_list->next->pagelist->retVal =ret;
		return ret;
	}
	send_prog_2p_confirm2();
	send_read_status(&state);
	do_deselect_chip();
//	dprintf("DEBUG %s: State 0x%02X\n", __func__, state);
	ret =(state & NAND_STATUS_FAIL ? IO_ERROR : SUCCESS);	
	if(ret <0)
		aligned_list->pagelist->retVal =ret;
	return ret;
}
/******************************************************
*   nand_write_2p_page_dummy ; It will be called when the nand doesn't support two_plane operation        
*/
static inline int nand_write_2p_page_dummy(Aligned_List *aligned_list)
{
	int ret;
	ret =nand_write_1p_page(aligned_list);    //first page
	if(ret <0)
		return ret;
	return nand_write_1p_page(aligned_list->next);  //second page
}

/******************************************************
*   nand_write_2p_page ;        
*/
static inline int nand_write_2p_page(Aligned_List *aligned_list)
{
	if(g_pnand_chip->planenum == 2)
		return nand_write_2p_page_real(aligned_list);   // the nand supports two-plane operation and one-plane operation
	else
		return nand_write_2p_page_dummy(aligned_list);  // the nand supports one-plane operation only.
}

/******************************************************
*   nand_read_page ;        
*/

int nand_read_page(unsigned int pageid, unsigned int offset,unsigned int bytes,void * databuf)
{
	unsigned int p_pageid;
	int ret;
	if(pageid > g_pnand_chip->ppchip)  //judge pgeid
		return ENAND;
	if(bytes ==0 || (bytes + offset) > g_pnand_chip->writesize)  //judge offset and bytes
		return ENAND;
	
	p_pageid =get_physical_addr(pageid);
	do_select_chip(p_pageid);
	nand_read_1p_oob(p_pageid);   //read-1p-page oob
	ret =nand_read_page_data(offset,bytes>>g_parameter,(unsigned char *)databuf);
	g_pnand_ecc->free_bch_buffer(g_oobsize);   //reset poobbuf             
	do_deselect_chip();
	return (ret < 0) ? ret : SUCCESS;	
}

/******************************************************
*   nand_read_pages ;        
*/
int nand_read_pages(Aligned_List *aligned_list)
{
	unsigned char temp;
	int ret,i,j;
	Aligned_List *templist =aligned_list;
	i=j=0;
	while(templist)
	{
		temp = templist->opsmodel & 0x80;
		if(temp){
			ret =nand_read_2p_page(templist);
			templist = templist->next->next;
			i+=2;
		}
		else{
			ret =nand_read_1p_page(templist);
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
int nand_write_page(unsigned int pageid, unsigned int offset,unsigned int bytes,void * databuf)
{
	unsigned int p_pageid;
	unsigned char state;
	int ret;
	if(pageid > g_pnand_chip->ppchip)  //judge pgeid
		return ENAND;
	if(bytes ==0 || (bytes + offset) > g_pnand_chip->writesize)  //judge offset and bytes
		return ENAND;
		
	p_pageid =get_physical_addr(pageid);
	do_select_chip(p_pageid);
	
	send_prog_page(offset,p_pageid);   
	ret =nand_write_page_data(offset,bytes>>g_parameter,(unsigned char *)databuf);

	if((ret =nand_write_page_oob())){
		dprintf("\n\n**********nand_write_page, write oob wrong: ret =%d************\n",ret);
		return ret;
	}	
	send_prog_confirm();
	send_read_status(&state);
	do_deselect_chip();
	
	return state & NAND_STATUS_FAIL ? IO_ERROR : SUCCESS;
}
/******************************************************
*   nand_write_pages ;        
*/
int nand_write_pages(Aligned_List *aligned_list)
{
	unsigned char temp;
	int ret;
	Aligned_List *templist =aligned_list;
	while(templist)
	{
		temp = templist->opsmodel & 0x80;
		if(temp){
			ret =nand_write_2p_page(templist);
			templist = templist->next->next;
		}
		else{
			ret =nand_write_1p_page(templist);
			templist =templist->next;
		}
		if(ret)
			return ret;
	}
	return SUCCESS;
}
/******************************************************
*   nand_erase_blocks ;        
*/
int nand_erase_blocks(BlockList *headlist)
{
	int state;
	int startblock =0;
	int count =0;	
	struct singlelist *listhead;
	BlockList *templist =headlist;
	while(templist)                                   
	{
		startblock =templist->startBlock;
	    count =templist->BlockCount;	
		if (startblock % 2)
		{
			state = nand_erase_block(templist->startBlock);
			if (state < 0){
				templist->retVal =templist->BlockCount-count;
				return state;
			}
			count--;
			startblock++;
		}
		while(count >> 1)
		{
			state = nand_erase_2p_block(startblock);
			if (state < 0){
				templist->retVal =templist->BlockCount-count;
				return state;
			}
			count -=2;
			startblock +=2;
		}	
		if (count)
		{
			state = nand_erase_block(templist->startBlock);
			if (state < 0){
				templist->retVal =templist->BlockCount-count;
				return state;
			}
		}
		state =nand_erase_blocks(templist);
		if(state)
			break;
		listhead = (templist->head).next;
		if(!listhead)
			break;
		templist = singlelist_entry(listhead,BlockList,head);
	}
	return state;
}
/******************************************************
*   isbadblock ;
*   return value : 0 -- valid block , -1 -- invalid block        
*/
int isbadblock(unsigned int blockid)          //按cpu方式读写 
{
	unsigned char state;
	do_select_chip((blockid +1)*g_pnand_chip->ppblock -1);
	send_read_page(g_pagesize+g_pnand_chip->badblockpos,((blockid +1)*g_pnand_chip->ppblock -1));
//	send_read_confirm();
	jz_nand_io.read_data_withrb(&state, 1);
	do_deselect_chip();
	if (state == 0x0)
		return ENAND;
	return SUCCESS;	
}
/******************************************************
*   markbadblock ;
*          
*/
int markbadblock(unsigned int blockid)          //按cpu方式读写 ， 
{
	unsigned char state =0x0;
	do_select_chip((blockid +1)*g_pnand_chip->ppblock -1);
	send_prog_page(g_pagesize+g_pnand_chip->badblockpos,((blockid +1)*g_pnand_chip->ppblock -1));
	jz_nand_io.write_data_norb(&state, 1);
	send_prog_confirm();
	send_read_status(&state);
	do_deselect_chip();
	dprintf("DEBUG %s: State 0x%02X\n", __func__, state);
	return (state & NAND_STATUS_FAIL ? IO_ERROR : SUCCESS);	
}









