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
#include <linux/delay.h>
#define OLD 0

#define WRITE 0
#define READ 1
static JZ_NAND_CHIP *g_pnand_chip;
static NAND_CTRL *g_pnand_ctrl;
static JZ_IO *g_pnand_io;
static JZ_ECC *g_pnand_ecc;
/*
static void dump_buf1(unsigned char *databuf,int len)
{
	int i = 0;
	for(i=0;i<len;i++){
		if(i%16 == 0)
			printk("\n");
		printk("%02x ",databuf[i]);
	}
}
*/
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
static unsigned int g_eccbit;      // the number of ECC checked bits
static int g_startblock;           // the first blockid of one partition
static int g_startpage;            // the first pageid of one partition
static int g_pageperblock;          // the pageperblock of one partition
static unsigned int g_pagecount;    // the page number of one partition
static struct platform_nand_partition *g_pnand_pt;    // nand partition info

static unsigned char g_writebuf[512];          // reserve buf,it is 0xff all;
static unsigned char *g_readbuf;
/*
  struct EccSector{
  unsigned char *buf;
  int *retval;
  };
*/
struct EccSector *g_eccsector;
static unsigned int g_eccsector_len;
static unsigned char **g_pointarray;
static unsigned int g_pointarray_len;
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
	memset(g_writebuf,0xff,512);
	g_readbuf =(unsigned char *)nand_malloc_buf(g_eccsize);
	g_eccsector_len = g_pagesize / 512;
	g_eccsector =(struct EccSector *)nand_malloc_buf(sizeof(struct EccSector)*g_eccsector_len);
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
	g_pageperblock =ppt->pageperblock;
	g_pagecount = ppt->PageCount;
	g_poobbuf = (unsigned char *)g_pnand_ecc->get_ecc_code_buffer(g_oobsize+g_freesize); /*cacl by byte*/
	memset(g_eccsector,0,sizeof(struct EccSector)*g_eccsector_len);
	g_pointarray_len = g_writesize / 512;
	if(g_pointarray)
		nand_free_buf(g_pointarray);
	g_pointarray =(unsigned char **)nand_malloc_buf(sizeof(unsigned char *)*g_pointarray_len);
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
#define NAND_READID_NORB
static inline void send_get_nand_id(char *nand_id)
{
	g_pnand_io->send_cmd_norb(CMD_READID);
	g_pnand_io->send_addr(-1, 0x00, 1);

	/* Read manufacturer and device IDs */
#ifdef NAND_READID_NORB
	g_pnand_io->read_data_norb(&nand_id[0], 5);
	msleep(10);
#else
	g_pnand_io->read_data_withrb(&nand_id[0], 5);
#endif
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

	if (page >= 0)
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
	// printk("nand erase one block ; blockid =%d ++++++++++++++++++++++++++++++\n",block);
	//	page =get_physical_addr(page);
	do_select_chip(host,page);

	send_erase_block(page);
	ret =send_read_status(&status);

	do_deselect_chip(host);
	//printk(" nand erase block: status =%d ; ret =%d \n",status,ret);
	if(ret)
		return ret;
	else
		return status & NAND_STATUS_FAIL ? ENAND : 0;
}

/**
 * nand_erase_2p_block -
 */
