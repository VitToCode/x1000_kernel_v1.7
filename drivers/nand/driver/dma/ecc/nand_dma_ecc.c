/*
 *  lib_nand/nand_ecc.c
 *
 *  ECC utils.
 */

#include "nand_api.h"

extern JZ_NAND_BDMA  nand_bdma;
extern JZ_ECC jz_default_ecc;
static JZ_ECC *pnand_ecc;


static inline unsigned int *get_ecc_code_buffer(unsigned int len)   //*cacl by byte*/
{
	return (unsigned int *)nand_bdma.get_dma_ecc_buf(len);
}

static inline void *get_pdma_databuf_pointer(void)
{
	return nand_bdma.get_relevant_pointer();
}

/**
 * bch_decode_enable -
 * @eccsize:	Data bytes per ECC step
 * @parsize:	ECC bytes per ECC step
 */
static inline void bch_decode_enable(int eccsize, int parsize)
{
	__ecc_enable(BCH_DECODE, pnand_ecc->eccbit);
	__ecc_cnt_dec(eccsize * 2 + parsize);

#if USE_BCH_AC	/* use bch auto correction */
	unsigned int tmp =bch_readl(BCH_CNT);
	tmp |= (eccsize * 2) << BCH_CNT_ENC_BIT;
	bch_writel(BCH_CNT,tmp);
#endif
}

/**
  bch_encode_enable -
 * @eccsize:	Data bytes per ECC step
 */
static inline void bch_encode_enable(int eccsize)
{
	__ecc_enable(BCH_ENCODE, pnand_ecc->eccbit);
	__ecc_cnt_enc(eccsize * 2);
}

/**
 * bch_correct
 * @dat:        data to be corrected
 * @idx:        the index of error bit in an eccsize
 */
static inline void bch_correct(unsigned char *dat, int idx)
{
	int eccsize = pnand_ecc->eccsize;
	int i, bit;		/* the 'bit' of i byte is error */


	i = (idx - 1) >> 3;
	bit = (idx - 1) & 0x7;

	dprintf("error:i=%d, bit=%d\n",i,bit);

	if (i < eccsize){
		dat[i] ^= (1 << bit);
	}
}

/**
 * bch_correct_dma
 * @dat:        data to be corrected
 * @errs0:      pointer to the dma target buffer of bch decoding which stores BHINTS and
 *              BHERR0~3(8-bit BCH) or BHERR0~1(4-bit BCH)
 */
static inline int bch_correct_dma(unsigned char *dat, unsigned int *errs0)
{
	unsigned int stat, i;
	unsigned int *errs = errs0;
	int ret = 0;

	stat = errs[0];
	if (stat & BCH_INTS_ERR) {
		dprintf("DEBUG BCH: stat=%x err0:%x err1:%x \n", stat, errs[1], errs[2]);
		if (stat & BCH_INTS_UNCOR) {
			dprintf("NAND: Uncorrectable ECC error\n");
			return -1;
		} else {
			unsigned int errcnt = (stat & BCH_INTS_ERRC_MASK) >> BCH_INTS_ERRC_BIT;
			ret = errcnt;

			/*begin at the second DWORD*/
			errs = (unsigned int *)&errs0[4];
			for (i = 0; i < errcnt; i++)
			{
				/* errs[i>>1]:	get the error report regester value.
				 * (i+1):	the error bit index.
				 * errs[i>>1] >> (((i + 1) % 2) << 4):	means when error.
				 * errs[i>>1] >> 16:	bit index is even.
				 */
				bch_correct(dat, ((errs[i >> 1] >> ((i % 2) << 4))) & BCH_ERR_INDEX_MASK);
			}
		}
	}
	return ret;
}



/**
 * bch_enable -
 */
static inline void bch_enable(unsigned int mode)
{
	unsigned int eccsize = pnand_ecc->eccsize;
	unsigned int parsize = pnand_ecc->parsize;

	//	dprintf("==>%s: L%d, eccsize=%d\n", __func__, __LINE__, eccsize);

	bch_writel(BCH_INTS,0xffffffff);

	if (mode == BCH_DECODE)
		bch_decode_enable(eccsize, parsize);

	if (mode == BCH_ENCODE)
		bch_encode_enable(eccsize);

	__ecc_dma_enable();  // bch dma model
}
/**
 * bch_disable -
 */
