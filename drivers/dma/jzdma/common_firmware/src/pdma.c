
/*
 * pdma.c
 */
#include <common.h>
#include <pdma.h>
#include <asm/jzsoc.h>

#define PDMA_BCH_DCMD (PDMAC_DCMD_SWDH_32 | PDMAC_DCMD_DWDH_32 | PDMAC_DCMD_TSZ_AUTO | PDMAC_DCMD_TIE | PDMAC_DCMD_RDIL_4BYTE)
#define PDMA_IO_DCMD (PDMAC_DCMD_SWDH_32 | PDMAC_DCMD_DWDH_32 | PDMAC_DCMD_TSZ_AUTO | PDMAC_DCMD_TIE | PDMAC_DCMD_RDIL_4BYTE)
#define PDMA_DDR_DCMD (PDMAC_DCMD_SAI | PDMAC_DCMD_DAI | PDMAC_DCMD_SWDH_32 | PDMAC_DCMD_DWDH_32 | PDMAC_DCMD_TSZ_AUTO | PDMAC_DCMD_TIE | PDMAC_DCMD_RDIL_4BYTE)

void io_channel_dmastart(unsigned char *source_addr, unsigned char *dest_addr,
		unsigned int trans_count, int mode)
{
	unsigned int dcmd = PDMA_IO_DCMD;
	unsigned int direction = mode & 0xffff;
	unsigned int wait = mode & DMA_WAIT_FINISH;

	/* the count must be multiple of 4bytes,because of RDIL = 4bytes.*/
	REG32(PDMAC_DCCS(PDMA_IO_CHANNEL)) = 0;
	REG32(PDMAC_DTC(PDMA_IO_CHANNEL)) = (((trans_count + 3) >> 2) << 2);
	REG32(PDMAC_DRT(PDMA_IO_CHANNEL)) = PDMAC_DRT_RT_AUTO;
	if(wait)
		dcmd &= ~PDMAC_DCMD_TIE;
	if (direction & NEMC_TO_TCSM) {
		REG32(PDMAC_DSA(PDMA_IO_CHANNEL)) = CPHYSADDR((u32)source_addr);
		REG32(PDMAC_DTA(PDMA_IO_CHANNEL)) = TPHYSADDR((u32)dest_addr);
		dcmd |= PDMAC_DCMD_SID_SPECIAL | PDMAC_DCMD_DID_TCSM | PDMAC_DCMD_DAI;
	} else { /* TCSM_TO_NEMC or TCSM_TO_NEMC_FILL */
		REG32(PDMAC_DSA(PDMA_IO_CHANNEL)) = TPHYSADDR((u32)source_addr);
		REG32(PDMAC_DTA(PDMA_IO_CHANNEL)) = CPHYSADDR((u32)dest_addr);
		if(direction & TCSM_TO_NEMC){ /* TCSM_TO_NEMC */
			dcmd |= PDMAC_DCMD_SID_TCSM | PDMAC_DCMD_DID_SPECIAL | PDMAC_DCMD_SAI;
		}else
			dcmd |= PDMAC_DCMD_SID_TCSM | PDMAC_DCMD_DID_SPECIAL;
	}
	REG32(PDMAC_DCMD(PDMA_IO_CHANNEL)) = dcmd;
	REG32(PDMAC_DCCS(PDMA_IO_CHANNEL)) = PDMAC_DCCS_NDES | PDMAC_DCCS_CTE;
	while(wait && !(REG32(PDMAC_DCCS(PDMA_IO_CHANNEL)) & PDMAC_DCCS_TT));
}

