/*
 * JZSOC DMA controller
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 */

#undef DEBUG
#define VERBOSE_DEBUG

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <soc/irq.h>

#include <mach/jzdma.h>

#define CH_DSA	0x00
#define CH_DTA	0x04
#define CH_DTC	0x08
#define CH_DRT	0x0C
#define CH_DCS	0x10
#define CH_DCM	0x14
#define CH_DDA	0x18
#define CH_DSD	0x1C

#define TCSM	0x2000

#define DMAC	0x1000
#define DIRQP	0x1004
#define DDR	0x1008
#define DDRS	0x100C
#define DMACP	0x101C
#define DSIRQP	0x1020
#define DSIRQM	0x1024
#define DCIRQP	0x1028
#define DCIRQM	0x102C

/* MCU of PDMA */
#define DMCS	0x1030
#define DMNMB	0x1034
#define DMSMB	0x1038
#define DMINT	0x103C

/* MCU of PDMA */
#define DMINT_S_IP      BIT(17)
#define DMINT_N_IP      BIT(16)

#define DMAC_HLT	BIT(3)
#define DMAC_AR		BIT(2)

#define DCS_NDES	BIT(31)
#define DCS_AR		BIT(4)
#define DCS_TT		BIT(3)
#define DCS_HLT		BIT(2)
#define DCS_CTE		BIT(0)

#define DCM_SAI		BIT(23)
#define DCM_DAI		BIT(22)
#define DCM_SP_MSK	(0x3 << 14)
#define DCM_SP_32	DCM_SP_MSK
#define DCM_SP_16	BIT(15)
#define DCM_SP_8	BIT(14)
#define DCM_DP_MSK	(0x3 << 12)
#define DCM_DP_32	DCM_DP_MSK
#define DCM_DP_16	BIT(13)
#define DCM_DP_8	BIT(12)
#define DCM_TSZ_MSK	(0x7 << 8)
#define DCM_TSZ_SHF	8
#define DCM_STDE	BIT(2)
#define DCM_TIE		BIT(1)
#define DCM_LINK	BIT(0)

#define MCU_MSG_TYPE_NORMAL	0x1<<24
#define MCU_MSG_TYPE_INTC	0x2<<24
#define MCU_MSG_TYPE_INTC_MASKA	0x3<<24
#define GET_MSG_TYPE(msg) 	(msg & 0x07000000)
#define GET_MSG_MASK(msg)	(msg >> 27)

#define MCU_TEST_INTER_DMA
#ifdef MCU_TEST_INTER_DMA
#define MCU_TEST_DATA_DMA 0xB3424FC0 //PDMA_BANK6 - 0x40
#endif
__initdata static int firmware[] = {
#include "firmware.hex"
};

struct jzdma_master;

struct dma_desc {
	unsigned long dcm;
	dma_addr_t dsa;
	dma_addr_t dta;
	unsigned long dtc;
	unsigned long sd;
	unsigned long drt;
	unsigned long reserved[2];
};

struct jzdma_channel {
	int 			id;
	int 			residue;
	struct dma_chan		chan;
	enum jzdma_type		type;
	struct dma_async_tx_descriptor	tx_desc;
	dma_cookie_t		last_completed;
	dma_cookie_t		last_good;
	struct tasklet_struct	tasklet;
	spinlock_t		lock;
#define CHFLG_SLAVE		BIT(0)
	unsigned short		flags;
	unsigned short		status;
	unsigned long 		dcs_saved;
	struct dma_desc		*desc;
	dma_addr_t 		desc_phys;
	unsigned short		desc_nr;
	unsigned short		desc_max;
	struct scatterlist 	*sgl;
	unsigned long	 	sg_len;
	unsigned short		last_sg;
	unsigned long 		tx_dcm_def;
	unsigned long 		rx_dcm_def;
	struct dma_slave_config	*config;
	void __iomem		*iomem;
	struct jzdma_master	*master;
};

enum channel_status {
	STAT_STOPED,STAT_SUBED,STAT_PREPED,STAT_RUNNING,
};

struct jzdma_master {
	struct device		*dev;
	void __iomem   		*iomem;
	struct clk		*clk;
	int 			irq;
	int                     irq_pdmam;   /* irq_pdmam for PDMAM irq */
	struct dma_device	dma_device;
	enum jzdma_type		*map;
	struct irq_chip		irq_chip;
	struct jzdma_channel	channel[NR_DMA_CHANNELS];
};

static struct device *chan2dev(struct dma_chan *chan)
{
	return &chan->dev->device;
}

static inline struct jzdma_channel *to_jzdma_chan(struct dma_chan *chan)
{
	return container_of(chan, struct jzdma_channel, chan);
}

