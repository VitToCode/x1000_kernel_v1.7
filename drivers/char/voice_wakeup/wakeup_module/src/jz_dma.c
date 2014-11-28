#include "interface.h"
#include "jz_dma.h"

void build_one_desc(struct dma_config* dma, struct dma_desc* _desc, struct dma_desc* next_desc){
	struct dma_desc *desc = (struct dma_desc *)((unsigned int)_desc | 0xA0000000);
	desc->dsa = dma->src;
	desc->dta = dma->dst;
	desc->dtc = (dma->count&0xffffff) | ( ((unsigned long)(next_desc) >> 4) << 24);
	desc->drt = dma->type;
	desc->sd = dma->sd;
	desc->dcm = dma->increment<<22 | dma->tsz<<8 | dma->sp_dp<<12 | dma->rdil<<16 |dma->stde<<2| dma->tie<<1 | dma->link;
	/* this would be a bug. pay attention */
	//flush_cache_all();
}


void pdma_config(struct dma_config* dma){
	if(!dma->descriptor){
		DMADSA(dma->channel) = dma->src;
		DMADTA(dma->channel) = dma->dst;
		DMADTC(dma->channel) = dma->count;
		DMADRT(dma->channel) = dma->type;
		DMADCS(dma->channel) = 0x80000000;
		DMADCM(dma->channel) = 0;
		DMADCM(dma->channel) = dma->increment<<22 | dma->tsz<<8 | dma->rdil<<16 | dma->sp_dp<<12|dma->stde<<2| dma->tie<<1 | dma->link ;
	}else{
		//DMADMAC = 0;
		DMADCS(dma->channel) = 0;
		DMADDA(dma->channel) = dma->desc;
		if(dma->des8){
			DMADCS(dma->channel) |= dma->des8<<30;
		}else{
			DMADRT(dma->channel) = dma->type;
			DMADSD(dma->channel) = dma->sd;
		}

		DMADDB |= 1 << dma->channel;
		DMADDS |= 1 << dma->channel;
	}
}
void pdma_start(int channel){
	//printf("enable dma channel %d\n",channel);
	DMADMAC |= 1;
	DMADCS(channel) |= 1;
}

void pdma_wait(channel){
	while(!(DMADCS(channel)&1<<3)){
		printf("dma channel %d wait TT\n",channel);
	}
	printf("channel %d Transfer done.\n",channel);
}

void pdma_end(int channel){
	DMADCS(channel) &= ~1;
	//DMADMAC &= ~1;
}

unsigned int pdma_trans_addr(int channel, int direction)
{
	if(direction == DMA_MEM_TO_DEV) { /*ddr to device*/
		return DMADSA(channel);
	} else if (direction == DMA_DEV_TO_MEM){/*device to dma*/
		return DMADTA(channel);
	} else if(direction == DMA_MEM_TO_MEM) {
		printf("src:%08x , dst:%08x\n", DMADSA(channel), DMADTA(channel));
	}
	return 0;
}