static inline void bch_disable(void)
{
	__bch_disable();
}
static inline int dma_ecc_encode_stage1(unsigned char *sourcebuf, unsigned char *targetbuf)     
{
	unsigned int len = pnand_ecc->eccsize;
	int ret=0;

	dma_cache_wback((u32)sourcebuf,len);
	dma_cache_wback((u32)targetbuf,len);

	REG_BDMAC_DCMD(MEMCPY_DMA_CHAN) = BDMAC_DCMD_SAI | BDMAC_DCMD_DAI | BDMAC_DCMD_SWDH_32 | BDMAC_DCMD_DWDH_32 | BDMAC_DCMD_DS_64BYTE ;  /* DMA command */
	REG_BDMAC_DSAR(MEMCPY_DMA_CHAN) = CPHYSADDR((unsigned int)sourcebuf);   /* DMA source address */
	REG_BDMAC_DTAR(MEMCPY_DMA_CHAN) =	CPHYSADDR((unsigned int)targetbuf);    /* DMA target address*/
	REG_BDMAC_DTCR(MEMCPY_DMA_CHAN) = len/64;                 /* DMA transfer count */
	REG_BDMAC_DRSR(MEMCPY_DMA_CHAN) = BDMAC_DRSR_RS_AUTO;     /* DMA request source */

	REG_BDMAC_DCCSR(MEMCPY_DMA_CHAN) &= ~(BDMAC_DCCSR_EN | BDMAC_DCCSR_HLT | BDMAC_DCCSR_TT | BDMAC_DCCSR_AR);
	REG_BDMAC_DCCSR(MEMCPY_DMA_CHAN)|= BDMAC_DCCSR_NDES; /* DMA channel control/status */

	REG_BDMAC_DMACR &= ~(BDMAC_DMACR_AR |BDMAC_DMACR_HLT);    /* DMA control register */

	//	dump_jz_bdma_channel(MEMCPY_DMA_CHAN);

	nand_bdma.nand_dma_start(MEMCPY_DMA_CHAN);     //start bdma channel 2	

	//	dump_jz_bdma_channel(MEMCPY_DMA_CHAN);

	while((ret =nand_bdma.nand_dma_finish(MEMCPY_DMA_CHAN)) == DMA_RUNNING);   // wait for bdma channel 2 finish  
	return ret;
}

static inline int dma_ecc_encode_stage2(unsigned char *sourcebuf, unsigned char *eccbuf)
{
	unsigned int len = pnand_ecc->eccsize;
	unsigned int eccbytes = pnand_ecc->eccbytes;
	int ret=0;
	dma_cache_wback((u32)sourcebuf,len);
	dma_cache_wback((u32)eccbuf,eccbytes);
	REG_BDMAC_DCMD(BCH_DMA_CHAN) = BDMAC_DCMD_SAI | BDMAC_DCMD_DAI | BDMAC_DCMD_SWDH_32 | BDMAC_DCMD_DWDH_8 |
		BDMAC_DCMD_DS_64BYTE ;  /* DMA command */
	REG_BDMAC_DSAR(BCH_DMA_CHAN) = CPHYSADDR((unsigned int)sourcebuf);   /* DMA source address */
	REG_BDMAC_DTAR(BCH_DMA_CHAN) =	CPHYSADDR((unsigned int)eccbuf);    /* DMA target address*/
	REG_BDMAC_DSD(BCH_DMA_CHAN) =0;                        /* DMA Stride Address */
	REG_BDMAC_DTCR(BCH_DMA_CHAN) = len/64;                 /* DMA transfer count */
	REG_BDMAC_DRSR(BCH_DMA_CHAN) = BDMAC_DRSR_RS_BCH_ENC;     /* DMA request source */
	//	REG_BDMAC_DNT(BCH_DMA_CHAN) = BDMAC_NDTCTIMER_EN | BDMAC_TAILCNT_BIT  //set dma nand timer 

	REG_BDMAC_DCCSR(BCH_DMA_CHAN) &= ~(BDMAC_DCCSR_EN | BDMAC_DCCSR_HLT | BDMAC_DCCSR_TT | BDMAC_DCCSR_AR); 
	REG_BDMAC_DCCSR(BCH_DMA_CHAN)|= BDMAC_DCCSR_NDES; /* DMA channel control/status */

	REG_BDMAC_DMACR &= ~(BDMAC_DMACR_AR |BDMAC_DMACR_HLT);    /* DMA control register */

	bch_enable(BCH_ENCODE);
	nand_bdma.nand_dma_start(BCH_DMA_CHAN);     //start bdma channel 0	
	while((ret =nand_bdma.nand_dma_finish(BCH_DMA_CHAN)) == DMA_RUNNING);   // wait for bdma channel 1 finish
	//		dprintf("DEBUG nand : dma_ecc_encode_stage2  ret = %d\n",ret);
	bch_disable();
	return ret;
}
/*
 * dma_ecc_decode_stage1 -- bch decode  and correct data
 * @sourcebuf : databuf address,
 * @eccbuf    : eccbuf
 * @errsbuf   : store BHINT and BCHERRn
 */