/* tsz for 1,2,4,8,16,32,64 bytes */
const static char dcm_tsz[7] = { 1,2,0,0,3,4,5};
static inline unsigned int get_current_tsz(unsigned long dcmp)
{
	int i;
	int val = (dcmp & DCM_TSZ_MSK) >> DCM_TSZ_SHF;
	for(i = 0;i < sizeof(dcm_tsz)/sizeof(dcm_tsz[0]);i++){
		if(val == dcm_tsz[i])
			break;
	}
	return (1 << i);
}
static inline unsigned get_max_tsz(unsigned long val, unsigned long *dcmp)
{

	int ord;

	ord = ffs(val) - 1;
	if (ord < 0)
		ord = 0;
	else if (ord > 6)
		ord = 6;

	*dcmp &= ~DCM_TSZ_MSK;
	*dcmp |= dcm_tsz[ord] << DCM_TSZ_SHF;

	/* if tsz == 8, set it to 4 */
	return ord == 3 ? 4 : 1 << ord;
}

#if 0
static void dump_dma_desc(struct jzdma_channel *dmac)
{
	struct dma_desc *desc = dmac->desc;
	int i;

	for(i = 0; i < dmac->desc_nr; i++)
		dev_info(chan2dev(&dmac->chan),
				"DSA: %x, DTA: %x, DCM: %lx, DTC:%lx DRT:%x\n",
				desc[i].dsa,desc[i].dta,desc[i].dcm,desc[i].dtc,desc[i].drt);

	dev_info(chan2dev(&dmac->chan),"CH_DSA = 0x%08x\n",readl(dmac->iomem + CH_DSA));
	dev_info(chan2dev(&dmac->chan),"CH_DTA = 0x%08x\n",readl(dmac->iomem + CH_DTA));
	dev_info(chan2dev(&dmac->chan),"CH_DTC = 0x%08x\n",readl(dmac->iomem + CH_DTC));
	dev_info(chan2dev(&dmac->chan),"CH_DRT = 0x%08x\n",readl(dmac->iomem + CH_DRT));
	dev_info(chan2dev(&dmac->chan),"CH_DCS = 0x%08x\n",readl(dmac->iomem + CH_DCS));
	dev_info(chan2dev(&dmac->chan),"CH_DCM = 0x%08x\n",readl(dmac->iomem + CH_DCM));
	dev_info(chan2dev(&dmac->chan),"CH_DDA = 0x%08x\n",readl(dmac->iomem + CH_DDA));
	dev_info(chan2dev(&dmac->chan),"CH_DSD = 0x%08x\n",readl(dmac->iomem + CH_DSD));
}

static void dump_dma(struct jzdma_master *master)
{
	dev_info(master->dev,"DMAC   = 0x%08x\n",readl(master->iomem + DMAC));
	dev_info(master->dev,"DIRQP  = 0x%08x\n",readl(master->iomem + DIRQP));
	dev_info(master->dev,"DDR    = 0x%08x\n",readl(master->iomem + DDR));
	dev_info(master->dev,"DDRS   = 0x%08x\n",readl(master->iomem + DDRS));
	dev_info(master->dev,"DMACP  = 0x%08x\n",readl(master->iomem + DMACP));
	dev_info(master->dev,"DSIRQP = 0x%08x\n",readl(master->iomem + DSIRQP));
	dev_info(master->dev,"DSIRQM = 0x%08x\n",readl(master->iomem + DSIRQM));
	dev_info(master->dev,"DCIRQP = 0x%08x\n",readl(master->iomem + DCIRQP));
	dev_info(master->dev,"DCIRQM = 0x%08x\n",readl(master->iomem + DCIRQM));
}
void jzdma_dump(struct dma_chan *chan)
{
	struct jzdma_channel *dmac = to_jzdma_chan(chan);
	dump_dma_desc(dmac);
	dump_dma(dmac->master);
}
EXPORT_SYMBOL_GPL(jzdma_dump);
#else
#define dump_dma_desc(A) (void)(0)
#define dump_dma(A) (void)(0)
#endif

static int build_one_desc(struct jzdma_channel *dmac, dma_addr_t src,dma_addr_t dst,
		unsigned long dcm, unsigned cnt,enum jzdma_type type)
{
	struct dma_desc *desc = dmac->desc + dmac->desc_nr;
	if (dmac->desc_nr >= dmac->desc_max)
		return -1;

	desc->dsa = src;
	desc->dta = dst;
	desc->dcm = dcm;
	desc->drt = type;
	desc->dtc = (((unsigned int)(desc + 1) >> 4)<<24) + cnt;
	dmac->desc_nr++;

	return 0;
}

static int build_desc_from_sg(struct jzdma_channel *dmac,struct scatterlist *sgl, unsigned int sg_len,
		enum dma_data_direction direction)
{
	struct dma_slave_config *config = dmac->config;
	struct scatterlist *sg;
	unsigned long dcm;
	short i,tsz;

	if (direction == DMA_TO_DEVICE)
		dcm = DCM_SAI | dmac->tx_dcm_def;
	else
		dcm = DCM_DAI | dmac->rx_dcm_def;

	/* clear LINK bit when issue pending */
	dcm |= DCM_TIE | DCM_LINK;