void bch_channel_dmastart(unsigned char *source_addr, unsigned char *dest_addr,
		unsigned int trans_count, int mode)
{
	unsigned int dcmd = PDMA_BCH_DCMD;
	unsigned int direction = mode & 0xffff;
	unsigned int wait = mode & DMA_WAIT_FINISH;
	/* the count must be multiple of 4bytes,because of RDIL = 4bytes.*/
	REG32(PDMAC_DCCS(PDMA_BCH_CHANNEL)) = 0;
	REG32(PDMAC_DTC(PDMA_BCH_CHANNEL)) = (((trans_count + 3) >> 2) << 2);
	REG32(PDMAC_DRT(PDMA_BCH_CHANNEL)) = PDMAC_DRT_RT_AUTO;
	if(wait)
		dcmd &= ~PDMAC_DCMD_TIE;
	if (direction & BCH_TO_TCSM) {
		REG32(PDMAC_DSA(PDMA_BCH_CHANNEL)) = CPHYSADDR((u32)source_addr);
		REG32(PDMAC_DTA(PDMA_BCH_CHANNEL)) = TPHYSADDR((u32)dest_addr);
		dcmd |= PDMAC_DCMD_SID_SPECIAL | PDMAC_DCMD_DID_TCSM | PDMAC_DCMD_SAI | PDMAC_DCMD_DAI;
	} else { /* TCSM_TO_BCH */
		REG32(PDMAC_DSA(PDMA_BCH_CHANNEL)) = TPHYSADDR((u32)source_addr);
		REG32(PDMAC_DTA(PDMA_BCH_CHANNEL)) = CPHYSADDR((u32)dest_addr);
		dcmd |= PDMAC_DCMD_SID_TCSM | PDMAC_DCMD_DID_SPECIAL | PDMAC_DCMD_SAI;
	}
	REG32(PDMAC_DCMD(PDMA_BCH_CHANNEL)) = dcmd;
	REG32(PDMAC_DCCS(PDMA_BCH_CHANNEL)) = PDMAC_DCCS_NDES | PDMAC_DCCS_CTE;
	while(wait && !(REG32(PDMAC_DCCS(PDMA_BCH_CHANNEL)) & PDMAC_DCCS_TT));
}

void ddr_channel_dmastart(unsigned char *source_addr, unsigned char *dest_addr,
				  unsigned int trans_count, int mode, int channel)
{
	/* the count must be multiple of 4bytes,because of RDIL = 4bytes.*/
	unsigned int dcmd = PDMA_DDR_DCMD;
	unsigned int direction = mode & 0xffff;
	REG32(PDMAC_DCCS(channel)) = 0;
	REG32(PDMAC_DTC(channel)) = (((trans_count + 3) >> 2) << 2);
	REG32(PDMAC_DRT(channel)) = PDMAC_DRT_RT_AUTO;
	if (direction & DDR_TO_TCSM) {
		REG32(PDMAC_DSA(channel)) = CPHYSADDR((u32)source_addr);
		REG32(PDMAC_DTA(channel)) = TPHYSADDR((u32)dest_addr);
	} else { /* TCSM_TO_DDR */
		REG32(PDMAC_DSA(channel)) = TPHYSADDR((u32)source_addr);
		REG32(PDMAC_DTA(channel)) = CPHYSADDR((u32)dest_addr);
	}
	REG32(PDMAC_DCMD(channel)) = dcmd;
	REG32(PDMAC_DCCS(channel)) = PDMAC_DCCS_NDES | PDMAC_DCCS_CTE;
}


void pdma_channel_init(void)
{
	/* Enable Special Channel 0 and Channel 1 */
	REG32(PDMAC_DMAC) |= PDMAC_DMAC_CH01;

	REG32(PDMAC_DCCS(PDMA_DDR_CHANNEL)) |= PDMAC_DCCS_NDES;
	REG32(PDMAC_DCCS(PDMA_MOVE_CHANNEL)) |= PDMAC_DCCS_NDES;

	REG32(PDMAC_DMACP) |= ((1 << PDMA_IO_CHANNEL) | (1 << PDMA_BCH_CHANNEL) |
			       (1 << PDMA_DDR_CHANNEL) | ( 1 << PDMA_MOVE_CHANNEL));
}
