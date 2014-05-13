/*
 * src/bch.c
 */
#include <common.h>
#include <nand.h>
#include <pdma.h>
#include <mcu.h>
#include <asm/jzsoc.h>

//#define BCH_DEBUG

static inline void bch_encode_enable(int ecclevel, int eccsize, int parsize)
{
	unsigned int bch_cnt,bch_cr;

	// __bch_ints_clear
	REG32(BCH_CRS) = BCH_CR_BCHE;
	REG32(BCH_INTS) = 0xffffffff;
	// __bch_cnt_set
	bch_cnt = REG32(BCH_CNT);
	bch_cnt &= ~(BCH_CNT_PARITY_MASK | BCH_CNT_BLOCK_MASK);
	bch_cnt |= (parsize << BCH_CNT_PARITY_BIT | eccsize << BCH_CNT_BLOCK_BIT);
	REG32(BCH_CNT) = bch_cnt;
	//__bch_encoding(ecclevel);
        bch_cr = BCH_CR_BSEL(ecclevel) | BCH_CR_ENCE | BCH_CR_BCHE | BCH_CR_MZSB_MASK(ecclevel) | BCH_CR_INIT;
	REG32(BCH_CR) = bch_cr;
}

static inline void bch_decode_enable(int ecclevel, int eccsize, int parsize)
{
	unsigned int bch_cnt,bch_cr;

	REG32(BCH_CRS) = BCH_CR_BCHE;
	// __bch_ints_clear
	REG32(BCH_INTS) = 0xffffffff;
        // __bch_cnt_set
	bch_cnt = REG32(BCH_CNT);
	bch_cnt &= ~(BCH_CNT_PARITY_MASK | BCH_CNT_BLOCK_MASK);
	bch_cnt |= (parsize << BCH_CNT_PARITY_BIT | eccsize << BCH_CNT_BLOCK_BIT);
	REG32(BCH_CNT) = bch_cnt;
	// __bch_decoding(ecclevel);
	bch_cr = BCH_CR_BSEL(ecclevel) | BCH_CR_DECE | BCH_CR_BCHE | BCH_CR_MZSB_MASK(ecclevel)| BCH_CR_INIT;
	REG32(BCH_CR) = bch_cr;
}

/* BCH ENCOGING CFG */
void pdma_bch_encode_prepare(NandChip *nand_info, PipeNode *pipe)
{
	bch_encode_enable(nand_info->ecclevel, nand_info->eccsize, nand_info->eccbytes);

	/* write DATA to BCH_DR */
#if 1
	bch_channel_dmastart(pipe->pipe_data, (unsigned char *)BCH_DR, nand_info->eccsize, TCSM_TO_BCH);
#else
	bch_channel_cfg(pipe->pipe_data, (unsigned char *)BCH_DR, nand_info->eccsize, TCSM_TO_BCH);
	__pdmac_channel_irq_disable(PDMA_BCH_CHANNEL);
	__pdmac_special_channel_launch(PDMA_BCH_CHANNEL);

	while (!__pdmac_channel_end_detected(PDMA_BCH_CHANNEL));
	__pdmac_channel_mirq_clear(PDMA_BCH_CHANNEL);
	__pdmac_channel_irq_enable(PDMA_BCH_CHANNEL);

	/* clear bch's register */
	REG32(BCH_INTS) = 0xffffffff;
	REG32(BCH_CRC) = BCH_CR_BCHE;
	/*{
		int i = 0;
		for(i = 0; i <1024; i++){
			*(volatile unsigned char *)(BCH_DR) = *(pipe->pipe_data + i);
		}
	}*/
#endif
}

/* BCH DECOGING CFG */
void pdma_bch_decode_prepare(NandChip *nand_info, PipeNode *pipe)
{
	bch_decode_enable(nand_info->ecclevel, nand_info->eccsize, nand_info->eccbytes);

	/* write DATA and PARITY to BCH_DR */
	bch_channel_dmastart(pipe->pipe_data, (unsigned char *)BCH_DR,nand_info->eccsize + nand_info->eccbytes, TCSM_TO_BCH);
}
//----------------------------------------------------------------------------/

/* BCH ENCOGING HANDLE */
void bch_encode_complete(NandChip *nand_info, PipeNode *pipe)
{
	/* wait for finishing encoding */
	while (REG32(PDMAC_DMCS) & PDMAC_DMCS_BCH_EF);
	/* get Parity to TCSM */
	bch_channel_dmastart((unsigned char *)BCH_PAR0, pipe->pipe_par, nand_info->eccbytes, BCH_TO_TCSM | DMA_WAIT_FINISH);
	/* clear bch's register */
	REG32(BCH_INTS) = 0xffffffff;
	REG32(BCH_CRC) = BCH_CR_BCHE;
}

/* BCH DECOGING HANDLE */
static void bch_error_correct(NandChip *nand, unsigned short *data_buf,
				unsigned int *err_buf, int err_bit)
{
	unsigned short err_mask;
	u32 idx; /* the 'bit' of idx half-word is error */

	idx = (err_buf[err_bit] & BCH_ERR_INDEX_MASK) >> BCH_ERR_INDEX_BIT;
	err_mask = (err_buf[err_bit] & BCH_ERR_MASK_MASK) >> BCH_ERR_MASK_BIT;

	data_buf[idx] ^= err_mask;
}

void bch_decode_complete(NandChip *nand, unsigned char *data_buf,unsigned char *err_buf, char *report)
{
	unsigned int stat;
	int i, err_cnt;
	/* wait for finishing decoding */
	while (REG32(PDMAC_DMCS) & PDMAC_DMCS_BCH_DF);

	/* get BCH Status */
	stat = REG32(BCH_INTS);

	*report = 0;

        if (stat & BCH_INTS_ALLf) {
                *report = MSG_RET_EMPTY; /* ECC ALL 'FF' */
        }else if (stat & BCH_INTS_UNCOR) {
		*report = MSG_RET_FAIL; /* Uncorrectable ECC Error*/
	} else if (stat & BCH_INTS_ERR) {
		err_cnt = (stat & BCH_INTS_TERRC_MASK) >> BCH_INTS_TERRC_BIT;
		if(err_cnt > nand->ecclevel / 3)
			*report = MSG_RET_MOVE; /* Move Block*/
		err_cnt = (stat & BCH_INTS_ERRC_MASK) >> BCH_INTS_ERRC_BIT;

		if (err_cnt) {
			/* read BCH Error Report use Special CH0 */
			bch_channel_dmastart((unsigned char *)BCH_ERR0, err_buf, err_cnt << 2, BCH_TO_TCSM | DMA_WAIT_FINISH);
			for (i = 0; i < err_cnt; i++)
				bch_error_correct(nand, (unsigned short *)data_buf, (unsigned int *)err_buf, i);
		}
	}
	/* clear bch's register */
	REG32(BCH_INTS) = 0xffffffff;
	REG32(BCH_CRC) = BCH_CR_BCHE;
}