	for_each_sg(sgl, sg, sg_len, i) {
		dma_addr_t mem;

		mem = sg_dma_address(sg);

		if (direction == DMA_TO_DEVICE) {
                        tsz = get_max_tsz(mem | sg_dma_len(sg) | config->dst_maxburst, &dcm);
			tsz = sg_dma_len(sg) / tsz;
			build_one_desc(dmac, mem, config->dst_addr, dcm, tsz, dmac->type);
		} else {
                        tsz = get_max_tsz(mem | sg_dma_len(sg) | config->src_maxburst, &dcm);
			tsz = sg_dma_len(sg) / tsz;
			build_one_desc(dmac, config->src_addr, mem, dcm, tsz, dmac->type+1);
		}
	}

	return i;
}

static void jzdma_mcu_reset(struct jzdma_master *dma)
{
	unsigned long dmcs;
	dmcs = readl(dma->iomem + DMCS);
	dmcs |= 0x1;
	writel(dmcs, dma->iomem + DMCS);
}

static int jzdma_load_firmware(struct jzdma_master *dma)
{
	int i;
	dev_info(dma->dev,"firmware size:%d bytes\n",sizeof(firmware));
	dev_dbg(dma->dev,"dump:\n");
	for(i=0;i<256;i+=4)
		dev_dbg(dma->dev,"%08x:%08x:%08x:%08x\n",
				firmware[i],firmware[i+1],firmware[i+2],firmware[i+3]);
	memcpy(dma->iomem + TCSM,firmware,sizeof(firmware));
#ifdef MCU_TEST_INTER_DMA
				(*((unsigned long long *)MCU_TEST_DATA_DMA)) = 0;
				(*(((unsigned long long *)MCU_TEST_DATA_DMA)+1)) = 0;
				(*(((unsigned long long *)MCU_TEST_DATA_DMA)+2)) = 0;
				(*(((unsigned long long *)MCU_TEST_DATA_DMA)+3)) = 0;
				(*(((unsigned long long *)MCU_TEST_DATA_DMA)+4)) = 0;
				(*(((unsigned long long *)MCU_TEST_DATA_DMA)+5)) = 0;
#endif
	return 0;
}

static void jzdma_mcu_init(struct jzdma_master *dma)
{
	unsigned long dmcs;
	dmcs = readl(dma->iomem + DMCS);
	dmcs &= ~0x1;
	writel(dmcs, dma->iomem + DMCS);
}

static struct dma_async_tx_descriptor *jzdma_add_desc(struct dma_chan *chan, dma_addr_t src,
		dma_addr_t dst,unsigned cnt,enum dma_data_direction direction,int flag)
{
	unsigned long tsz,dcm=0,type = 0;
	struct jzdma_channel *dmac = to_jzdma_chan(chan);

	if (!(dmac->status == STAT_STOPED || dmac->status == STAT_PREPED))
		return NULL;

	dev_vdbg(chan2dev(chan),"Channel %d add desc\n",dmac->chan.chan_id);

	if(direction == DMA_TO_DEVICE) {
		tsz = get_max_tsz(dmac->config->dst_maxburst, &dcm);
		if (flag == 1){
			dcm |= DCM_SAI | dmac->tx_dcm_def | DCM_LINK | DCM_TIE;
		}else if(flag == 0){
			dcm |= DCM_SAI | DCM_DAI | dmac->tx_dcm_def | DCM_LINK | DCM_TIE;
		}
		type = dmac->type;
	} else {
		tsz = get_max_tsz(dmac->config->src_maxburst, &dcm);
		dcm |= DCM_DAI | dmac->rx_dcm_def | DCM_TIE | DCM_LINK;
		type = dmac->type+1;
	}

	build_one_desc(dmac, src, dst, dcm, cnt, type);

	BUG_ON(!(dmac->flags & CHFLG_SLAVE));

	/* use 8-word descriptors */
	writel(1<<30, dmac->iomem+CH_DCS);

	/* tx descriptor shouldn't reused before dma finished. */
	dmac->tx_desc.flags |= DMA_CTRL_ACK;
	dmac->status = STAT_PREPED;
	return &dmac->tx_desc;
}

static struct dma_async_tx_descriptor *jzdma_prep_slave_sg(struct dma_chan *chan, struct scatterlist *sgl,
		unsigned int sg_len, enum dma_data_direction direction,unsigned long flags)
{
	struct jzdma_channel *dmac = to_jzdma_chan(chan);

	if (dmac->status != STAT_STOPED)
		return NULL;

	dev_vdbg(chan2dev(chan),"Channel %d prepare slave sg list\n",dmac->chan.chan_id);

	BUG_ON(!(dmac->flags & CHFLG_SLAVE));

	dmac->last_sg = 0;
	dmac->sgl = sgl;
	dmac->sg_len = sg_len;
	if (sg_len > dmac->desc_max) {
		sg_len = dmac->desc_max;
		dmac->last_sg = sg_len;
	}
	build_desc_from_sg(dmac, sgl, sg_len, direction);

	/* use 8-word descriptors */
	writel(1<<30, dmac->iomem+CH_DCS);

	/* tx descriptor shouldn't reused before dma finished. */
	dmac->tx_desc.flags |= DMA_CTRL_ACK;
	dmac->status = STAT_PREPED;
	return &dmac->tx_desc;
}

