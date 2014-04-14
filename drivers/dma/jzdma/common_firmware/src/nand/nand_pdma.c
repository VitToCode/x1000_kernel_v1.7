/*
 * src/nand_pdma.c
 */
#include <common.h>
#include <pdma.h>
#include <nand.h>
#include <mcu.h>
#include <asm/jzsoc.h>

void pdma_nand_nemc_data(unsigned int src, unsigned int dest, unsigned int len,unsigned int direction)
{
	nemc_channel_cfg((unsigned char *)src, (unsigned char *)dest, len, direction);
	__pdmac_special_channel_launch(PDMA_NEMC_CHANNEL);
}

void pdma_nand_nemc_parity(unsigned int src, unsigned int dest, unsigned int len,unsigned int direction)
{
	__pdmac_channel_irq_disable(PDMA_NEMC_CHANNEL);
	nemc_channel_cfg((unsigned char *)src, (unsigned char *)dest, len, direction);
	__pdmac_special_channel_launch(PDMA_NEMC_CHANNEL);
	while (!__pdmac_channel_end_detected(PDMA_NEMC_CHANNEL));
	__pdmac_channel_mirq_clear(PDMA_NEMC_CHANNEL);
	__pdmac_channel_irq_enable(PDMA_NEMC_CHANNEL);
}

void pdma_nand_ddr_data(unsigned int src,unsigned int dest,unsigned int len,unsigned int direction)
{
	ddr_channel_cfg((unsigned char *)src,(unsigned char *)dest,len,direction,PDMA_DDR_CHANNEL);
	__pdmac_channel_launch(PDMA_DDR_CHANNEL);
}

void pdma_nand_movetaskmsg(unsigned int src, unsigned int dest, unsigned int len)
{
	ddr_channel_cfg((unsigned char *)src,(unsigned char *)dest, len, DDR_TO_TCSM, PDMA_MOVE_CHANNEL);
	__pdmac_channel_launch(PDMA_MOVE_CHANNEL);
}