int nand_erase_2p_block(NAND_BASE *host,unsigned int block)
{
	unsigned char status;
	int ret;
	unsigned int page = (block*g_pnand_chip->planenum + g_startblock) * g_pnand_chip->ppblock;
	//printk("nand erase two block ; blockid =%d ; page =%d ++++++++++++++++++++++++++++++\n",block,page);

	do_select_chip(host,page);

	send_erase_2p_block1(page);
	page += g_pnand_chip->ppblock;
	send_erase_2p_block2(page);
	ret =send_read_status(&status);

	printk(" nand erase two block: status =%d ; ret =%d \n",status,ret);
	do_deselect_chip(host);
	if(ret)
		return ret;
	else
		return status & NAND_STATUS_FAIL ? ENAND : 0;
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
//	printk(" %s  phy_pageid = %d \n",__func__,p_pageid);
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
//	if(p_pageid == 42497)
//		dump_buf1(g_poobbuf,g_oobsize);
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
//	printk(" %s  phy_pageid = %d \n",__func__,p_pageid);
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
	unsigned int steps = (size / g_eccsize);
	unsigned char *eccbuf =g_poobbuf+g_eccpos+(offset/g_eccsize)*g_eccbytes;
	return g_pnand_ecc->ecc_enable_decode(host,databuf, eccbuf, g_eccbit,steps);
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
	ret =ecc_page_decode(host,offset,size,databuf);
	return ret;   // ALL_FF, Uncorrectable or the largest number of corrective bits
}
static inline int nand_read_sector_data(NAND_BASE *host,int id,const int num)
{
	int ret =SUCCESS;
	int i=0,j=0;
	int sectorid =id * num;
	unsigned char *eccbuf =g_poobbuf+g_eccpos+ id * g_eccbytes;
	send_read_random(sectorid * 512);
	for(i=0;i<num;i++){
		g_pnand_io->read_data_norb(g_eccsector[sectorid+i].buf,512);
	}

	bch_enable(host,BCH_DECODE,g_eccsize,g_eccbit);
	for(i=0;i<num;i++)
		for(j=0;j<512;j++)
			bch_writeb(host->bch_iomem,BCH_DR,*(g_eccsector[sectorid+i].buf+j));
	for(j=0;j<g_eccbytes;j++)
		bch_writeb(host->bch_iomem,BCH_DR,eccbuf[j]);
	/* Wait for completion */
	bch_decode_sync(host);
	// dump_buf(g_eccsector[sectorid].buf,512);
	//dump_buf(eccbuf,14*4);
	ret =g_pnand_ecc->bch_decode_correct(host,&g_eccsector[sectorid]);
	bch_decints_clear(host);
	bch_disable(host);
	if(ret < 0 || ret >= g_eccbit-1)
	{
		for(i=0;i<num;i++)
			if(g_eccsector[sectorid+i].retval){
				if(ret < 0)
					*(g_eccsector[sectorid+i].retval) =ret;
				else
					*(g_eccsector[sectorid+i].retval) |= (1<<16);
			}
	}
	return ret;
}
static inline int nand_write_sector_data(NAND_BASE *host,int id, const int num)
{
	int ret =SUCCESS;
	int i=0,j=0;
	int sectorid =id * num;
	unsigned char *eccbuf =g_poobbuf+g_eccpos+ id * g_eccbytes;
	volatile unsigned char *paraddr = (volatile unsigned char *)(host->bch_iomem+BCH_PAR0);
	bch_enable(host,BCH_ENCODE, g_eccsize,g_eccbit);
	for(i=0; i<num; i++){// how much 512'bytes per ecc steps
		for(j=0;j<512;j++)
			bch_writeb(host->bch_iomem,BCH_DR,*(g_eccsector[sectorid+i].buf + j));
	}
	bch_encode_sync(host);
	for (i=0; i < g_eccbytes; i++)
		eccbuf[i] = *paraddr++;
	bch_encints_clear(host);
	bch_disable(host);
	send_prog_random(id*g_eccsize);
	for(i=0; i<num; i++){// how much 512'bytes per ecc steps
		g_pnand_io->write_data_norb(g_eccsector[sectorid+i].buf, 512);
	}
	return ret;
}

/***********************************************************
 * data_process
 * return value : < 0  error ; = 0  success
 */