static struct dma_async_tx_descriptor *jzdma_prep_memcpy(struct dma_chan *chan, dma_addr_t dma_dest,
		dma_addr_t dma_src, size_t len, unsigned long flags)
{
	struct jzdma_channel *dmac = to_jzdma_chan(chan);
	unsigned long tsz,dcm = 0;

	if (!(dmac->status == STAT_STOPED || dmac->status == STAT_PREPED))
		return NULL;

	tsz = get_max_tsz(dma_dest | dma_src | len, &dcm);

	dev_vdbg(chan2dev(chan),"Channel %d prepare memcpy d:%x s:%x l:%d %lx %lx\n"
			,dmac->chan.chan_id, dma_dest, dma_src, len, tsz, dcm);

	dcm |= DCM_TIE | DCM_DAI | DCM_SAI | DCM_LINK;
	build_one_desc(dmac, dma_src, dma_dest, dcm, len/tsz, JZDMA_REQ_AUTO);

	/* use 8-word descriptors */
	writel(1<<30, dmac->iomem+CH_DCS);

	/* tx descriptor can reused before dma finished. */
	dmac->tx_desc.flags &= ~DMA_CTRL_ACK;

	dmac->status = STAT_PREPED;
	return &dmac->tx_desc;
}

static struct dma_async_tx_descriptor *jzdma_prep_dma_cyclic(struct dma_chan *chan, dma_addr_t dma_addr,
		size_t buf_len, size_t period_len,enum dma_data_direction direction)
{
	int i = 0;
	unsigned long tsz = 0;
	unsigned long dcm = 0;
	unsigned long cnt = 0;
	unsigned int periods = buf_len / period_len;
	struct jzdma_channel *dmac = to_jzdma_chan(chan);
	struct dma_slave_config *config = dmac->config;
	struct dma_desc *desc = dmac->desc;

	dmac->desc_nr = 0;

	if (direction == DMA_TO_DEVICE)
		dcm |= DCM_SAI;
	else
		dcm |= DCM_DAI;

	/* clear LINK bit when issue pending */
	dcm |= DCM_TIE | DCM_LINK;

	/* the last periods will not be used,
	   |>-------------------------------------->|
	   | desc[0]->desc[1]->...->desc[periods-1] |->desc[periods]
	   |<--------------------------------------<|
	   desc[periods-1] DOA point to desc[0], so it will
	   be a cycle, and desc[periods] will not be used,
	   in that case, the last desc->dcm set ~DCM_LINK
	   when call jzdma_issue_pending(); will not effect
	   the real desc we need to use (desc[0 ~ (periods-1)])
	 */
	if (periods >= dmac->desc_max - 1)
		return NULL;

	for (i = 0; i <=  periods; i++) {
		/* get desc address */
		desc = dmac->desc + dmac->desc_nr;
		/* computer tsz */
		if (direction == DMA_TO_DEVICE) {
			tsz = get_max_tsz(dma_addr | period_len | config->dst_maxburst, &dcm);
			cnt = period_len / tsz;
			desc->dsa = dma_addr;
			desc->dta = config->dst_addr;
			desc->drt = dmac->type;
		} else {
			tsz = get_max_tsz(dma_addr | period_len | config->src_maxburst, &dcm);
			cnt = period_len / tsz;
			desc->dsa = config->src_addr;
			desc->dta = dma_addr;
			desc->drt = dmac->type + 1;
		}
		/* set dcm */
		desc->dcm = dcm;
		/* set the last desc point to the first one */
		if (i == periods -1)
			desc->dtc = ((i + 1)<<24) + cnt;
		else
			desc->dtc = ((0)<<24) + cnt;
		/* update dma_addr and desc_nr */
		dma_addr += period_len;
		dmac->desc_nr ++;
	}

	/* use 8-word descriptors */
	writel(1<<30, dmac->iomem+CH_DCS);

	/* tx descriptor can reused before dma finished. */
	dmac->tx_desc.flags &= ~DMA_CTRL_ACK;

	dmac->status = STAT_PREPED;

	return &dmac->tx_desc;
}

static dma_cookie_t jzdma_tx_submit(struct dma_async_tx_descriptor *tx)
{
	struct jzdma_channel *dmac = to_jzdma_chan(tx->chan);
	dma_cookie_t cookie = dmac->chan.cookie;

	if (dmac->status != STAT_PREPED)
		return -EINVAL;

	spin_lock_bh(&dmac->lock);

	if (++cookie < 0)
		cookie = 1;
	dmac->chan.cookie = cookie;
	dmac->tx_desc.cookie = cookie;
	dmac->status = STAT_SUBED;
	dmac->residue = -1;

	spin_unlock_bh(&dmac->lock);

	dev_vdbg(chan2dev(&dmac->chan),"Channel %d submit\n",dmac->chan.chan_id);

	return cookie;
}

