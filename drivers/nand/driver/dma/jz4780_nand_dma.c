/*
 * lib_nand/controller/io/nand_dma.c
 *
 * JZ4770 Nand Driver
 *
 * NAND BDMA utils.
 */

#include "nand_api.h"
#include "jz4770cpm.h"
volatile static LINKED_LIST  data_list[2];

static unsigned char *g_bank0_buf;    //store BHINT and BCHERRn
static unsigned int g_bank0_len;
static unsigned int g_bank0_offset = 0;  
static unsigned char *g_bank1_buf;   //store data buffer
static unsigned char *g_bank2_buf;   //store data buffer

static inline int nand_dma_init(JZ_ECC *pnand_ecc)
{
	unsigned int eccsize =pnand_ecc->eccsize;
	
	g_bank0_buf = (unsigned char *)nand_malloc_buf(2*1024);
	if(!g_bank0_buf)
		return ENAND;
	g_bank0_len = 2048;
	g_bank1_buf = (unsigned char *)nand_malloc_buf(1024);
	if(!g_bank1_buf)
		return ENAND;
	g_bank2_buf = (unsigned char *)nand_malloc_buf(1024);
	if(!g_bank2_buf)
		return ENAND;
	memset(g_bank0_buf,0xff,2048);
	memset(g_bank1_buf,0xff,1024);
	memset(g_bank2_buf,0xff,1024);
	
    cpm_start_clock(CGM_BDMA);
	REG_BDMAC_DMACKES = 0x07;
	
	data_list[0].pbuf =g_bank1_buf;
	data_list[0].perrs =g_bank1_buf+eccsize+100;
	data_list[0].mark =0;
	data_list[0].next =data_list+1;
	data_list[1].pbuf =g_bank2_buf;
	data_list[1].perrs =g_bank1_buf+eccsize+100;
	data_list[1].mark =0;
	data_list[1].next =data_list;
	
	return SUCCESS;	
}

static inline  void * get_relevant_pointer(void)
{
/*	void *ptemp =0;
	switch(mode)
	{
		case BANK0_POINTER :
		ptemp =(void *)g_bank0_buf;
		break;
		
		case DATA_POINTER_LIST :
		ptemp =(void *)data_list;
		break;
		
		default :
			break;
	}
*/	return (void *)data_list;
}

static inline unsigned char *get_dma_ecc_buf(unsigned int len)    /*cacl by byte*/
{
	unsigned char *ptemp;
	
	/*g_bank0_offset cacl by word, make sure ptemp can be divied by 4*/
	if ((g_bank0_offset >= g_bank0_len) || (g_bank0_offset + len >g_bank0_len))
	{
		dprintf("error: there isn't enough buffer for errs!\n");
		return 0;
	}
	
	ptemp = g_bank0_buf + g_bank0_offset;
	g_bank0_offset += len;
	
	return ptemp;
}

static inline void reset_bch_buffer(unsigned int len)  
{	
	memset(g_bank0_buf,0xFF,len);
//	g_bank0_offset =0;
}

static inline void nand_dma_start(unsigned int channel)
{
	if(channel == NAND_DMA_CHAN)
		REG_BDMAC_DCCSR(channel)|=BDMAC_DCCSR_FRBS(0);
	REG_BDMAC_DCCSR(channel) |=BDMAC_DCCSR_EN;
	REG_BDMAC_DMACR |= BDMAC_DMACR_DMAE;
}
static inline int nand_dma_finish(unsigned int channel)
{
	if((REG_BDMAC_DCCSR(channel) & (BDMAC_DCCSR_AR | BDMAC_DCCSR_TT)))
	{
		REG_BDMAC_DCCSR(channel) &= ~(BDMAC_DCCSR_EN);
		if(REG_BDMAC_DCCSR(channel) & BDMAC_DCCSR_TT)
			return DMA_TERMINATE;     //finish
		else
			return DMA_AR;     // address error
	}
	else
		return DMA_RUNNING;	      // no finish
}
static inline void nand_dma_deinit()
{
	nand_free_buf(g_bank0_buf);
	nand_free_buf(g_bank1_buf);
	nand_free_buf(g_bank2_buf);
	return ;
}
JZ_NAND_BDMA  nand_bdma ={
	.nand_dma_init = nand_dma_init,
	.nand_dma_deinit =nand_dma_deinit,
	.get_relevant_pointer = get_relevant_pointer,
	.get_dma_ecc_buf =get_dma_ecc_buf,
	.reset_bch_buffer = reset_bch_buffer,
	.nand_dma_start =nand_dma_start,
	.nand_dma_finish =nand_dma_finish,
};


