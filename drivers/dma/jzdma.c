/*
 * JZSOC DMA controller
 *
 * The code contained herein is licensed under the GNU General Public
 * License. You may obtain a copy of the GNU General Public License
 * Version 2 or later at the following locations:
 */

//#define DEBUG
//#define VERBOSE_DEBUG

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/clk.h>

#include <mach/jzdma.h>

#define CH_DSA	0x00
#define CH_DTA	0x04
#define CH_DTC	0x08
#define CH_DRT	0x0C
#define CH_DCS	0x10
#define CH_DCM	0x14
#define CH_DDA	0x18
#define CH_DSD	0x1C

#define GLOBAL_REG_OFFSET	(0x1000)

#define DMAC	(GLOBAL_REG_OFFSET + 0x00)
#define DIRQP	(GLOBAL_REG_OFFSET + 0x04)
#define DDR	(GLOBAL_REG_OFFSET + 0x08)
#define DDRS	(GLOBAL_REG_OFFSET + 0x0C)
/* MCU of PDMA */
#define DMACP	(GLOBAL_REG_OFFSET + 0x1C)
#define DSIRQP	(GLOBAL_REG_OFFSET + 0x20)
#define DSIRQM	(GLOBAL_REG_OFFSET + 0x24)
#define DCIRQP	(GLOBAL_REG_OFFSET + 0x28)
#define DCIRQM	(GLOBAL_REG_OFFSET + 0x2C)
#define DMCS	(GLOBAL_REG_OFFSET + 0x30)
#define DMNMB	(GLOBAL_REG_OFFSET + 0x34)
#define DMSMB	(GLOBAL_REG_OFFSET + 0x38)
#define DMINT	(GLOBAL_REG_OFFSET + 0x3C)

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
#define DCM_SP_16	BIT(15)
#define DCM_SP_8	BIT(14)
#define DCM_DP_MSK	(0x3 << 12)
#define DCM_DP_16	BIT(13)
#define DCM_DP_8	BIT(12)
#define DCM_TSZ_MSK	(0x7 << 8)
#define DCM_TSZ_SHF	8
#define DCM_STDE	BIT(2)
#define DCM_TIE		BIT(1)
#define DCM_LINK	BIT(0)

struct jzdma_master;
struct jzdma_engine;

struct dma_desc {
	unsigned long dcm;
	dma_addr_t dsa,dta;
	unsigned long dtc;
};

struct jzdma_channel {
	unsigned int			channel;
	struct dma_chan			chan;
	struct dma_async_tx_descriptor	tx_desc;
	dma_cookie_t			last_completed;
	dma_cookie_t			last_good;
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
	unsigned long 		dcm_def;
	struct jzdma_slave	*slave;
	void __iomem		*iomem;
	struct jzdma_master	*master;
	bool 				map_chan0;
	bool				map_chan1;
};

enum channel_status {
	STAT_STOPED,STAT_SUBED,STAT_PREPED,STAT_RUNNING,
};

#define NR_DMA_CHANNELS 	32
struct jzdma_master {
	int 		irq;
	void __iomem   	*iomem;
	struct jzdma_engine	*engine;
	struct jzdma_channel	channel[NR_DMA_CHANNELS];
};

struct jzdma_engine {
	struct device		*dev;
	struct dma_device	dma_device;
	struct jzdma_master	master;
	struct clk		*clk;
	void __iomem		*iomem;
};

static struct device *chan2dev(struct dma_chan *chan)
{
	return &chan->dev->device;
}

static inline struct jzdma_channel *to_jzdma_chan(struct dma_chan *chan)
{
	return container_of(chan, struct jzdma_channel, chan);
}

static inline unsigned get_max_tsz(unsigned long val, unsigned long *dcmp)
{
	/* tsz for 1,2,4,8,16,32,64 bytes */
	const static char dcm_tsz[7] = { 1,2,0,0,3,4,5 };
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
			 "DSA: %x, DTA: %x, DCM: %lx, DTC:%lx\n",
			 desc[i].dsa,desc[i].dta,desc[i].dcm,desc[i].dtc);
}
#else
#define dump_dma_desc(A) (void)(0)
#endif

