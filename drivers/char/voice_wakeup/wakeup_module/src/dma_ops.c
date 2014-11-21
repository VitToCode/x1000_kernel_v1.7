#include <common.h>
#include <jz_cpm.h>
#include "jz_dma.h"
#include "jz_dmic.h"
#include "dmic_config.h"
#include "interface.h"
#include "dma_ops.h"

int *src_buf;
int *dst_buf;


struct dma_config config;
struct dma_desc *desc;

void build_circ_descs(void)
{
	int i;
	struct dma_desc *cur;
	struct dma_desc *next;
	for(i=0; i< NR_DESC; i++) {
		config.src = V_TO_P(src_buf);
		config.dst = V_TO_P(dst_buf + ((BUF_SIZE/NR_DESC/sizeof(dst_buf)) * i));
		config.count = BUF_SIZE/NR_DESC/4; /*count of data unit*/
		config.link = 1;
		cur = &desc[i];
		if(i == NR_DESC -1) {
			next = &desc[0];
		} else {
			next = &desc[i+1];
		}
		if(0) {
		printf("cur_desc:%x, next_desc:%x\n", cur, next);
		}
		build_one_desc(&config, cur, next);
	}

}

void dump_descs(void)
{
	int i;
	struct dma_desc *temp;
	struct dma_desc *temp_phy;
	for(i=0; i<NR_DESC; i++) {
		temp = &desc[i];
		temp_phy = (struct dma_desc *)((unsigned int)temp | 0xA0000000);

		printf("desc[%d].dcm:%08x:dcm:%08x\n",i,temp->dcm, temp_phy->dcm);
		printf("desc[%d].dsa:%08x:dsa:%08x\n",i,temp->dsa, temp_phy->dsa);
		printf("desc[%d].dta:%08x:dta:%08x\n",i,temp->dta, temp_phy->dta);
		printf("desc[%d].dtc:%08x:dtc:%08x\n",i,temp->dtc, temp_phy->dtc);
		printf("desc[%d].sd:%08x:sd:%08x\n",i,temp->sd, temp_phy->sd);
		printf("desc[%d].drt:%08x:drt:%08x\n",i,temp->drt, temp_phy->drt);
		printf("desc[%d].reserved[0]:%08x:reserved[0]:%08x\n",i,temp->reserved[0], temp_phy->reserved[0]);
		printf("desc[%d].reserved[1]:%08x:reserved[1]:%08x\n",i,temp->reserved[1], temp_phy->reserved[1]);

	}

}
void dump_tcsm()
{
	int i;
	unsigned int *t = (unsigned int *)TCSM_BANK_5;
	for(i = 0; i< 256; i++) {
		printf("t[%d]:%x\n", i, *(t+i));
	}
}