static enum dma_status jzdma_tx_status(struct dma_chan *chan,dma_cookie_t cookie,
		struct dma_tx_state *txstate)
{
	struct jzdma_channel *dmac = to_jzdma_chan(chan);
	dma_cookie_t last_used;
	enum dma_status ret;
	int residue;

	last_used = chan->cookie;

	ret = dma_async_is_complete(cookie, dmac->last_completed, last_used);
	if(dmac->residue == -1) {
		residue = readl(dmac->iomem + CH_DTC) * get_current_tsz(readl(dmac->iomem + CH_DCM));
		dma_set_tx_state(txstate, dmac->last_completed, last_used, residue); 
	} else {
		dma_set_tx_state(txstate, dmac->last_completed, last_used, dmac->residue);
	}

	if (ret == DMA_SUCCESS
			&& dmac->last_completed != dmac->last_good
			&& (dma_async_is_complete(cookie, dmac->last_good, last_used)
				== DMA_IN_PROGRESS))
		ret = DMA_ERROR;

	return ret;
}

static void jzdma_chan_tasklet(unsigned long data)
{
	struct jzdma_channel *dmac = (struct jzdma_channel *)data;
	unsigned long dcs;

	dcs = dmac->dcs_saved;

	dev_vdbg(chan2dev(&dmac->chan),"tasklet: DCS%d=%lx\n", dmac->chan.chan_id, dcs);

	if (dcs & DCS_AR)
		dev_notice(chan2dev(&dmac->chan),"Addr Error: DCS%d=%lx\n", dmac->chan.chan_id, dcs);
	if (dcs & DCS_HLT)
		dev_notice(chan2dev(&dmac->chan),"DMA Halt: DCS%d=%lx\n", dmac->chan.chan_id, dcs);

	spin_lock(&dmac->lock);
	if (dcs & DCS_TT)
		dmac->last_good = dmac->tx_desc.cookie;

	dmac->status = STAT_STOPED;

	dmac->last_completed = dmac->tx_desc.cookie;
	dmac->desc_nr = 0;
	spin_unlock(&dmac->lock);

	if (dmac->tx_desc.callback)
		dmac->tx_desc.callback(dmac->tx_desc.callback_param);
}

static void pdmam_chan_tasklet(unsigned long data)
{
	struct jzdma_channel *dmac = (struct jzdma_channel *)data;
#ifdef MCU_TEST_INTER_DMA
	(*(((unsigned long long *)(MCU_TEST_DATA_DMA))+5))++;
#endif
        spin_lock(&dmac->lock);
        dmac->status = STAT_STOPED;
        dmac->last_good = dmac->tx_desc.cookie;
        dmac->last_completed = dmac->tx_desc.cookie;
        dmac->desc_nr = 0;
        spin_unlock(&dmac->lock);
	if (dmac->tx_desc.callback)
		dmac->tx_desc.callback(dmac->tx_desc.callback_param);
}

static void jzdma_issue_pending(struct dma_chan *chan)
{
	struct jzdma_channel *dmac = to_jzdma_chan(chan);
	struct dma_desc *desc = dmac->desc + dmac->desc_nr - 1;

	if (dmac->status != STAT_SUBED)
		return;

	dmac->status = STAT_RUNNING;

	dev_vdbg(chan2dev(chan),
			"Channel %d issue pending\n",dmac->chan.chan_id);

	desc->dcm &= ~DCM_LINK;
	desc->dcm |= DCM_TIE;//bc

	/* dma descriptor address */
	writel(dmac->desc_phys, dmac->iomem+CH_DDA);
	/* initiate descriptor fetch */
	writel(BIT(dmac->id), dmac->master->iomem+DDRS);

	/* DCS.CTE = 1 */
	set_bit(0, dmac->iomem+CH_DCS);

        dump_dma_desc(dmac);
	dump_dma(dmac->master);
}

static void jzdma_terminate_all(struct dma_chan *chan)
{
	struct jzdma_channel *dmac = to_jzdma_chan(chan);

	dev_vdbg(chan2dev(chan), "terminate_all %d\n", dmac->chan.chan_id);
	spin_lock_bh(&dmac->lock);

	dmac->status = STAT_STOPED;
	dmac->desc_nr = 0;
	dmac->residue = readl(dmac->iomem + CH_DTC) * get_current_tsz(readl(dmac->iomem + CH_DCM));

	/* clear dma status */
	writel(0, dmac->iomem+CH_DCS);

	spin_unlock_bh(&dmac->lock);
}