static int build_one_desc(struct jzdma_channel *dmac, dma_addr_t src,
			  dma_addr_t dst, unsigned long dcm, unsigned cnt)
{
	struct dma_desc *desc = dmac->desc + dmac->desc_nr;

	if (dmac->desc_nr >= dmac->desc_max)
		return -1;

	desc->dsa = src;
	desc->dta = dst;
	desc->dcm = dcm;
	desc->dtc = ((dmac->desc_nr+1)<<24) + cnt;
	dmac->desc_nr ++;
	return 0;
}

static int build_desc_from_sg(struct jzdma_channel *dmac,
			      struct scatterlist *sgl, unsigned int sg_len,
			      enum dma_data_direction direction)
{
	struct jzdma_slave *slave = dmac->slave;
	struct scatterlist *sg;
	unsigned long dcm = dmac->dcm_def;
	short i,tsz;

	dcm |= slave->dcm & JZDMA_DCM_MSK;
	if (direction == DMA_TO_DEVICE)
		dcm |= DCM_SAI;
	else
		dcm |= DCM_DAI;

	/* clear LINK bit when issue pending */
	dcm |= DCM_TIE | DCM_LINK;

	for_each_sg(sgl, sg, sg_len, i) {
		dma_addr_t mem;

		mem = sg_dma_address(sg);

		tsz = get_max_tsz(mem | sg_dma_len(sg) | slave->max_tsz
				  , &dcm);
		tsz = sg_dma_len(sg) / tsz;
		if (direction == DMA_TO_DEVICE)
			build_one_desc(dmac, mem, slave->tx_reg, dcm, tsz);
		else
			build_one_desc(dmac, slave->rx_reg, mem, dcm, tsz);
	}
	return i;
}

static struct dma_async_tx_descriptor *
jzdma_prep_slave_sg(struct dma_chan *chan, struct scatterlist *sgl,
		    unsigned int sg_len, enum dma_data_direction direction,
		    unsigned long flags)
{
	struct jzdma_channel *dmac = to_jzdma_chan(chan);
	struct jzdma_slave *slave = dmac->slave;

	if (dmac->status != STAT_STOPED)
		return NULL;

	if (dmac->map_chan0 || dmac->map_chan1)
		return NULL;

	dev_vdbg(chan2dev(chan),
		 "Channel %d prepare slave sg list\n",dmac->channel);

	BUG_ON(!(dmac->flags & CHFLG_SLAVE));

	dmac->last_sg = 0;
	dmac->sgl = sgl;
	dmac->sg_len = sg_len;
	if (sg_len > dmac->desc_max) {
		sg_len = dmac->desc_max;
		dmac->last_sg = sg_len;
	}
	build_desc_from_sg(dmac, sgl, sg_len, direction);

	/* use 4-word descriptors */
	writel(0, dmac->iomem+CH_DCS);

	/* request type */
	if (direction == DMA_TO_DEVICE)
		writel(slave->req_type_tx, dmac->iomem+CH_DRT);
	else
		writel(slave->req_type_rx, dmac->iomem+CH_DRT);

	/* tx descriptor shouldn't reused before dma finished. */
	dmac->tx_desc.flags |= DMA_CTRL_ACK;
	dmac->status = STAT_PREPED;
	return &dmac->tx_desc;
}

static struct dma_async_tx_descriptor *
jzdma_prep_memcpy(struct dma_chan *chan, dma_addr_t dma_dest,
		  dma_addr_t dma_src, size_t len, unsigned long flags)
{
	struct jzdma_channel *dmac = to_jzdma_chan(chan);
	unsigned long tsz,dcm = 0;
	void __iomem *iomem = NULL;

	if (dmac->status != STAT_STOPED)
		return NULL;

	if (dmac->map_chan0 || dmac->map_chan1) {
		if (dmac->map_chan0)
			iomem = dmac->master->channel[0].iomem;
		else
			iomem = dmac->master->channel[1].iomem;

		dcm |= DCM_TIE | DCM_DAI | DCM_SAI | DCM_TSZ_MSK;
		dcm &= ~(DCM_LINK | DCM_STDE | DCM_DP_MSK | DCM_SP_MSK);

		writel(dcm, iomem + CH_DCM);
		clear_bit(31, iomem + CH_DCS);

		/* routine for channel0 || channel1 special use */
	} else {
		tsz = get_max_tsz(dma_dest | dma_src | len, &dcm);

		dev_vdbg(chan2dev(chan),
				 "Channel %d prepare memcpy d:%x s:%x l:%d %lx %lx\n"
				 ,dmac->channel, dma_dest, dma_src, len, tsz, dcm);

		dcm |= DCM_TIE | DCM_DAI | DCM_SAI | DCM_LINK;
		build_one_desc(dmac, dma_src, dma_dest, dcm, len/tsz);

		writel(JZDMA_REQ_AUTO, dmac->iomem+CH_DRT);

		/* use 4-word descriptors */
		writel(0, dmac->iomem+CH_DCS);

		/* tx descriptor can reused before dma finished. */
		dmac->tx_desc.flags &= ~DMA_CTRL_ACK;
	}

	dmac->status = STAT_PREPED;
	return &dmac->tx_desc;
}