static inline int data_process(NAND_BASE *host,PageList *templist,unsigned int temp,unsigned char mode)
{
	int ret=SUCCESS;
	int i=0, retflag=0;
	struct singlelist *listhead=0;
	int index=0,flag=0;
	int sectorperecc = g_eccsize /512;
	int offset = templist->OffsetBytes;
	int bytes =templist->Bytes;
	templist->retVal =templist->Bytes;
	if(templist->Bytes ==0 ||(templist->Bytes + templist->OffsetBytes)>g_writesize)
	{
		templist->retVal = -1;
		return ENAND;
	}
	temp--;
	for(index =0; index < g_eccsector_len; index++)
	{
		if(index == offset / 512){
			g_eccsector[index].buf =(unsigned char *)templist->pData + i;
			g_eccsector[index].retval = &(templist->retVal);
			i +=512;
			offset +=512;
			bytes -=512;
			flag =1;
		}else{
			if(mode)
				g_eccsector[index].buf =g_readbuf + (index%sectorperecc)*512;
			else
				g_eccsector[index].buf =g_writebuf;
			g_eccsector[index].retval = NULL;
		}
		//	index++;
		if(((index+1)%sectorperecc == 0) && flag){
			if(mode)
				ret =nand_read_sector_data(host,index/sectorperecc,g_eccsize/512);
			else
				ret =nand_write_sector_data(host,index/sectorperecc,g_eccsize/512);
			flag =0;
			if(ret<0)
				return ret;
			if(ret>0)
				retflag = 1;
		}
		if(!bytes){
			if(temp){
				listhead = (templist->head).next;
				templist = singlelist_entry(listhead,PageList,head);
				if(templist->Bytes ==0 ||(templist->Bytes + templist->OffsetBytes)>g_writesize)
				{
					templist->retVal =-1;
					ret =ENAND;
					break;
				}
				templist->retVal =templist->Bytes;
				offset = templist->OffsetBytes;
				bytes = templist->Bytes;
				i=0;
				temp--;
			}else
				break;
		}
	}
	//  printk("~~~~~~~~1111~~~~~~~~~~index = %d\n",index);
	if((index+1)%sectorperecc && flag){
		while((index+1)%sectorperecc){
			index++;
			g_eccsector[index].buf =g_readbuf + (index%sectorperecc)*512;
			g_eccsector[index].retval = NULL;
		}
		if(mode)
			ret =nand_read_sector_data(host,index/sectorperecc,g_eccsize/512);
		else
			ret =nand_write_sector_data(host,index/sectorperecc,g_eccsize/512);
		if(ret < 0)
			return ret;
		if(ret > 0)
			retflag = 1;
	}
	return retflag ? 1 : ret;
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
		goto nand_read_1p_page_error;
	}
	ret =data_process(host,templist,temp,READ);
	g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);   //reset poobbuf
nand_read_1p_page_error:
	do_deselect_chip(host);
	if(ret<0)
		printk("DEBUG: %s [%d] ret = %d \n ",__func__,__LINE__,ret);
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
		goto nand_read_2p_page_error;
	}
	ret =data_process(host,templist,temp,READ);
	g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);   //reset poobbuf
	if(ret < 0)
		goto nand_read_2p_page_error;
	templist =aligned_list->next->pagelist;        //second page
	if((templist->startPageID < 0) || (templist->startPageID > g_pagecount))  //judge startPageID
	{
		templist->retVal =-1;
		goto nand_read_2p_page_error;
	}
	temp = aligned_list->next->opsmodel & 0x00ffffff;   // 23 ~ 0 : the number of pagelist
	ret =nand_read_2p_page_oob(p_pageid+g_pnand_chip->ppblock,0);    // read oobsize of second page
	if(ret){
		dprintf("\n\n**********nand_read_2p_page, read oob of second page wrong: ret =%d************\n",ret);
		templist->retVal =ret;
		goto nand_read_2p_page_error;
	}
	ret =data_process(host,templist,temp,READ);
nand_read_2p_page_error:
	g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);   //reset poobbuf
	do_deselect_chip(host);
	if(ret<0)
		printk("DEBUG: %s [%d] ret = %d \n ",__func__,__LINE__,ret);

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
	unsigned int steps = (size / g_eccsize);
	unsigned char *eccbuf =g_poobbuf+g_eccpos+(offset / g_eccsize)*g_eccbytes;
	g_pnand_ecc->ecc_enable_encode(host,databuf, eccbuf,g_eccbit,steps);
	//		dump_buf(g_poobbuf,g_oobsize);
}