static int jzdma_control(struct dma_chan *chan, enum dma_ctrl_cmd cmd,
		unsigned long arg)
{
	struct jzdma_channel *dmac = to_jzdma_chan(chan);
	struct dma_slave_config *config = (void*)arg;
	int ret = 0;

	switch (cmd) {
		case DMA_TERMINATE_ALL:
			jzdma_terminate_all(chan);
			break;

		case DMA_SLAVE_CONFIG:
			ret |= !config->src_addr_width || !config->dst_addr_width;
			ret |= !config->src_maxburst || !config->dst_maxburst;
			ret |= !config->src_addr && !config->dst_addr;
			if (ret) {
				dev_warn(chan2dev(chan), "Bad DMA control argument !");
				ret = -EINVAL;
				break;
			}

			switch(config->dst_addr_width) {
				case DMA_SLAVE_BUSWIDTH_1_BYTE:
					dmac->tx_dcm_def = DCM_SP_8 | DCM_DP_8;
					break;
				case DMA_SLAVE_BUSWIDTH_2_BYTES:
					dmac->tx_dcm_def = DCM_SP_16 | DCM_DP_16;
					break;
				case DMA_SLAVE_BUSWIDTH_4_BYTES:
					dmac->tx_dcm_def &= ~DCM_SP_32;
					dmac->tx_dcm_def &= ~DCM_DP_32;
					break;
				case DMA_SLAVE_BUSWIDTH_8_BYTES:
				case DMA_SLAVE_BUSWIDTH_UNDEFINED:
					dev_warn(chan2dev(chan), "Bad DMA control argument !");
					return -EINVAL;
			}

			switch(config->src_addr_width) {
				case DMA_SLAVE_BUSWIDTH_1_BYTE:
					dmac->rx_dcm_def = DCM_SP_8 | DCM_DP_8;
					break;
				case DMA_SLAVE_BUSWIDTH_2_BYTES:
					dmac->rx_dcm_def = DCM_SP_16 | DCM_DP_16;
					break;
				case DMA_SLAVE_BUSWIDTH_4_BYTES:
					dmac->tx_dcm_def &= ~DCM_SP_32;
					dmac->tx_dcm_def &= ~DCM_DP_32;
					break;
				case DMA_SLAVE_BUSWIDTH_8_BYTES:
				case DMA_SLAVE_BUSWIDTH_UNDEFINED:
					dev_warn(chan2dev(chan), "Bad DMA control argument !");
					return -EINVAL;
			}

			dmac->flags |= CHFLG_SLAVE;
			dmac->config = config;
			break;

		default:
			return -ENOSYS;
	}

	return ret;
}

irqreturn_t jzdma_int_handler(int irq, void *dev)
{
	struct jzdma_master *master = (struct jzdma_master *)dev;
	unsigned long pending;
	int i;

	pending = readl(master->iomem + DIRQP);

	for (i = 0; i < NR_DMA_CHANNELS; i++) {
		struct jzdma_channel *dmac = master->channel + i;

		if (!(pending & (1<<i)))
			continue;

		dmac->dcs_saved = readl(dmac->iomem + CH_DCS);
		writel(0, dmac->iomem + CH_DCS);
		if (dmac->status != STAT_RUNNING)
			continue;

		tasklet_schedule(&dmac->tasklet);
	}
	pending = readl(master->iomem + DMAC);
	pending &= ~(DMAC_HLT | DMAC_AR);
	writel(pending, master->iomem + DMAC);
	writel(0, master->iomem + DIRQP);
	return IRQ_HANDLED;
}

irqreturn_t mcu_int_handler(int irq_pdmam, void *dev)
{
	struct jzdma_master *master = (struct jzdma_master *)dev;
	unsigned long pending, mailbox = 0;
        struct jzdma_channel *dmac = master->channel + 3;
	int tmp;
#ifdef MCU_TEST_INTER_DMA
	(*(((unsigned long long *)(MCU_TEST_DATA_DMA))+4))++;
#endif
        spin_lock(&dmac->lock);

	pending = readl(master->iomem + DMINT);

	if (pending & DMINT_N_IP) 
		mailbox = readl(master->iomem + DMNMB);
#if 1
	else{
		spin_unlock(&dmac->lock);
		return IRQ_HANDLED;
	}
#else
        else if(pending & DMINT_S_IP)
		mailbox = readl(master->iomem + DMSMB);
#endif	
        spin_unlock(&dmac->lock);
        *(int *)dmac->tx_desc.callback_param = (mailbox & 0xffff);
        tasklet_schedule(&dmac->tasklet);
#if 1
        tmp = readl(master->iomem + DMINT);
        tmp &= ~DMINT_N_IP;
        writel(tmp, master->iomem + DMINT);
#else
        if(pending & DMINT_N_IP) {
                tmp = readl(master->iomem + DMINT);
                tmp &= ~DMINT_N_IP;
                writel(tmp, master->iomem + DMINT);
	} else if(pending & DMINT_S_IP) {
                tmp = readl(master->iomem + DMINT);
                tmp &= ~DMINT_S_IP;
                writel(tmp, master->iomem + DMINT);
	}
#endif
		/*
		  dev_dbg(master->dev, "go into mcu irq function. DMNMB = 0x%08x \n",
		  readl(master->iomem+DMNMB));
		  for (tmp = 0; tmp<11; tmp++){
		  dev_dbg(master->dev, "\n%04d:",tmp);
		  dev_dbg(master->dev, "%08x ",*(unsigned int *)(0xB34247c0+tmp*4));
		  }
		*/
	return IRQ_HANDLED;
}


