
/*
 * pdma.c
 */
#include <common.h>
#include <pdma.h>
#include <asm/jzsoc.h>

__bank4 void nemc_channel_cfg(unsigned char *source_addr, unsigned char *dest_addr,
		unsigned int trans_count, int mode)
{
	/* the count must be multiple of 4bytes,because of RDIL = 4bytes.*/
	REG_PDMAC_DTC(PDMA_NEMC_CHANNEL) = (((trans_count + 3) >> 2) << 2);
	REG_PDMAC_DRT(PDMA_NEMC_CHANNEL) = PDMAC_DRT_RT_AUTO;

	if (mode & NEMC_TO_TCSM) {
		REG_PDMAC_DSA(PDMA_NEMC_CHANNEL) = CPHYSADDR((u32)source_addr);
		REG_PDMAC_DTA(PDMA_NEMC_CHANNEL) = TPHYSADDR((u32)dest_addr);

		REG_PDMAC_DCMD(PDMA_NEMC_CHANNEL) &= ~(PDMAC_DCMD_SID_MASK | PDMAC_DCMD_DID_MASK |
				PDMAC_DCMD_SAI | PDMAC_DCMD_DAI);
		REG_PDMAC_DCMD(PDMA_NEMC_CHANNEL) |= PDMAC_DCMD_SID_SPECIAL | PDMAC_DCMD_DID_TCSM |
			PDMAC_DCMD_DAI;
	} else { /* TCSM_TO_NEMC or TCSM_TO_NEMC_FILL */
		REG_PDMAC_DSA(PDMA_NEMC_CHANNEL) = TPHYSADDR((u32)source_addr);
		REG_PDMAC_DTA(PDMA_NEMC_CHANNEL) = CPHYSADDR((u32)dest_addr);

		REG_PDMAC_DCMD(PDMA_NEMC_CHANNEL) &= ~(PDMAC_DCMD_SID_MASK | PDMAC_DCMD_DID_MASK |
				PDMAC_DCMD_SAI | PDMAC_DCMD_DAI);
		if(mode & TCSM_TO_NEMC){ /* TCSM_TO_NEMC */
			REG_PDMAC_DCMD(PDMA_NEMC_CHANNEL) |= PDMAC_DCMD_SID_TCSM | PDMAC_DCMD_DID_SPECIAL |
				PDMAC_DCMD_SAI;
		}else
			REG_PDMAC_DCMD(PDMA_NEMC_CHANNEL) |= PDMAC_DCMD_SID_TCSM | PDMAC_DCMD_DID_SPECIAL;
	}
}

__bank4 void bch_channel_cfg(unsigned char *source_addr, unsigned char *dest_addr,
		unsigned int trans_count, int mode)
{
	/* the count must be multiple of 4bytes,because of RDIL = 4bytes.*/
	REG_PDMAC_DTC(PDMA_BCH_CHANNEL) = (((trans_count + 3) >> 2) << 2);
	REG_PDMAC_DRT(PDMA_BCH_CHANNEL) = PDMAC_DRT_RT_AUTO;

	if (mode & BCH_TO_TCSM) {
		REG_PDMAC_DSA(PDMA_BCH_CHANNEL) = CPHYSADDR((u32)source_addr);
		REG_PDMAC_DTA(PDMA_BCH_CHANNEL) = TPHYSADDR((u32)dest_addr);

		REG_PDMAC_DCMD(PDMA_BCH_CHANNEL) &= ~(PDMAC_DCMD_SID_MASK | PDMAC_DCMD_DID_MASK |
				PDMAC_DCMD_SAI | PDMAC_DCMD_DAI);
		REG_PDMAC_DCMD(PDMA_BCH_CHANNEL) |= PDMAC_DCMD_SID_SPECIAL | PDMAC_DCMD_DID_TCSM |
			PDMAC_DCMD_SAI | PDMAC_DCMD_DAI;
	} else { /* TCSM_TO_BCH */
		REG_PDMAC_DSA(PDMA_BCH_CHANNEL) = TPHYSADDR((u32)source_addr);
		REG_PDMAC_DTA(PDMA_BCH_CHANNEL) = CPHYSADDR((u32)dest_addr);

		REG_PDMAC_DCMD(PDMA_BCH_CHANNEL) &= ~(PDMAC_DCMD_SID_MASK | PDMAC_DCMD_DID_MASK |
				PDMAC_DCMD_SAI | PDMAC_DCMD_DAI);
		REG_PDMAC_DCMD(PDMA_BCH_CHANNEL) |= PDMAC_DCMD_SID_TCSM | PDMAC_DCMD_DID_SPECIAL |
			PDMAC_DCMD_SAI;
	}
}

