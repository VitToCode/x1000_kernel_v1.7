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
	__bch_ints_clear();
	__bch_cnt_set(eccsize, parsize);
	__bch_encoding(ecclevel);
}

static inline void bch_decode_enable(int ecclevel, int eccsize, int parsize)
{
	__bch_ints_clear();
	__bch_cnt_set(eccsize, parsize);
	__bch_decoding(ecclevel);
}

/* BCH ENCOGING CFG */
void pdma_bch_encode_prepare(NandChip *nand_info, PipeNode *pipe)
{
	bch_encode_enable(nand_info->ecclevel, nand_info->eccsize, nand_info->eccbytes);

	/* write DATA to BCH_DR */
#if 1
	bch_channel_cfg(pipe->pipe_data, (unsigned char *)BCH_DR, nand_info->eccsize, TCSM_TO_BCH);
	__pdmac_special_channel_launch(PDMA_BCH_CHANNEL);
#else	
	bch_channel_cfg(pipe->pipe_data, (unsigned char *)BCH_DR, nand_info->eccsize, TCSM_TO_BCH);
	__pdmac_channel_irq_disable(PDMA_BCH_CHANNEL);
	__pdmac_special_channel_launch(PDMA_BCH_CHANNEL);

	while (!__pdmac_channel_end_detected(PDMA_BCH_CHANNEL));
	__pdmac_channel_mirq_clear(PDMA_BCH_CHANNEL);
	__pdmac_channel_irq_enable(PDMA_BCH_CHANNEL);

	/* clear bch's register */
	__bch_encints_clear();
	__bch_disable();
/*	{
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
	bch_channel_cfg(pipe->pipe_data, (unsigned char *)BCH_DR,nand_info->eccsize + nand_info->eccbytes, TCSM_TO_BCH);
	__pdmac_special_channel_launch(PDMA_BCH_CHANNEL);
}
//----------------------------------------------------------------------------/

/* BCH ENCOGING HANDLE */
void bch_encode_complete(NandChip *nand_info, PipeNode *pipe)
{
    /* wait for finishing encoding */
	__mbch_encode_sync();
	/* get Parity to TCSM */
	bch_channel_cfg((unsigned char *)BCH_PAR0, pipe->pipe_par, nand_info->eccbytes, BCH_TO_TCSM);
	__pdmac_channel_irq_disable(PDMA_BCH_CHANNEL);
	__pdmac_special_channel_launch(PDMA_BCH_CHANNEL);

#ifdef BCH_DEBUG
	volatile int timeout = 4000; //for debug
	while (!__pdmac_channel_end_detected(PDMA_BCH_CHANNEL) && timeout--);
#else
	while (!__pdmac_channel_end_detected(PDMA_BCH_CHANNEL));
#endif
	__pdmac_channel_mirq_clear(PDMA_BCH_CHANNEL);
	__pdmac_channel_irq_enable(PDMA_BCH_CHANNEL);

	/* clear bch's register */
	__bch_encints_clear();
	__bch_disable();
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
	__mbch_decode_sync();
	/* get BCH Status */
	stat = REG_BCH_INTS;
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
			bch_channel_cfg((unsigned char *)BCH_ERR0, err_buf, err_cnt << 2, BCH_TO_TCSM);
			__pdmac_channel_irq_disable(PDMA_BCH_CHANNEL);
			__pdmac_special_channel_launch(PDMA_BCH_CHANNEL);

#ifdef BCH_DEBUG
			volatile int timeout = 4000; //for debug
			while (!__pdmac_channel_end_detected(PDMA_BCH_CHANNEL) && timeout--);
#else
			while (!__pdmac_channel_end_detected(PDMA_BCH_CHANNEL));
#endif
			__pdmac_channel_mirq_clear(PDMA_BCH_CHANNEL);
			__pdmac_channel_irq_enable(PDMA_BCH_CHANNEL);

			for (i = 0; i < err_cnt; i++)
				bch_error_correct(nand, (unsigned short *)data_buf, (unsigned int *)err_buf, i);
		}
	}
	/* clear bch's register */
	__bch_decints_clear();
	__bch_disable();
}