irqreturn_t pdma_int_handler(int irq_pdmam, void *dev)
{
	unsigned long pending, mailbox = 0;
	int mask;
	struct jzdma_master *master = (struct jzdma_master *)dev;

	pending = readl(master->iomem + DMINT);

	if(pending & DMINT_N_IP)
		mailbox = readl(master->iomem + DMNMB);

	if(GET_MSG_TYPE(mailbox) == MCU_MSG_TYPE_NORMAL) {
#ifdef MCU_TEST_INTER_DMA
	(*(((unsigned long long *)(MCU_TEST_DATA_DMA))+3))++;
#endif
		generic_handle_irq(IRQ_MCU);
	} 
	if(GET_MSG_TYPE(mailbox) == MCU_MSG_TYPE_INTC) {
		generic_handle_irq(IRQ_GPIO0);
	} 
	if(GET_MSG_TYPE(mailbox) == MCU_MSG_TYPE_INTC_MASKA) {
		mask = GET_MSG_MASK(mailbox);
		*((volatile int *)(0xb0010058)) &= ~(1<<mask);
		generic_handle_irq(IRQ_GPIO0);
	}

	writel(0, master->iomem + DMINT);

	return IRQ_HANDLED;
}

static int jzdma_alloc_chan_resources(struct dma_chan *chan)
{
	struct jzdma_channel *dmac = to_jzdma_chan(chan);
	int ret = 0;

	dma_async_tx_descriptor_init(&dmac->tx_desc, chan);
	dmac->tx_desc.tx_submit = jzdma_tx_submit;
	/* txd.flags will be overwritten in prep funcs, FIXME */
	dmac->tx_desc.flags = DMA_CTRL_ACK;

	dmac->desc = dma_alloc_coherent(chan2dev(chan), PAGE_SIZE,&dmac->desc_phys, GFP_KERNEL);
	if (!dmac->desc) {
		dev_info(chan2dev(chan),"No Memory! ch %d\n", chan->chan_id);
		return -ENOMEM;
	}
	dmac->desc_max = PAGE_SIZE / sizeof(struct dma_desc);

	dev_info(chan2dev(chan),"Channel %d have been requested.(phy id %d,type 0x%02x)\n",
			dmac->chan.chan_id,dmac->id,dmac->type);

	return ret;
}

static void jzdma_free_chan_resources(struct dma_chan *chan)
{
	struct jzdma_channel *dmac = to_jzdma_chan(chan);

	dmac->config = NULL;
	dmac->flags = 0;
	dmac->status = 0;

	dma_free_coherent(chan2dev(chan), PAGE_SIZE,dmac->desc, dmac->desc_phys);
}

unsigned int pdmam_startup_irq(struct irq_data *data)
{
	return 0;
}

void pdmam_irq_dummy(struct irq_data *data)
{
}

int pdmam_set_type(struct irq_data *data, unsigned int flow_type)
{
	return 0;
}

int pdmam_set_wake(struct irq_data *data, unsigned int on)
{
	return 0;
}