void dump_dma_register(int chn)
{
	printf("=============== dma dump register ===============\n");
	printf("DMAC   : 0x%08X\n", DMADMAC);
	printf("DIRQP  : 0x%08X\n", DMADIRQP);
	printf("DDB    : 0x%08X\n", DMADDB);
	printf("DDS    : 0x%08X\n", DMADDS);
	printf("DMACP  : 0x%08X\n", DMADMACP);
	printf("DSIRQP : 0x%08X\n", DMADSIRQP);
	printf("DSIRQM : 0x%08X\n", DMADSIRQM);
	printf("DCIRQP : 0x%08X\n", DMADCIRQP);
	printf("DCIRQM : 0x%08X\n", DMADCIRQM);
	printf("DMCS   : 0x%08X\n", DMADMCS);
	printf("DMNMB  : 0x%08X\n", DMADMNMB);
	printf("DMSMB  : 0x%08X\n", DMADMSMB);
	printf("DMINT  : 0x%08X\n", DMADMINT);

	printf("* DMAC.FMSC = 0x%d\n", DMA_READBIT(DMADMAC, 31, 1));
	printf("* DMAC.FSSI = 0x%d\n", DMA_READBIT(DMADMAC, 30, 1));
	printf("* DMAC.FTSSI= 0x%d\n", DMA_READBIT(DMADMAC, 29, 1));
	printf("* DMAC.FUART= 0x%d\n", DMA_READBIT(DMADMAC, 28, 1));
	printf("* DMAC.FAIC = 0x%d\n", DMA_READBIT(DMADMAC, 27, 1));
	printf("* DMAC.INTCC= 0x%d\n", DMA_READBIT(DMADMAC, 17, 5));
	printf("* DMAC.INTCE= 0x%d\n", DMA_READBIT(DMADMAC, 16, 1));
	printf("* DMAC.HLT  = 0x%d\n", DMA_READBIT(DMADMAC,  3, 1));
	printf("* DMAC.AR   = 0x%d\n", DMA_READBIT(DMADMAC,  2, 1));
	printf("* DMAC.CH01 = 0x%d\n", DMA_READBIT(DMADMAC,  1, 1));
	printf("* DMAC.DMAE = 0x%d\n", DMA_READBIT(DMADMAC,  0, 1));

	if(chn>=0 && chn<=31){
		printf("");
		printf("DSA(%02d) : 0x%08X\n", chn, DMADSA(chn));
		printf("DTA(%02d) : 0x%08X\n", chn, DMADTA(chn));
		printf("DTC(%02d) : 0x%08X\n", chn, DMADTC(chn));
		printf("DRT(%02d) : 0x%08X\n", chn, DMADRT(chn));
		printf("DCS(%02d) : 0x%08X\n", chn, DMADCS(chn));
		printf("DCM(%02d) : 0x%08X\n", chn, DMADCM(chn));
		printf("DDA(%02d) : 0x%08X\n", chn, DMADDA(chn));
		printf("DSD(%02d) : 0x%08X\n", chn, DMADSD(chn));

		printf("* DCS(%02d).NDES = 0x%d\n", chn, DMA_READBIT(DMADCS(chn), 31, 1));
		printf("* DCS(%02d).DES8 = 0x%d\n", chn, DMA_READBIT(DMADCS(chn), 30, 1));
		printf("* DCS(%02d).CDOA = 0x%d\n", chn, DMA_READBIT(DMADCS(chn), 8, 8));
		printf("* DCS(%02d).AR   = 0x%d\n", chn, DMA_READBIT(DMADCS(chn), 4, 1));
		printf("* DCS(%02d).TT   = 0x%d\n", chn, DMA_READBIT(DMADCS(chn), 3, 1));
		printf("* DCS(%02d).HLT  = 0x%d\n", chn, DMA_READBIT(DMADCS(chn), 2, 1));
		printf("* DCS(%02d).CTE  = 0x%d\n", chn, DMA_READBIT(DMADCS(chn), 0, 1));

		printf("* DCM(%02d).SDI  = 0x%d\n", chn, DMA_READBIT(DMADCM(chn), 26, 2));
		printf("* DCM(%02d).DID  = 0x%d\n", chn, DMA_READBIT(DMADCM(chn), 24, 2));
		printf("* DCM(%02d).SAI  = 0x%d\n", chn, DMA_READBIT(DMADCM(chn), 23, 1));
		printf("* DCM(%02d).DAI  = 0x%d\n", chn, DMA_READBIT(DMADCM(chn), 22, 1));
		printf("* DCM(%02d).RDIL = 0x%d\n", chn, DMA_READBIT(DMADCM(chn), 16, 4));
		printf("* DCM(%02d).SP   = 0x%d\n", chn, DMA_READBIT(DMADCM(chn), 14, 2));
		printf("* DCM(%02d).DP   = 0x%d\n", chn, DMA_READBIT(DMADCM(chn), 12, 2));
		printf("* DCM(%02d).TSZ  = 0x%d\n", chn, DMA_READBIT(DMADCM(chn),  8, 3));
		printf("* DCM(%02d).STDE = 0x%d\n", chn, DMA_READBIT(DMADCM(chn),  2, 1));
		printf("* DCM(%02d).TIE  = 0x%d\n", chn, DMA_READBIT(DMADCM(chn),  1, 1));
		printf("* DCM(%02d).LINK = 0x%d\n", chn, DMA_READBIT(DMADCM(chn),  0, 1));

		printf("* DDA(%02d).DBA = 0x%08X\n", chn, DMA_READBIT(DMADDA(chn),  12, 20));
		printf("* DDA(%02d).DOA = 0x%08X\n", chn, DMA_READBIT(DMADDA(chn),  4, 8));

		printf("* DSD(%02d).TSD = 0x%08X\n", chn, DMA_READBIT(DMADSD(chn),  16, 16));
		printf("* DSD(%02d).SSD = 0x%08X\n", chn, DMA_READBIT(DMADSD(chn),   0, 16));
	}
	printf("INT_ICSR0:0x%08X\n",*(int volatile*)0xB0001000);
	printf("\n");
}

void dma_open(void)
{
	//dump_dma_register(DMA_CHANNEL);
	/*config and enable*/
	REG32(CPM_IOBASE + CPM_CLKGR0) &= ~(1 << 21);

	//desc =(struct dma_desc *)((unsigned int)_desc | 0xA0000000);
	desc = (struct dma_desc *)(DMA_DESC_ADDR);
	src_buf = (int *)DMIC_RX_FIFO;
	dst_buf = (int *)TCSM_BANK_5;

	config.type = DMIC_REQ_TYPE; /* dmic reveive request */
	config.channel = DMA_CHANNEL;
	/*...*/
	config.increment = 1; /*src no inc, dst inc*/
	config.rdil = 0;
	config.sp_dp = 0x00; /*32bit*/
	config.stde = 0;
	config.descriptor = 1;
	config.des8 = 1;
	config.sd = 0;
	config.tsz	 = 0;
	config.desc = V_TO_P(desc);
	config.tie = 1;

	build_circ_descs();

	pdma_config(&config);
	dump_descs();
	pdma_start(DMA_CHANNEL);
	//dump_dma_register(DMA_CHANNEL);

}

void dma_close(void)
{
	/*disable dma*/
	pdma_end(DMA_CHANNEL);
	//dump_dma_register(DMA_CHANNEL);
}