/**
 *  nand_write_page_data - just write page data and oob data
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
//	printk("eccbit =%d  ; freesize =%d \n",g_eccbit,g_freesize);
	send_prog_random(g_writesize);
	if (g_freesize){
		g_pnand_io->write_data_norb(g_poobbuf+g_oobsize,g_freesize);
	}
	g_pnand_io->write_data_norb(g_poobbuf,g_oobsize);
	g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);   //reset poobbuf
}

static inline int nand_write_1p_page(NAND_BASE *host,Aligned_List *aligned_list)
{
	unsigned char state;
	unsigned int temp;
	int ret=0;
	unsigned int p_pageid;
	PageList * templist;
//	dprintf("\nDEBUG nand : nand_write_1p_page\n");
	temp = aligned_list->opsmodel & 0x00ffffff; // 23 ~ 0 :the number of pagelist
	templist =aligned_list->pagelist;

	//    dprintf("\nDEBUG nand : pageid =%d ; g_startpage =%d ; g_pagecount =%d \n",templist->startPageID,g_startpage,g_pagecount);
	if((templist->startPageID < 0) || (templist->startPageID > g_pagecount))  //judge startPageID
	{
		templist->retVal =-1;
		printk("DEBUG: %s [%d] ret = %d \n ",__func__,__LINE__,ret);
		return ENAND;
	}
	p_pageid =get_physical_addr(templist->startPageID+g_startpage);
	do_select_chip(host,p_pageid);

	send_prog_page(templist->OffsetBytes,p_pageid);
	ret =data_process(host,templist,temp,WRITE);
	if(ret){
		printk("DEBUG: %s [%d] ret = %d \n ",__func__,__LINE__,ret);
		goto nand_write_1p_page_error;
	}
	nand_write_page_oob();
	send_prog_confirm();
	ret =send_read_status(&state);

	if(ret == 0)
		ret =(state & NAND_STATUS_FAIL ? IO_ERROR : SUCCESS);
	if(ret < 0){
		printk("DEBUG: %s [%d] ret = %d \n ",__func__,__LINE__,ret);
		aligned_list->pagelist->retVal =ret;
	}
nand_write_1p_page_error:
	g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);   //reset poobbuf
	do_deselect_chip(host);
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

	//  first page
	temp = aligned_list->opsmodel & 0x00ffffff; //23~0 : the number of pagelist
	templist =aligned_list->pagelist;
//	printk("   go into nand_write_2p_page_real ^^^^^^^^^^^^^^^\n");
	if((templist->startPageID < 0) || (templist->startPageID > g_pagecount))  //judge startPageID
	{
		templist->retVal =-1;
		return ENAND;
	}
	p_pageid =get_physical_addr(templist->startPageID+g_startpage);
	do_select_chip(host,p_pageid);

	send_prog_2p_page1(templist->OffsetBytes,p_pageid);   // first page
	ret =data_process(host,templist,temp,WRITE);
	if(ret < 0){
		printk("DEBUG: %s [%d] ret = %d \n ",__func__,__LINE__,ret);
		goto nand_write_2p_page_error;
	}
	nand_write_page_oob();
	send_prog_2p_confirm1();

	//  second page
	templist =aligned_list->next->pagelist;
	if((templist->startPageID < 0) || (templist->startPageID > g_pagecount))  //judge startPageID
	{
		templist->retVal =-1;
		ret =  ENAND;
		goto nand_write_2p_page_error;
	}
	temp = aligned_list->next->opsmodel & 0x00ffffff;     //second page

	send_prog_2p_page2(templist->OffsetBytes,p_pageid+g_pnand_chip->ppblock);
	ret =data_process(host,templist,temp,WRITE);
	if(ret < 0){
		printk("DEBUG: %s [%d] ret = %d \n ",__func__,__LINE__,ret);
		goto nand_write_2p_page_error;
	}
	nand_write_page_oob();
	send_prog_2p_confirm2();
	ret =send_read_status(&state);
	do_deselect_chip(host);
	//	dprintf("DEBUG %s: State 0x%02X\n", __func__, state);
	if(ret == 0)
		ret =(state & NAND_STATUS_FAIL ? IO_ERROR : SUCCESS);
	if(ret < 0){
		printk("DEBUG: %s [%d] ret = %d \n ",__func__,__LINE__,ret);
		aligned_list->pagelist->retVal =ret;
	}
nand_write_2p_page_error:
	g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);   //reset poobbuf
	do_deselect_chip(host);
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
	if(g_pnand_pt->use_planes)
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
	if(ret < 0){
		printk("%s  ret = %d\n",__func__,ret);
		return ret;        //return  timeout error
	}
	ret =nand_read_page_data(host,offset,bytes,(unsigned char *)databuf);
	if(ret < 0)
		printk("%s  ret = %d\n",__func__,ret);
	g_pnand_ecc->free_bch_buffer(g_oobsize+g_freesize);   //reset poobbuf
	do_deselect_chip(host);
	return (ret < 0) ? ret : bytes;           //return Uncorrectable error or success
}
/******************************************************
 *   nand_read_pages ;
 */