__bank4 void ddr_channel_cfg(unsigned char *source_addr, unsigned char *dest_addr,
		unsigned int trans_count, int mode, int channel)
{
	/* the count must be multiple of 4bytes,because of RDIL = 4bytes.*/
	REG_PDMAC_DTC(channel) = (((trans_count + 3) >> 2) << 2);
	REG_PDMAC_DRT(channel) = PDMAC_DRT_RT_AUTO;

	if (mode & DDR_TO_TCSM) {
		REG_PDMAC_DSA(channel) = CPHYSADDR((u32)source_addr);
		REG_PDMAC_DTA(channel) = TPHYSADDR((u32)dest_addr);
	} else { /* TCSM_TO_DDR */
		REG_PDMAC_DSA(channel) = TPHYSADDR((u32)source_addr);
		REG_PDMAC_DTA(channel) = CPHYSADDR((u32)dest_addr);
	}
}

__bank5  void pdma_channel_init(void)
{
	/* Enable Special Channel 0 and Channel 1 */
	REG_PDMAC_DMAC |= PDMAC_DMAC_CH01;

	REG_PDMAC_DCCS(PDMA_DDR_CHANNEL) |= PDMAC_DCCS_NDES;
	REG_PDMAC_DCCS(PDMA_MOVE_CHANNEL) |= PDMAC_DCCS_NDES;

	__pdmac_channel_programmable_set(PDMA_NEMC_CHANNEL);
	__pdmac_channel_programmable_set(PDMA_BCH_CHANNEL);
	__pdmac_channel_programmable_set(PDMA_DDR_CHANNEL);
	__pdmac_channel_programmable_set(PDMA_MOVE_CHANNEL);

	REG_PDMAC_DCMD(PDMA_NEMC_CHANNEL) = PDMAC_DCMD_SWDH_32 | PDMAC_DCMD_DWDH_32 |
		PDMAC_DCMD_TSZ_AUTO | PDMAC_DCMD_TIE | PDMAC_DCMD_RDIL_4BYTE;
	REG_PDMAC_DCMD(PDMA_BCH_CHANNEL) = PDMAC_DCMD_SWDH_32 | PDMAC_DCMD_DWDH_32 |
		PDMAC_DCMD_TSZ_AUTO | PDMAC_DCMD_TIE | PDMAC_DCMD_RDIL_4BYTE;
	REG_PDMAC_DCMD(PDMA_DDR_CHANNEL) = PDMAC_DCMD_SAI | PDMAC_DCMD_DAI |
		PDMAC_DCMD_SWDH_32 | PDMAC_DCMD_DWDH_32 |
		PDMAC_DCMD_TSZ_AUTO | PDMAC_DCMD_TIE | PDMAC_DCMD_RDIL_4BYTE;
	REG_PDMAC_DCMD(PDMA_MOVE_CHANNEL) = PDMAC_DCMD_SAI | PDMAC_DCMD_DAI |
		PDMAC_DCMD_SWDH_32 | PDMAC_DCMD_DWDH_32 |
		PDMAC_DCMD_TSZ_AUTO | PDMAC_DCMD_TIE | PDMAC_DCMD_RDIL_4BYTE;
}
