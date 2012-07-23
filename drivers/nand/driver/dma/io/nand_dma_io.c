/*
 * lib_nand/nand_dma_io.c
 *
 * NAND I/O utils.
 */

#include "nand_api.h"

extern JZ_IO jz_nand_io;
static JZ_IO *pnand_io;
extern JZ_NAND_BDMA  nand_bdma;
/**
 * dma_write_data_norb - 
 * @buf:	data buffer
 * @count:	send size in byte
 */
static inline void dma_write_data_norb(void *buf, int len)     //bdma channel 1
{
	unsigned int *ptemp =(unsigned int *)buf;
    dma_cache_wback((u32)buf,len);
	REG_BDMAC_DCMD(NAND_DMA_CHAN) = BDMAC_DCMD_SAI | BDMAC_DCMD_SWDH_32 | BDMAC_DCMD_DWDH_32 | BDMAC_DCMD_DS_64BYTE ;  /* DMA command */
	REG_BDMAC_DSAR(NAND_DMA_CHAN) =CPHYSADDR((unsigned int)ptemp);   /* DMA source address */
	REG_BDMAC_DTAR(NAND_DMA_CHAN) =CPHYSADDR((unsigned int)pnand_io->dataport);	/* DMA target address: dataport */
	REG_BDMAC_DSD(NAND_DMA_CHAN) =0;                        /* DMA Stride Address */
	REG_BDMAC_DTCR(NAND_DMA_CHAN) = len/64;                 /* DMA transfer count */
	REG_BDMAC_DRSR(NAND_DMA_CHAN) = BDMAC_DRSR_RS_AUTO;     /* DMA request source */
	
	REG_BDMAC_DCCSR(NAND_DMA_CHAN) =0;
//	REG_BDMAC_DCCSR(NAND_DMA_CHAN) &= ~(BDMAC_DCCSR_EN | BDMAC_DCCSR_HLT | BDMAC_DCCSR_TT | BDMAC_DCCSR_AR);
	REG_BDMAC_DCCSR(NAND_DMA_CHAN)|= BDMAC_DCCSR_NDES; /* DMA channel control/status */
	REG_BDMAC_DMACR &= ~(BDMAC_DMACR_AR |BDMAC_DMACR_HLT);  /* DMA control register */

	nand_bdma.nand_dma_start(NAND_DMA_CHAN);     //start bdma channel 1
}

static inline void dma_read_data_norb(void *buf, int len)     //bdma channel 1
{
	unsigned int *ptemp =(unsigned int *)buf;
    dma_cache_wback((u32)buf,len);
	REG_BDMAC_DCMD(NAND_DMA_CHAN) = BDMAC_DCMD_DAI | BDMAC_DCMD_SWDH_32 | BDMAC_DCMD_DWDH_32 | BDMAC_DCMD_DS_64BYTE ;  /* DMA command */
	REG_BDMAC_DSAR(NAND_DMA_CHAN) = CPHYSADDR((unsigned int)pnand_io->dataport);   /* DMA source address */
	REG_BDMAC_DTAR(NAND_DMA_CHAN) =	CPHYSADDR((unsigned int)ptemp);    /* DMA target address: cmdport */
	REG_BDMAC_DSD(NAND_DMA_CHAN) =0;                        /* DMA Stride Address */
	REG_BDMAC_DTCR(NAND_DMA_CHAN) = len/64;                 /* DMA transfer count */
	REG_BDMAC_DRSR(NAND_DMA_CHAN) = BDMAC_DRSR_RS_AUTO;     /* DMA request source */
	REG_BDMAC_DNT(NAND_DMA_CHAN) = 0;  //set dma nand timer 

	REG_BDMAC_DCCSR(NAND_DMA_CHAN) =0;
//	REG_BDMAC_DCCSR(NAND_DMA_CHAN) &= ~(BDMAC_DCCSR_EN | BDMAC_DCCSR_HLT | BDMAC_DCCSR_TT | BDMAC_DCCSR_AR);
	REG_BDMAC_DCCSR(NAND_DMA_CHAN)|= BDMAC_DCCSR_NDES; /* DMA channel control/status */

	REG_BDMAC_DMACR &= ~(BDMAC_DMACR_AR |BDMAC_DMACR_HLT);  /* DMA control register */
	
//	dprintf("\nDEBUG nand:nand_dma_start 0x%x\n",(int)nand_bdma.nand_dma_start);
	nand_bdma.nand_dma_start(NAND_DMA_CHAN);     //start bdma channel 1
}

static inline void dma_read_data_withrb(void *buf, int len)     //bdma channel 1
{
	unsigned int *ptemp =(unsigned int *)buf;

    dma_cache_wback_inv((u32)buf,len);
	REG_BDMAC_DCMD(NAND_DMA_CHAN) = BDMAC_DCMD_DAI | BDMAC_DCMD_SWDH_32 | BDMAC_DCMD_DWDH_32 | BDMAC_DCMD_DS_8BIT ;  /* DMA command */
	REG_BDMAC_DSAR(NAND_DMA_CHAN) = CPHYSADDR((unsigned int)pnand_io->dataport);   /* DMA source address */
	REG_BDMAC_DTAR(NAND_DMA_CHAN) =	CPHYSADDR((unsigned int)ptemp);    /* DMA target address: cmdport */
	REG_BDMAC_DSD(NAND_DMA_CHAN) =0;                        /* DMA Stride Address */
	REG_BDMAC_DTCR(NAND_DMA_CHAN) = len;                 /* DMA transfer count */
	REG_BDMAC_DRSR(NAND_DMA_CHAN) = BDMAC_DRSR_RS_AUTO;     /* DMA request source */
//	REG_BDMAC_DNT(NAND_DMA_CHAN) = BDMAC_NDTCTIMER_EN | BDMAC_TAILCNT_BIT;  //set dma nand timer 

	REG_BDMAC_DCCSR(NAND_DMA_CHAN) =0;
//	REG_BDMAC_DCCSR(NAND_DMA_CHAN) &= ~(BDMAC_DCCSR_EN | BDMAC_DCCSR_HLT | BDMAC_DCCSR_TT | BDMAC_DCCSR_AR);	
	REG_BDMAC_DCCSR(NAND_DMA_CHAN)|= BDMAC_DCCSR_NDES; /* DMA channel control/status */
	REG_BDMAC_DMACR &= ~(BDMAC_DMACR_AR |BDMAC_DMACR_HLT);    /* DMA control register */

	nand_bdma.nand_dma_start(NAND_DMA_CHAN);     //start bdma channel 1
}

static inline int dma_channel1_finish(void)
{
	return nand_bdma.nand_dma_finish(NAND_DMA_CHAN);
}

static inline void dma_io_init(void *nand_io)
{
	pnand_io = (JZ_IO *)nand_io;
		
	pnand_io->io_init =jz_nand_io.io_init;
	pnand_io->send_cmd_norb = jz_nand_io.send_cmd_norb;
	pnand_io->send_cmd_withrb = jz_nand_io.send_cmd_withrb;
	pnand_io->send_addr = jz_nand_io.send_addr;
	pnand_io->verify_data = jz_nand_io.verify_data;
	pnand_io->wait_ready = jz_nand_io.wait_ready;
										
	pnand_io->io_init(nand_io);
}
JZ_IO jz_nand_dma_io ={
	.io_init = dma_io_init,
	.read_data_norb = dma_read_data_norb,
	.write_data_norb = dma_write_data_norb,
	.read_data_withrb = dma_read_data_withrb,
	.dma_nand_finish =dma_channel1_finish,
};