static inline int dma_ecc_decode_stage1(unsigned char *sourcebuf, unsigned char *eccbuf, unsigned int *errsbuf)
{
	jz_bdma_desc_8word desc_data,desc_ecc;
	int ret=0;
	unsigned int eccsize = pnand_ecc->eccsize;
	unsigned int eccbytes = pnand_ecc->eccbytes;

	dma_cache_wback((u32)sourcebuf,eccsize);
	dma_cache_wback((u32)eccbuf,eccbytes);
	dma_cache_wback((u32)errsbuf,pnand_ecc->errssize);

	desc_data.dcmd = BDMAC_DCMD_SAI | BDMAC_DCMD_DAI | BDMAC_DCMD_SWDH_32 | BDMAC_DCMD_DWDH_32 | BDMAC_DCMD_DS_64BYTE |
		BDMAC_DCMD_LINK;  // DMA command 
	desc_data.dsadr = CPHYSADDR((unsigned int)sourcebuf);   // DMA source address 
	desc_data.dtadr = CPHYSADDR((unsigned int)errsbuf);    // DMA target address 
	desc_data.dstrd =0;                        // DMA Stride Address 
	desc_data.dcnt = eccsize/64;                 // DMA transfer count 
	desc_data.dreqt = BDMAC_DRSR_RS_BCH_DEC;     // DMA request source
	desc_data.dnt =0;
	desc_data.ddadr =CPHYSADDR((unsigned int)&desc_ecc);

	desc_ecc.dcmd = BDMAC_DCMD_BLAST | BDMAC_DCMD_SAI | BDMAC_DCMD_DAI | BDMAC_DCMD_SWDH_8 | BDMAC_DCMD_DWDH_8 | 
		BDMAC_DCMD_DS_8BIT ;  // DMA command 
	desc_ecc.dsadr = CPHYSADDR((unsigned int)eccbuf);   // DMA source address 
	desc_ecc.dtadr = CPHYSADDR((unsigned int)errsbuf);    // DMA target address 
	desc_ecc.dstrd =0;                        // DMA Stride Address 
	desc_ecc.dcnt = eccbytes;                 // DMA transfer count 
	desc_ecc.dreqt = BDMAC_DRSR_RS_BCH_DEC;     // DMA request source
	desc_ecc.dnt =0;
	desc_ecc.ddadr =0;
	dma_cache_wback_inv((u32)&desc_data,32);
	dma_cache_wback_inv((u32)&desc_ecc,32);


	REG_BDMAC_DCCSR(BCH_DMA_CHAN) =0;	
	REG_BDMAC_DCCSR(BCH_DMA_CHAN)|= BDMAC_DCCSR_DES8 | BDMAC_DCCSR_LASTMD1; // DMA channel control/status 
	REG_BDMAC_DMACR &= ~(BDMAC_DMACR_AR |BDMAC_DMACR_HLT);    // DMA control register 
	REG_BDMAC_DDA(BCH_DMA_CHAN)=CPHYSADDR((unsigned int)&desc_data);
	REG_BDMAC_DMADBSR = BDMAC_DMADBSR_DBS0;

	bch_enable(BCH_DECODE);
	//	dprintf("DEBUG nand : dma_ecc_decode_stage1  REG_BDMAC_DCCSR(BCH_DMA_CHAN)=0x%x\n",REG_BDMAC_DCCSR(BCH_DMA_CHAN));
	nand_bdma.nand_dma_start(BCH_DMA_CHAN);     //start bdma channel 0	

	while((ret =nand_bdma.nand_dma_finish(BCH_DMA_CHAN)) == DMA_RUNNING);   // wait for bdma channel 0 finish
	//			dprintf("DEBUG nand : dma_ecc_decode_stage1  1: ret = %d, channel0 DCCSR =0x%x, eccbit =0x%x, DTCR =%d\n",ret,(int)REG_BDMAC_DCCSR(BCH_DMA_CHAN),pnand_ecc->eccbit,REG_BDMAC_DTCR(BCH_DMA_CHAN));
	bch_disable();
	if(ret)
		return ret;
	return bch_correct_dma(sourcebuf,errsbuf);  //return errs number
}