static struct dma_async_tx_descriptor *
jzdma_prep_dma_cyclic(struct dma_chan *chan, dma_addr_t dma_addr,
					  size_t buf_len, size_t period_len,
					  enum dma_data_direction direction)
{
	int i = 0;
	unsigned long tsz = 0;
	unsigned long dcm = 0;
	unsigned long cnt = 0;
	unsigned int periods = buf_len / period_len;
	struct jzdma_channel *dmac = to_jzdma_chan(chan);
	struct jzdma_slave *slave = dmac->slave;
	struct dma_desc *desc = dmac->desc;

	dmac->desc_nr = 0;

	if (dmac->map_chan0 || dmac->map_chan1)
		return NULL;

	dcm |= slave->dcm & JZDMA_DCM_MSK;
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
		tsz = get_max_tsz(dma_addr | period_len | slave->max_tsz, &dcm);
		cnt = period_len / tsz;
		/* set src and dst */
		if (direction == DMA_TO_DEVICE) {
			desc->dsa = dma_addr;
			desc->dta = slave->tx_reg;
		} else {
			desc->dsa = slave->rx_reg;
			desc->dta = dma_addr;
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

	/* use 4-word descriptors */
	writel(0, dmac->iomem+CH_DCS);

	/* request type */
	if (direction == DMA_TO_DEVICE)
		writel(slave->req_type_tx, dmac->iomem+CH_DRT);
	else
		writel(slave->req_type_rx, dmac->iomem+CH_DRT);

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

	spin_unlock_bh(&dmac->lock);

	dev_vdbg(chan2dev(&dmac->chan),
		 "Channel %d submit\n",dmac->channel);

	return cookie;
}

static enum dma_status jzdma_tx_status(struct dma_chan *chan,
				       dma_cookie_t cookie,
				       struct dma_tx_state *txstate)
{
	struct jzdma_channel *dmac = to_jzdma_chan(chan);
	dma_cookie_t last_used;
	enum dma_status ret;

	last_used = chan->cookie;

	ret = dma_async_is_complete(cookie, dmac->last_completed, last_used);
	dma_set_tx_state(txstate, dmac->last_completed, last_used, 0);

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

	dev_vdbg(chan2dev(&dmac->chan),
		 "tasklet: DCS%d=%lx\n", dmac->channel, dcs);