int nand_read_pages(NAND_BASE *host,Aligned_List *aligned_list)
{
	unsigned int temp;
	int ret,i,j;
	int flag = 0;
	Aligned_List *templist =aligned_list;
	i=j=0;
	while(templist)
	{
		temp = templist->opsmodel & (0xff<<24);  // 31~24 : operation mode
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
		if(ret < 0){
			printk("%s  ret = %d\n",__func__,ret);
			return ret;
		}
		if(ret > 0)
			flag = 1;
	}
	return flag?1:SUCCESS;
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
	if(state & NAND_STATUS_FAIL)
		printk("%s  state = %d\n",__func__,state);
	return state & NAND_STATUS_FAIL ? IO_ERROR : bytes;
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
//		printk(" nand write pages :  opsmodel = 0x%x ;temp =0x%x \n",templist->opsmodel,temp);
		if(temp){
			ret =nand_write_2p_page(host,templist);
			templist = templist->next->next;
		}
		else{
			ret =nand_write_1p_page(host,templist);
			templist =templist->next;
		}
		if(ret){
			printk("%s ret = %d\n",__func__,ret);
			return ret;
		}
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
	int count = 0, errflag = 0;
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
				templist->retVal = state;
				if (state < 0){
					printk("DEBUG: %s [%d] phy_blockid = %d state = %d \n ",__func__,__LINE__,
														(startblock * g_pnand_chip->planenum +g_startblock),state);
					errflag = 1;
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
				templist->retVal = state;
				if (state < 0){
					printk("DEBUG: %s [%d] phy_blockid = %d state = %d \n ",__func__,__LINE__,startblock+g_startblock,state);
					errflag = 1;
				}
				startblock++;
			}
			listhead = (templist->head).next;
			if(!listhead)
				break;
			templist = singlelist_entry(listhead,BlockList,head);
		}
	}
	return errflag ? ENAND : SUCCESS ;
}
/******************************************************
 *   isbadblock ;
 *   return value : 0 -- valid block , -1 -- invalid block
 */
int isbadblock(NAND_BASE *host, int blockid)          //按cpu方式读写
{
	unsigned char state[4][NAND_ECC_POS];  // there are four page per badblock,which stores badblock message
	int ret =0;
	int i=0,j=0,k=0;
	unsigned int pageid=0;
	unsigned char times =g_pnand_pt->use_planes+1;  // one-plane ,times =1 ;two-plane ,times =2
	int bit0_num = 0;
	memset(state[0],0xff,4*NAND_ECC_POS);
	pageid =g_startpage + blockid * g_pageperblock;
	while(times--){
		do_select_chip(host,pageid);
		while(i<4){
			send_read_page(g_pagesize+g_pnand_chip->badblockpos,(pageid +i));
			ret =g_pnand_io->read_data_withrb(state[i], NAND_ECC_POS);
			if(ret){
				printk("DEBUG: %s [%d] ret = %d \n ",__func__,__LINE__,ret);
				return ret;  // nand io_error
			}
		/* cale the number of the bit is 0 per 128bits in state */
			for(j=0; j <NAND_ECC_POS; j++)
			{
				if(state[i][j] != 0xff)
					for(k=0;k<8;k++)
						if(!(0x01 & (state[i][j]>>k)))
							bit0_num++;
			}
			i++;
		}
		do_deselect_chip(host);
		/* the block is badblock if the number of the bit ,which is 0,is more than 64 */
		if(bit0_num > 4*4*NAND_ECC_POS) 
			return ENAND;
		bit0_num = 0;
		if(times){
			pageid +=g_pnand_chip->ppblock;
			i=0;
		}
	}
	return SUCCESS;
}
/******************************************************
 *   markbadblock ;
 *
 */