/*
 * dma_ecc_decode_stage2 -- bch decode  and correct data
 * @sourcebuf    : the buffer,which pdma provided  
 * @targetbuf    : the buffer,which mtd provided
 */
static inline int dma_ecc_decode_stage2(unsigned char *sourcebuf, unsigned char *targetbuf)
{
	int ret=0;
	unsigned int len = pnand_ecc->eccsize;
	dma_cache_wback((u32)sourcebuf,len);
	dma_cache_wback((u32)targetbuf,len);

	REG_BDMAC_DCCSR(MEMCPY_DMA_CHAN) &= ~(BDMAC_DCCSR_EN | BDMAC_DCCSR_HLT | BDMAC_DCCSR_TT | BDMAC_DCCSR_AR);	
	REG_BDMAC_DCCSR(MEMCPY_DMA_CHAN)|= BDMAC_DCCSR_NDES; /* DMA channel control/status */
	REG_BDMAC_DMACR &= ~(BDMAC_DMACR_AR |BDMAC_DMACR_HLT);    /* DMA control register */
	REG_BDMAC_DCMD(MEMCPY_DMA_CHAN) = BDMAC_DCMD_SAI | BDMAC_DCMD_DAI | BDMAC_DCMD_SWDH_32 | BDMAC_DCMD_DWDH_32 |
		BDMAC_DCMD_DS_64BYTE ;  /* DMA command */
	REG_BDMAC_DSAR(MEMCPY_DMA_CHAN) = CPHYSADDR((unsigned int)sourcebuf);   /* DMA source address */
	REG_BDMAC_DTAR(MEMCPY_DMA_CHAN) =	CPHYSADDR((unsigned int)targetbuf);    /* DMA target address */
	REG_BDMAC_DSD(MEMCPY_DMA_CHAN) =0;                        /* DMA Stride Address */
	REG_BDMAC_DTCR(MEMCPY_DMA_CHAN) = len/64;                 /* DMA transfer count */
	REG_BDMAC_DRSR(MEMCPY_DMA_CHAN) = BDMAC_DRSR_RS_AUTO;     /* DMA request source */

	//dump_jz_bdma_channel(MEMCPY_DMA_CHAN);
	nand_bdma.nand_dma_start(MEMCPY_DMA_CHAN);     //start bdma channel 2	
	while((ret =nand_bdma.nand_dma_finish(MEMCPY_DMA_CHAN)) == DMA_RUNNING);   // wait for bdma channel 2 finish
	return ret;	
}

static inline void free_bch_buffer(unsigned int len)  
{	
	nand_bdma.reset_bch_buffer(len);
}

/**
 * ecc_init -
 * 
 *
 */
static inline void ecc_init(void *nand_ecc, void *flash_type)
{
	dbg_line();

	NAND_FLASH_DEV *type = (NAND_FLASH_DEV *)flash_type;

	pnand_ecc = (JZ_ECC *)nand_ecc;

	if((type == 0) || (pnand_ecc == 0))
		eprintf("ecc_init: error!!type = 0x%x,pnand_ecc = 0x%x\n",(unsigned int)type,(unsigned int)pnand_ecc);

	pnand_ecc->get_ecc_code_buffer = get_ecc_code_buffer;
	pnand_ecc->dma_ecc_encode_stage1 = dma_ecc_encode_stage1;
	pnand_ecc->dma_ecc_encode_stage2 = dma_ecc_encode_stage2;
	pnand_ecc->dma_ecc_decode_stage1 = dma_ecc_decode_stage1;
	pnand_ecc->dma_ecc_decode_stage2 = dma_ecc_decode_stage2;
	pnand_ecc->get_pdma_databuf_pointer = get_pdma_databuf_pointer;
	pnand_ecc->free_bch_buffer = free_bch_buffer;

	pnand_ecc->ecc_get_eccbytes = jz_default_ecc.ecc_get_eccbytes;

	pnand_ecc->eccsize = type->eccblock;
	pnand_ecc->eccbit = type->eccbit;
	pnand_ecc->eccpos = NAND_ECC_POS;

	//	dprintf("\nDEBUG nand:jz_dma_ecc.c  ecc_init  pnand_ecc->ecc_get_eccbytes  0x%x  eccbit =%d \n",(int)pnand_ecc->ecc_get_eccbytes,type->eccbit);
	pnand_ecc->ecc_get_eccbytes(pnand_ecc);

}

JZ_ECC jz_nand_dma_ecc = 
{
	//	.ecc_get_eccbytes = jz_default_ecc.ecc_get_eccbytes,
	.ecc_init = ecc_init,
};