	if (dcs & DCS_AR)
		dev_notice(chan2dev(&dmac->chan),
			   "Addr Error: DCS%d=%lx\n", dmac->channel, dcs);
	if (dcs & DCS_HLT)
		dev_notice(chan2dev(&dmac->chan),
			   "DMA Halt: DCS%d=%lx\n", dmac->channel, dcs);

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

static void jzdma_issue_pending(struct dma_chan *chan)
{
	struct jzdma_channel *dmac = to_jzdma_chan(chan);
	struct dma_desc *desc = dmac->desc + dmac->desc_nr - 1;
	void __iomem *iomem = NULL;

	if (dmac->status != STAT_SUBED)
		return;

	dmac->status = STAT_RUNNING;

	if (dmac->map_chan0 || dmac->map_chan1) {
		if (dmac->map_chan0)
			iomem = dmac->master->channel[0].iomem;
		else
			iomem = dmac->master->channel[1].iomem;

		/* routine for channel0 || channel1 special use */
	} else {
		dev_vdbg(chan2dev(chan),
				 "Channel %d issue pending\n",dmac->channel);

		desc->dcm &= ~DCM_LINK;
		dump_dma_desc(dmac);

		/* dma descriptor address */
		writel(dmac->desc_phys, dmac->iomem+CH_DDA);
		/* initiate descriptor fetch */
		writel(BIT(dmac->channel), dmac->master->iomem+DDRS);
	}

	/* DCS.CTE = 1 */
	set_bit(0, dmac->iomem+CH_DCS);
}

static void jzdma_terminate_all(struct dma_chan *chan)
{
	struct jzdma_channel *dmac = to_jzdma_chan(chan);

	dev_vdbg(chan2dev(chan), "terminate_all %d\n", dmac->channel);
	spin_lock_bh(&dmac->lock);

	dmac->status = STAT_STOPED;
	dmac->desc_nr = 0;

	/* clear dma status */
	writel(0, dmac->iomem+CH_DCS);

	spin_unlock_bh(&dmac->lock);
}

static int jzdma_control(struct dma_chan *chan, enum dma_ctrl_cmd cmd,
			 unsigned long arg)
{
	struct jzdma_channel *dmac = to_jzdma_chan(chan);
	struct jzdma_slave *slave = (void*)arg;
	int ret = 0;

	switch (cmd) {
	case DMA_TERMINATE_ALL:
		jzdma_terminate_all(chan);
		break;

	case DMA_SLAVE_CONFIG:
		ret = !slave->reg_width;
		ret |= (!slave->tx_reg && !slave->rx_reg);
		ret |= (slave->tx_reg && !slave->req_type_tx);
		ret |= (slave->rx_reg && !slave->req_type_rx);
		if (ret) {
			dev_warn(chan2dev(chan), "Bad DMA control argument !");
			ret = -EINVAL;
			break;
		}

		dmac->dcm_def = 0;
		if (slave->reg_width == 1) {
			dmac->dcm_def |= DCM_SP_8 | DCM_DP_8;
		}
		else if (slave->reg_width == 2) {
			dmac->dcm_def |= DCM_SP_16 | DCM_DP_16;
		}
		else
			BUG_ON(slave->reg_width != 4);

		if (slave->channel0_special &&
			(!dmac->master->channel[0].map_chan0)) {
			/* here dmac->map_chan0 == true indicate
			   that this channel mapped to channel0.
			   master->channel[0].map_chan0 == true
			   indicate that channel0 has been mapped */
			dmac->map_chan0 = true;
			dmac->master->channel[0].map_chan0 = true;
		} else if (slave->channel1_special &&
				   (!dmac->master->channel[1].map_chan1)) {
			dmac->map_chan1 = true;
			dmac->master->channel[1].map_chan1 = true;
		} else {
			ret = -EINVAL;
			break;
		}

		dmac->flags |= CHFLG_SLAVE;
		dmac->slave = slave;
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

static int jzdma_alloc_chan_resources(struct dma_chan *chan)
{
	struct jzdma_channel *dmac = to_jzdma_chan(chan);
	int ret = 0;

	dma_async_tx_descriptor_init(&dmac->tx_desc, chan);
	dmac->tx_desc.tx_submit = jzdma_tx_submit;
	/* txd.flags will be overwritten in prep funcs, FIXME */
	dmac->tx_desc.flags = DMA_CTRL_ACK;

	dev_vdbg(chan2dev(chan),
		 "Channel %d have been requested\n",dmac->channel);

	return ret;
}

static void jzdma_free_chan_resources(struct dma_chan *chan)
{
	struct jzdma_channel *dmac = to_jzdma_chan(chan);

	dmac->slave = NULL;
	dmac->flags = 0;
	dmac->status = 0;

	if (dmac->map_chan0) {
		dmac->map_chan0 = false;
		dmac->master->channel[0].map_chan0 = false;
	} else if (dmac->map_chan1) {
		dmac->map_chan1 = false;
		dmac->master->channel[1].map_chan1 = false;
	}
}

static int __init jzdma_probe(struct platform_device *pdev)
{
	struct jzdma_engine *dma;
	struct resource *iores;
	short irq;
	int ret, i;

	printk("%s %d\n",__func__,__LINE__);
	dma = kzalloc(sizeof(*dma), GFP_KERNEL);
	if (!dma)
		return -ENOMEM;

	iores = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (!iores || irq < 0) {
		ret = -EINVAL;
		goto free_dma;
	}

	if (!request_mem_region(iores->start, resource_size(iores), pdev->name)) {
		ret = -EBUSY;
		goto free_dma;
	}

	dma->clk = clk_get(&pdev->dev, "pdma");
	clk_enable(dma->clk);
	dma->iomem = ioremap(iores->start, resource_size(iores));
	if (!dma->iomem) {
		ret = -ENOMEM;
		goto release_clk;
	}

	ret = request_irq(irq, jzdma_int_handler, IRQF_DISABLED,
			  "dma_irq", &dma->master);
	if (ret)
		goto release_iomap;

	/* Initialize dma engine */
	dma_cap_set(DMA_MEMCPY, dma->dma_device.cap_mask);
	dma_cap_set(DMA_SLAVE, dma->dma_device.cap_mask);
	//dma_cap_set(DMA_CYCLIC, dma->dma_device.cap_mask);

	INIT_LIST_HEAD(&dma->dma_device.channels);
	/* Initialize master/channel parameters */
	dma->master.engine = dma;
	dma->master.irq = irq;
	dma->master.iomem = dma->iomem;
	/* Hardware master enable */
	writel(1, dma->master.iomem + DMAC);
	/* DMAC->CH01 = 1 to enable channel 0&1 special use */
	set_bit(1, dma->master.iomem + DMAC);

	for (i = 0; i < NR_DMA_CHANNELS; i++) {
		struct jzdma_channel *dmac = &dma->master.channel[i];
		spin_lock_init(&dmac->lock);
		dmac->chan.device = &dma->dma_device;
		dmac->channel = i;
		tasklet_init(&dmac->tasklet, jzdma_chan_tasklet,
				     (unsigned long)dmac);
		dmac->iomem = dma->master.iomem + i * 0x20;
		dmac->master = dma->master.iomem;

		/* here channel 0 and channel 1 are reserved for speicial use and
		   used as no-descriptor mode, channel 2 ~ (NR_DMA_CHANNELS - 1) are
		   gerenal, used as descriptor mode */
		if (i >= 2) {
			dmac->desc = dma_alloc_coherent(&pdev->dev, PAGE_SIZE,&dmac->desc_phys, GFP_KERNEL);
			if (!dmac->desc) {
				dev_info(&pdev->dev,
						 "No Memory! ch %d\n", i);
				continue;
			}
			dmac->desc_max = PAGE_SIZE / sizeof(struct dma_desc);

			/* add chan like channel 5,4,3,... */
			list_add(&dmac->chan.device_node,
					 &dma->dma_device.channels);
		}
	}

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
	//dma->dma_device.dev->dma_parms = &dma->dma_parms;

	dma_set_max_seg_size(dma->dma_device.dev, 256);

	ret = dma_async_device_register(&dma->dma_device);
	if (ret) {
		dev_err(&pdev->dev, "unable to register\n");
		goto release_irq;
	}

	dev_info(dma->dev, "JZ SoC DMA initialized\n");
	return 0;

release_irq:
	free_irq(irq, &dma->master);
release_iomap:
	iounmap(dma->iomem);
release_clk:
	if(dma->clk)
		clk_put(dma->clk);
//release_mem:
	release_mem_region(iores->start, resource_size(iores));
free_dma:
	kfree(dma);
	dev_dbg(dma->dev, "init failed %d\n",ret);
	return ret;
}

static int __exit jzdma_remove(struct platform_device *pdev)
{
	struct jzdma_engine *dma = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < NR_DMA_CHANNELS; i++) {
			struct jzdma_channel *dmac = &dma->master.channel[i];
			if (i > 2)
				dma_free_coherent(NULL, PAGE_SIZE,
								  dmac->desc, dmac->desc_phys);
	}

	dma_async_device_unregister(&dma->dma_device);
	kfree(dma);
	return 0;
}

static struct platform_driver jzdma_driver = {
	.driver		= {
		.name	= "jzdma",
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