int markbadblock(NAND_BASE *host, int blockid)
{
	unsigned char state=0;
	unsigned char bbm[NAND_ECC_POS] ={0x00}; // bad block message
	int ret=0;
	int i=0;
	unsigned int pageid=0;
	unsigned char times =g_pnand_pt->use_planes+1;  // one-plane ,times =1 ;two-plane ,times =2
	pageid =g_startpage + blockid * g_pageperblock;
	while(times--){
		do_select_chip(host,pageid);
		while(i<4){
			send_prog_page(g_pagesize+g_pnand_chip->badblockpos,(pageid + i));
			g_pnand_io->write_data_norb(bbm, NAND_ECC_POS);
			send_prog_confirm();
			ret =send_read_status(&state);
			state = (state & NAND_STATUS_FAIL) ? IO_ERROR : SUCCESS;
			if(ret || state){
				printk("DEBUG: %s [%d] ret = %d state %d \n ",__func__,__LINE__,ret,state);
				return ret;  // nand io_error
			}
			i++;
		}
		do_deselect_chip(host);
		if(times){
			pageid +=g_pnand_chip->ppblock;
			i=0;
		}
	}
	return state;
}

/********************************************************
 *              spl opreation for nand                  *
 ********************************************************/
static int spl_write_nand(NAND_BASE *host, Aligned_List *list, unsigned char *bchbuf, int bchsize, int numblock)
{
	PageList *pagelist = NULL;
	struct singlelist *listhead = NULL;
	unsigned char *tmpbchbuf = NULL;
	unsigned int i, j, opsmodel, pageid, steps;
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
	for (i = 0; i < opsmodel; i++) {
		if(pagelist->Bytes == 0 || (pagelist->Bytes + pagelist->OffsetBytes) > g_pnand_chip->pagesize) {
			pagelist->retVal = -1;
			ret = ENAND;
			dprintf("\n\n**********spl_write_nand, pagelist bytes or offsetbyte wrong: ret =%d************\n",ret);
			goto spl_write_err2;
		}
		steps = (pagelist->Bytes / SPL_BCH_BLOCK);
		tmpbchbuf = bchbuf + (pagelist->OffsetBytes / SPL_BCH_BLOCK) * SPL_BCH_SIZE;
		g_pnand_ecc->ecc_enable_encode(host, pagelist->pData, tmpbchbuf, SPL_BCH_BIT, steps);
		for (j = 0; j < steps; j++) {
			if (!(pageid == (g_startpage + g_pnand_chip->ppblock * numblock) && pagelist->OffsetBytes == 0 && j == 0))
				pn_enable(host);
			g_pnand_io->write_data_norb(pagelist->pData + SPL_BCH_BLOCK * j, SPL_BCH_BLOCK);
			if (!(pageid == (g_startpage + g_pnand_chip->ppblock * numblock) && pagelist->OffsetBytes == 0 && j == 0))
				pn_disable(host);
			if (j < steps - 1)
				send_prog_random(pagelist->OffsetBytes + SPL_BCH_BLOCK * (j + 1));
		}
		listhead = (pagelist->head).next;
		pagelist = singlelist_entry(listhead,PageList,head);
		if (i < opsmodel - 1) {
			send_prog_random(pagelist->OffsetBytes);
		} else {
			send_prog_confirm();
			ret = send_read_status(&status);
		}
	}
	pagelist = list->pagelist;
	if(ret == 0) {
		ret = (status & NAND_STATUS_FAIL ? IO_ERROR : SUCCESS);
	} else if(ret < 0) {
		pagelist->retVal = ret;
		dprintf("\n\n**********spl_write_nand, write data IO_ERROR: ret =%d************\n",ret);
		goto spl_write_err2;
	}

	send_prog_page(0, pageid + 1);
	pn_enable(host);
	g_pnand_io->write_data_norb(bchbuf, bchsize);
	pn_disable(host);
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

//int mmmm= 0;
static int spl_read_nand(NAND_BASE *host, Aligned_List *list, int times, unsigned char *bchbuf, int bchsize)
{
	PageList *pagelist = NULL;
	struct singlelist *listhead = NULL;
	unsigned char *tmpbchbuf = NULL;
	unsigned int pageid, opsmodel, steps, i, j;
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
	pn_enable(host);
	ret =g_pnand_io->read_data_withrb(bchbuf, bchsize);
	pn_disable(host);

	if(ret){
		dprintf("\n\n**********spl_read_nand, read ecc error: ret =%d************\n",ret);
		pagelist->retVal = ret;
		goto spl_read_err2;
	}

	opsmodel = list->opsmodel & 0x00ffffff;
	send_read_page(pagelist->OffsetBytes, pageid);
	for (i=0; i < opsmodel; i++) {

		if(pagelist->Bytes == 0 || (pagelist->Bytes + pagelist->OffsetBytes) > g_pnand_chip->pagesize) {
			pagelist->retVal = -1;
			ret = ENAND;
			dprintf("\n\n**********spl_read_nand, bytes or offsetbyte error: ret =%d************\n",ret);
			goto spl_read_err2;
		}
		steps = (pagelist->Bytes / SPL_BCH_BLOCK);
		tmpbchbuf = bchbuf + (pagelist->OffsetBytes / SPL_BCH_BLOCK) * SPL_BCH_SIZE;

		for (j = 0; j < steps; j++) {

			if (!(pageid == (g_startpage + times) && pagelist->OffsetBytes == 0 && j == 0))
				pn_enable(host);

			if(i == 0 && j == 0) {
				g_pnand_io->read_data_withrb((unsigned char *)pagelist->pData + SPL_BCH_BLOCK * j, SPL_BCH_BLOCK);
				/* test spl read first and second block error and the third block right*/
				/*			*((unsigned char *)pagelist->pData + 10) = 0xef;
							if(mmmm % 2 && mmmm < 5)
							{
							memset((unsigned char *)pagelist->pData, 0xff,10);
							}
							mmmm++;
				*/
			}
			else
				g_pnand_io->read_data_norb((unsigned char *)pagelist->pData + SPL_BCH_BLOCK * j, SPL_BCH_BLOCK);

			if (!(pageid == (g_startpage + times) && pagelist->OffsetBytes == 0 && j == 0))
				pn_disable(host);
			if (j < steps - 1)
				send_read_random(pagelist->OffsetBytes + SPL_BCH_BLOCK * (j + 1));
		}

		ret = g_pnand_ecc->ecc_enable_decode(host, (unsigned char *)pagelist->pData, tmpbchbuf, SPL_BCH_BIT, steps);
		if(ret < 0) {
			pagelist->retVal = ret;
			dprintf("\n\n**********spl_read_nand, data ecc error: ret =%d************\n",ret);
			goto spl_read_err2;
		} else if(ret >= SPL_BCH_BIT - 1) {
			pagelist->retVal = pagelist->Bytes | ( 1 << 16);
			printk("$$$$$$$$$$$$$$$$$$$$$   ecc error bits is %d $$$$$$$$$$$$$$$$$$$$$\n",ret);
		} else {
			printk("#####################   ecc error bits is %d #####################\n",ret);
			pagelist->retVal = pagelist->Bytes;
		}

		//dump_buf((unsigned char *)pagelist->pData+512,512);
		//printk("@@@@@@@@@@@@@@@@@@@@@@   pagelist->retVal is 0x%08x  @@@@@@@@@@@@@@@@@@@@@@@@@@\n",pagelist->retVal);
		listhead = (pagelist->head).next;
		pagelist = singlelist_entry(listhead,PageList,head);
		if (i < opsmodel - 1)
			send_read_random(pagelist->OffsetBytes);
	}
spl_read_err2:
	do_deselect_chip(host);
spl_read_err1:
	g_pnand_ecc->eccsize = tmpeccsize;
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
			/* block 0 write spl, block 1 bakup block 0, X_BOOT_BLOCK is block 2,
			 which block start write x-boot*/
			for(i=0; i < X_BOOT_BLOCK; i++) {
				ret = spl_write_nand(host, alignelist, bchbuf, bchsize, i);

				if(ret < 0) {
					dprintf("spl write error\n");
				}
			}
		} else {
			ret = nand_write_1p_page(host, alignelist);
			if(ret < 0) {
				dprintf("boot write error\n");
				kfree(bchbuf);
				return ret;
			}
		}
		alignelist = alignelist->next;
	}
	if(ret == 0) {
		dprintf("spl write success\n");
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
				dprintf("^^^^^^^^^ boot read error ^^^^^^^^^^^\n");
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