static int __init jzdma_probe(struct platform_device *pdev)
{
	struct jzdma_master *dma;
	struct jzdma_platform_data *pdata;
	struct resource *iores;
	short irq, irq_pdmam,irq_mcu;        /* irq_pdmam for PDMAM irq */
	int i,ret = 0;
        unsigned int pdma_program = 0;  /* set pdma DMACP register */

	dma = kzalloc(sizeof(*dma), GFP_KERNEL);
	if (!dma)
		return -ENOMEM;

	pdata = pdev->dev.platform_data;
	if(!pdata) 
		return -ENODATA;

	dma->map = pdata->map;

	dma->irq_chip.name 		= "pdmam";
	dma->irq_chip.irq_startup 	= pdmam_startup_irq;
	dma->irq_chip.irq_shutdown 	= pdmam_irq_dummy;
	dma->irq_chip.irq_unmask 	= pdmam_irq_dummy;
	dma->irq_chip.irq_mask 		= pdmam_irq_dummy;
	dma->irq_chip.irq_mask_ack	= pdmam_irq_dummy;
	dma->irq_chip.irq_set_type 	= pdmam_set_type;
	dma->irq_chip.irq_set_wake	= pdmam_set_wake;

	for(i=pdata->irq_base;i<=pdata->irq_end;i++)
		irq_set_chip_and_handler(i,&dma->irq_chip,handle_level_irq);

	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	irq = platform_get_irq_byname(pdev, "irq");
	irq_mcu = platform_get_irq_byname(pdev, "mcu");
	irq_pdmam = platform_get_irq_byname(pdev, "pdmam");

	if (!iores || irq < 0 || irq_pdmam <0) {
		ret = -EINVAL;
		goto free_dma;
	}

	if (!request_mem_region(iores->start, resource_size(iores), pdev->name)) {
		ret = -EBUSY;
		goto free_dma;
	}

	dma->clk = clk_get(&pdev->dev, "pdma");
	if (IS_ERR(dma->clk)) {
		goto release_mem;
	}

	clk_enable(dma->clk);

	dma->iomem = ioremap(iores->start, resource_size(iores));
	if (!dma->iomem) {
		ret = -ENOMEM;
		goto release_clk;
	}

	ret = request_irq(irq, jzdma_int_handler, IRQF_DISABLED,"pdma", dma);
	if (ret)
		goto release_iomap;
	/* request irq_mcu */
	ret = request_irq(irq_mcu, mcu_int_handler, IRQF_DISABLED,"mcu", dma);
	if (ret)
		goto release_iomap;
	/* request irq_pdmam */
	ret = request_irq(irq_pdmam, pdma_int_handler, IRQF_DISABLED,"pdmam", dma);
	if (ret)
		goto release_iomap;

	/* Initialize dma engine */
	dma_cap_set(DMA_MEMCPY, dma->dma_device.cap_mask);
	dma_cap_set(DMA_SLAVE, dma->dma_device.cap_mask);
	dma_cap_set(DMA_CYCLIC, dma->dma_device.cap_mask);

	INIT_LIST_HEAD(&dma->dma_device.channels);
	/* Initialize master/channel parameters */
	dma->irq = irq;
	dma->irq_pdmam = irq_pdmam;
	dma->iomem = dma->iomem;
	/* Hardware master enable */
	writel(1 | 0x3f << 16, dma->iomem + DMAC);

	for (i = 0; i < NR_DMA_CHANNELS; i++) {
		struct jzdma_channel *dmac = &dma->channel[i];

		dmac->id = i;

		if(dma->map)
			dmac->type = GET_MAP_TYPE(dma->map[i]);
		if(dmac->type == JZDMA_REQ_INVAL) {
			dmac->type = JZDMA_REQ_AUTO;
        		dmac->chan.private = (void *)dmac->type;
                } else
                        dmac->chan.private = (void *)dma->map[i];

		spin_lock_init(&dmac->lock);
		dmac->chan.device = &dma->dma_device;
		if(dma->map[i] & (TYPE_MASK << 16)) { 
			tasklet_init(&dmac->tasklet, pdmam_chan_tasklet,
					(unsigned long)dmac);
                        pdma_program |= (1 << i);
                } else
			tasklet_init(&dmac->tasklet, jzdma_chan_tasklet,
					(unsigned long)dmac);

		dmac->iomem = dma->iomem + i * 0x20;
		dmac->master = dma;

		/* add chan like channel 5,4,3,... */
		list_add(&dmac->chan.device_node,
				&dma->dma_device.channels);
		dev_dbg(&pdev->dev,"add chan (phy id %d , type 0x%02x)\n",i,dmac->type);
	}
        /* the corresponding dma channel is set programmable */
        writel(pdma_program, dma->iomem + DMACP);

	dma->dev = &pdev->dev;
	dma->dma_device.dev = &pdev->dev;
	dma->dma_device.device_alloc_chan_resources = jzdma_alloc_chan_resources;
	dma->dma_device.device_free_chan_resources = jzdma_free_chan_resources;
	dma->dma_device.device_control = jzdma_control;
	dma->dma_device.device_tx_status = jzdma_tx_status;
	dma->dma_device.device_issue_pending = jzdma_issue_pending;
	dma->dma_device.device_prep_slave_sg = jzdma_prep_slave_sg;
	dma->dma_device.device_prep_dma_memcpy = jzdma_prep_memcpy;
	dma->dma_device.device_prep_dma_cyclic = jzdma_prep_dma_cyclic;
	dma->dma_device.device_add_desc = jzdma_add_desc;

	dma_set_max_seg_size(dma->dma_device.dev, 256);

	ret = dma_async_device_register(&dma->dma_device);
	if (ret) {
		dev_err(&pdev->dev, "unable to register\n");
		goto release_irq;
	}

	jzdma_mcu_reset(dma);
	jzdma_load_firmware(dma);
	jzdma_mcu_init(dma);

	dev_info(dma->dev, "JZ SoC DMA initialized\n");
	return 0;

release_irq:
	free_irq(irq_pdmam, dma);
	free_irq(irq, dma);
release_iomap:
	iounmap(dma->iomem);
release_clk:
	if(dma->clk)
		clk_put(dma->clk);
release_mem:
	release_mem_region(iores->start, resource_size(iores));
free_dma:
	kfree(dma);
	dev_dbg(dma->dev, "init failed %d\n",ret);
	return ret;
}

static int __exit jzdma_remove(struct platform_device *pdev)
{
	struct jzdma_master *dma = platform_get_drvdata(pdev);
	dma_async_device_unregister(&dma->dma_device);
	kfree(dma);
	return 0;
}

static struct platform_driver jzdma_driver = {
	.driver		= {
		.name	= "jz-dma",
	},
	.remove		= __exit_p(jzdma_remove),
};

static int __init jzdma_module_init(void)
{
	return platform_driver_probe(&jzdma_driver, jzdma_probe);
}
subsys_initcall(jzdma_module_init);

MODULE_AUTHOR("Zhong Zhiping <zpzhong@ingenic.cn>");
MODULE_DESCRIPTION("Ingenic jz4770 dma driver");
MODULE_LICENSE("GPL");
