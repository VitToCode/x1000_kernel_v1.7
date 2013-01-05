/**
 * xb_snd_pcm.c
 *
 * jbbi <jbbi@ingenic.cn>
 *
 * 24 APR 2012
 *
 */
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>
#include <linux/vmalloc.h>
#include <linux/string.h>
#include <linux/sound.h>
#include <linux/slab.h>
#include <sound/core.h>
#include <linux/switch.h>
#include <linux/dma-mapping.h>
#include <linux/soundcard.h>
#include <linux/earlysuspend.h>
#include <mach/jzdma.h>
#include <mach/jzsnd.h>
#include <soc/irq.h>
#include <soc/base.h>
#include "xb47xx_pcm.h"
/**
 * global variable
 **/
void volatile __iomem *volatile pcm_iomem;

static spinlock_t pcm_irq_lock;
static struct dsp_endpoints pcm_endpoints;

#define JZ4780_CPM_PCM_SYSCLK 12000000
#define PCM_FIFO_DEPTH 16

static struct pcm_board_info
{
	unsigned long rate;
	unsigned long replay_format;
	unsigned long record_format;
	unsigned long pcmclk;
	unsigned int irq;
	struct dsp_endpoints *endpoint;
}*pcm_priv;

/*
 *	dump
 *
 */
static void dump_pcm_reg(void)
{
	int i = 0;
	unsigned long pcm_regs[] = {
		PCMCTL0, PCMCFG0, PCMDP0, PCMINTC0, PCMINTS0, PCMDIV0 };

	for (i=0; i<ARRAY_SIZE(pcm_regs); i++) {
		printk("pcm reg %4x, = %4x \n",
				(unsigned int)pcm_regs[i], pcm_read_reg(pcm_regs[i]));
	}
}

/*##################################################################*\
 |* suspand func
\*##################################################################*/
static int pcm_suspend(struct platform_device *, pm_message_t state);
static int pcm_resume(struct platform_device *);
static void pcm_shutdown(struct platform_device *);

#ifdef CONFIG_ANDROID
static int is_pcm_suspended = 0;

static void pcm_late_resume(struct early_suspend *h)
{
	is_pcm_suspended = 0;
}

static struct early_suspend jz_i2s_early_suspend = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN,
	.resume = pcm_late_resume,
};
#endif
/*##################################################################*\
|* dev_ioctl
\*##################################################################*/
static int pcm_set_fmt(unsigned long *format,int mode)
{/*??*/
	int ret = 0;
	int data_width = 0;

    /*
	 * The value of format reference to soundcard.
	 * AFMT_MU_LAW      0x00000001
	 * AFMT_A_LAW       0x00000002
	 * AFMT_IMA_ADPCM   0x00000004
	 * AFMT_U8			0x00000008
	 * AFMT_S16_LE      0x00000010
	 * AFMT_S16_BE      0x00000020
	 * AFMT_S8			0x00000040
	 */

	switch (*format) {

	case AFMT_U8:
	case AFMT_S8:
		*format = AFMT_U8;
		data_width = 8;
		if (mode & CODEC_WMODE)
			__pcm_set_oss_sample_size(0);
		if (mode & CODEC_RMODE)
			__pcm_set_iss_sample_size(0);
		break;
	case AFMT_S16_LE:
	case AFMT_S16_BE:
		data_width = 16;
		*format = AFMT_S16_LE;
		if (mode & CODEC_WMODE)
			__pcm_set_oss_sample_size(1);
		if (mode & CODEC_RMODE)
			__pcm_set_iss_sample_size(1);
		break;
	default :
		printk("PCM: there is unknown format 0x%x.\n",(unsigned int)*format);
		return -EINVAL;
	}
	if (mode & CODEC_WMODE)
		if (pcm_priv->replay_format != *format) {
			pcm_priv->replay_format = *format;
			ret |= NEED_RECONF_TRIGGER | NEED_RECONF_FILTER;
			ret |= NEED_RECONF_DMA;
		}

	if (mode & CODEC_RMODE)
		if (pcm_priv->record_format != *format) {
			pcm_priv->record_format = *format;
			ret |= NEED_RECONF_TRIGGER | NEED_RECONF_FILTER;
			ret |= NEED_RECONF_DMA;
		}

	return ret;
}

static int pcm_set_channel(int* channel,int mode)
{
	return 0;
}

static int pcm_set_rate(unsigned long *rate)
{
	unsigned long div;
	if (!pcm_priv)
		return -ENODEV;
	div = pcm_priv->pcmclk/(8*(*rate)) - 1;
	if (div >= 0 && div < 32) {
		__pcm_set_syndiv(div);
		*rate = pcm_priv->pcmclk/(8*(div+1));
		pcm_priv->rate = *rate;
	} else
		*rate = pcm_priv->rate;
	return 0;
}

static int pcm_set_pcmclk(unsigned long pcmclk)
{
	unsigned long div;
	if (!pcm_priv)
		return -ENODEV;
	div = JZ4780_CPM_PCM_SYSCLK/pcmclk - 1;
	if (div >= 0 && div < 64) {
		__pcm_set_clkdiv(div);
		pcm_priv->pcmclk = JZ4780_CPM_PCM_SYSCLK/(div + 1);
	} else
		return -EINVAL;
	return 0;
}

static int get_burst_length(unsigned long val)
{
	/* burst bytes for 1,2,4,8,16,32,64 bytes */
	int ord;

	ord = ffs(val) - 1;
	if (ord < 0)
		ord = 0;
	else if (ord > 6)
		ord = 6;

	/* if tsz == 8, set it to 4 */
	return (ord == 3 ? 4 : 1 << ord)*8;
}

static void pcm_set_trigger(int mode)
{
	int data_width = 0;
	struct dsp_pipe *dp = NULL;
	int burst_length = 0;

	if (!pcm_priv)
		return;

	if (mode & CODEC_WMODE) {
		switch(pcm_priv->replay_format) {
		case AFMT_U8:
			data_width = 8;
			break;
		case AFMT_S16_LE:
			data_width = 16;
			break;
		}
		dp = pcm_priv->endpoint->out_endpoint;
		burst_length = get_burst_length((int)dp->paddr|(int)dp->fragsize|
				dp->dma_config.src_maxburst|dp->dma_config.dst_maxburst);
		__pcm_set_transmit_trigger(PCM_FIFO_DEPTH - burst_length/data_width);
	}
	if (mode &CODEC_RMODE) {
		switch(pcm_priv->record_format) {
		case AFMT_U8:
			data_width = 8;
			break;
		case AFMT_S16_LE:
			data_width = 16;
			break;
		}
		dp = pcm_priv->endpoint->in_endpoint;
		burst_length = get_burst_length((int)dp->paddr|(int)dp->fragsize|
				dp->dma_config.src_maxburst|dp->dma_config.dst_maxburst);
		__pcm_set_receive_trigger((PCM_FIFO_DEPTH - burst_length/data_width) - 1);
	}

	return;
}

static int pcm_enable(int mode)
{
	unsigned long rate = 8000;
	unsigned long replay_format = 16;
	unsigned long record_format = 16;
	int replay_channel = 1;
	int record_channel = 1;
	struct dsp_pipe *dp_other = NULL;
	if (!pcm_priv)
			return -ENODEV;

	pcm_set_rate(&rate);
	if (mode & CODEC_WMODE) {
		dp_other = pcm_priv->endpoint->in_endpoint;
		pcm_set_fmt(&replay_format,mode);
		pcm_set_channel(&replay_channel,mode);
		pcm_set_trigger(mode);
	}
	if (mode & CODEC_RMODE) {
		dp_other = pcm_priv->endpoint->out_endpoint;
		if (!dp_other->is_used) {
			pcm_set_fmt(&record_format,mode);
			pcm_set_channel(&record_channel,mode);
			pcm_set_trigger(mode);
		}
	}

	if (!dp_other->is_used) {
		/*avoid pop FIXME*/
		if (mode & CODEC_WMODE)
			__pcm_flush_fifo();
		__pcm_enable();
		__pcm_clock_enable();
	}
	return 0;
}

static int pcm_disable(int mode)			//CHECK codec is suspend?
{
	if (mode & CODEC_WMODE) {
		__pcm_disable_transmit_dma();
		__pcm_disable_replay();
	}
	if (mode & CODEC_RMODE) {
		__pcm_disable_receive_dma();
		__pcm_disable_record();
	}
	__pcm_disable();
	__pcm_clock_disable();
	return 0;
}


static int pcm_dma_enable(int mode)		//CHECK
{
	int val;
	if (!pcm_priv)
			return -ENODEV;
	if (mode & CODEC_WMODE) {
		__pcm_disable_transmit_dma();
		__pcm_enable_transmit_dma();
		__pcm_enable_replay();
	}
	if (mode & CODEC_RMODE) {
		__pcm_flush_fifo();
		__pcm_enable_record();
		/* read the first sample and ignore it */
		val = __pcm_read_fifo();
		__pcm_enable_receive_dma();
	}

	return 0;
}

static int pcm_dma_disable(int mode)		//CHECK seq dma and func
{
	if (mode & CODEC_WMODE) {
		__pcm_disable_transmit_dma();
		__pcm_disable_replay();
	}
	if (mode & CODEC_RMODE) {
		__pcm_disable_receive_dma();
		__pcm_disable_record();
	}
	return 0;
}

static int pcm_get_fmt_cap(unsigned long *fmt_cap)
{
	*fmt_cap |= AFMT_S16_LE|AFMT_U8;
	return 0;
}

static int pcm_get_fmt(unsigned long *fmt, int mode)
{
	if (!pcm_priv)
			return -ENODEV;

	if (mode & CODEC_WMODE)
		*fmt = pcm_priv->replay_format;
	if (mode & CODEC_RMODE)
		*fmt = pcm_priv->record_format;

	return 0;
}

static void pcm_dma_need_reconfig(int mode)
{
	struct dsp_pipe	*dp = NULL;

	if (!pcm_priv)
			return;
	if (mode & CODEC_WMODE) {
		dp = pcm_priv->endpoint->out_endpoint;
		dp->need_reconfig_dma = true;
	}
	if (mode & CODEC_RMODE) {
		dp = pcm_priv->endpoint->in_endpoint;
		dp->need_reconfig_dma = true;
	}

	return;
}

/********************************************************\
 * dev_ioctl
\********************************************************/
static long pcm_ioctl(unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	switch (cmd) {
	case SND_DSP_ENABLE_REPLAY:
		/* enable pcm record */
		/* set pcm default record format, channels, rate */
		/* set default replay route */
		ret = pcm_enable(CODEC_WMODE);
		break;

	case SND_DSP_DISABLE_REPLAY:
		/* disable pcm replay */
		ret = pcm_disable(CODEC_WMODE);
		break;

	case SND_DSP_ENABLE_RECORD:
		/* enable pcm record */
		/* set pcm default record format, channels, rate */
		/* set default record route */
		ret = pcm_enable(CODEC_RMODE);
		break;

	case SND_DSP_DISABLE_RECORD:
		/* disable pcm record */
		ret = pcm_disable(CODEC_RMODE);
		break;

	case SND_DSP_ENABLE_DMA_RX:
		ret = pcm_dma_enable(CODEC_RMODE);
		break;

	case SND_DSP_DISABLE_DMA_RX:
		ret = pcm_dma_disable(CODEC_RMODE);
		break;

	case SND_DSP_ENABLE_DMA_TX:
		ret = pcm_dma_enable(CODEC_WMODE);
		break;

	case SND_DSP_DISABLE_DMA_TX:
		ret = pcm_dma_disable(CODEC_WMODE);
		break;

	case SND_DSP_SET_REPLAY_RATE:
	case SND_DSP_SET_RECORD_RATE:
		ret = pcm_set_rate((unsigned long *)arg);
		break;

	case SND_DSP_SET_REPLAY_CHANNELS:
	case SND_DSP_SET_RECORD_CHANNELS:
		/* set record channels */
		ret = pcm_set_channel((int*)arg,CODEC_RMODE);
		break;

	case SND_DSP_GET_REPLAY_FMT_CAP:
	case SND_DSP_GET_RECORD_FMT_CAP:
		/* return the support record formats */
		ret = pcm_get_fmt_cap((unsigned long *)arg);
		break;

	case SND_DSP_GET_REPLAY_FMT:
		/* get current replay format */
		pcm_get_fmt((unsigned long *)arg,CODEC_WMODE);
		break;

	case SND_DSP_SET_REPLAY_FMT:
		/* set replay format */
		ret = pcm_set_fmt((unsigned long *)arg,CODEC_WMODE);
		if (ret < 0)
			break;
		/* if need reconfig the trigger, reconfig it */
		if (ret & NEED_RECONF_TRIGGER)
			pcm_set_trigger(CODEC_WMODE);
		/* if need reconfig the dma_slave.max_tsz, reconfig it and
		   set the dp->need_reconfig_dma as true */
		if (ret & NEED_RECONF_DMA)
			pcm_dma_need_reconfig(CODEC_WMODE);
		/* if need reconfig the filter, reconfig it */
		if (ret & NEED_RECONF_FILTER)
			;
		ret = 0;
		break;

	case SND_DSP_GET_RECORD_FMT:
		/* get current record format */
		pcm_get_fmt((unsigned long *)arg,CODEC_RMODE);

		break;

	case SND_DSP_SET_RECORD_FMT:
		/* set record format */
		ret = pcm_set_fmt((unsigned long *)arg,CODEC_RMODE);
		if (ret < 0)
			break;
		/* if need reconfig the trigger, reconfig it */
		if (ret & NEED_RECONF_TRIGGER)
			pcm_set_trigger(CODEC_RMODE);
		/* if need reconfig the dma_slave.max_tsz, reconfig it and
		   set the dp->need_reconfig_dma as true */
		if (ret & NEED_RECONF_DMA)
			pcm_dma_need_reconfig(CODEC_RMODE);
		/* if need reconfig the filter, reconfig it */
		if (ret & NEED_RECONF_FILTER)
			;
		ret = 0;
		break;
        case SND_MIXER_DUMP_REG:
                dump_pcm_reg();
		break;
	default:
		printk("SOUND_ERROR: %s(line:%d) unknown command!",
				__func__, __LINE__);
		ret = -EINVAL;
	}

	return ret;
}

/*##################################################################*\
|* functions
\*##################################################################*/
static irqreturn_t pcm_irq_handler(int irq, void *dev_id)
{
	unsigned long flags;
	irqreturn_t ret = IRQ_HANDLED;

	spin_lock_irqsave(&pcm_irq_lock,flags);

	spin_unlock_irqrestore(&pcm_irq_lock,flags);

	return ret;
}

static int pcm_init_pipe(struct dsp_pipe **dp , enum dma_data_direction direction,unsigned long iobase)
{
	if (*dp != NULL || dp == NULL)
		return 0;
	*dp = vmalloc(sizeof(struct dsp_pipe));
	if (*dp == NULL) {
		printk("pcm : init pipe fail vmalloc ");
		return -ENOMEM;
	}

	(*dp)->dma_config.direction = direction;
	(*dp)->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	(*dp)->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	(*dp)->dma_type = JZDMA_REQ_PCM0;

	(*dp)->fragsize = FRAGSIZE_S;
	(*dp)->fragcnt = FRAGCNT_M;
	(*dp)->is_non_block = true;
	(*dp)->is_used = false;
	(*dp)->can_mmap =true;
	INIT_LIST_HEAD(&((*dp)->free_node_list));
	INIT_LIST_HEAD(&((*dp)->use_node_list));

	if (direction == DMA_TO_DEVICE) {
		(*dp)->dma_config.src_maxburst = 16;
		(*dp)->dma_config.dst_maxburst = 16;
		(*dp)->dma_config.dst_addr = iobase + PCMDP0;
		(*dp)->dma_config.src_addr = 0;
	} else if (direction == DMA_FROM_DEVICE) {
		(*dp)->dma_config.src_maxburst = 16;
		(*dp)->dma_config.dst_maxburst = 16;
		(*dp)->dma_config.src_addr = iobase + PCMDP0;
		(*dp)->dma_config.dst_addr = 0;
	} else
		return -1;

	return 0;
}

static int pcm_global_init(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *pcm_resource = NULL;
	struct dsp_pipe *pcm_pipe_out = NULL;
	struct dsp_pipe *pcm_pipe_in = NULL;
	struct clk *pcm_sysclk = NULL;
	struct clk *pcmclk = NULL;

	pcm_resource = platform_get_resource(pdev,IORESOURCE_MEM,0);
	if (pcm_resource == NULL) {
		printk("pcm: platform_get_resource fail.\n");
		return -1;
	}

	/* map io address */
	if (!request_mem_region(pcm_resource->start, resource_size(pcm_resource), pdev->name)) {
		printk("pcm: mem region fail busy .\n");
		return -EBUSY;
	}
	pcm_iomem = ioremap(pcm_resource->start, resource_size(pcm_resource));
	if (!pcm_iomem) {
		printk ("pcm: ioremap fail.\n");
		ret =  -ENOMEM;
		goto __err_ioremap;
	}

	ret = pcm_init_pipe(&pcm_pipe_out,DMA_TO_DEVICE,pcm_resource->start);
	if (ret < 0)
		goto __err_init_pipeout;

	ret = pcm_init_pipe(&pcm_pipe_in,DMA_FROM_DEVICE,pcm_resource->start);
	if (ret < 0)
		goto __err_init_pipein;

	pcm_endpoints.out_endpoint = pcm_pipe_out;
	pcm_endpoints.in_endpoint = pcm_pipe_in;

	/* request aic clk FIXME*/
	pcm_sysclk = clk_get(&pdev->dev, "pcm");
	if (IS_ERR(pcm_sysclk)) {
                dev_dbg(&pdev->dev, "pcm clk_get failed\n");
                goto __err_init_pipein;
	}
	clk_enable(pcm_sysclk);

	spin_lock_init(&pcm_irq_lock);
	/* request irq */
	pcm_resource = platform_get_resource(pdev,IORESOURCE_IRQ,0);
	if (pcm_resource == NULL) {
		ret = -1;
		goto __err_irq;
	}

	/*FIXME share irq*/
	pcm_priv->irq = pcm_resource->start;
	ret = request_irq(pcm_resource->start, pcm_irq_handler,
					  IRQF_DISABLED, "pcm_irq", NULL);
	if (ret < 0)
		goto __err_irq;

	/*FIXME set sysclk output for codec*/
	pcmclk = clk_get(&pdev->dev, "cgu_pcm");
	if (IS_ERR(pcmclk)) {
                dev_dbg(&pdev->dev, "CGU pcm clk_get failed\n");
                goto __err_pcmclk;
	}
#if 0
	clk_set_rate(pcmclk, JZ4780_CPM_PCM_SYSCLK);
	if (clk_get_rate(pcmclk) > JZ4780_CPM_PCM_SYSCLK) {
		printk("codec interface set rate fail. clk %lu\n", clk_get_rate(pcmclk));
		goto __err_pcmclk;
	}
	clk_enable(pcmclk);
#endif
	__pcm_as_slaver();

	pcm_set_pcmclk(12000000);
	__pcm_disable_receive_dma();
	__pcm_disable_transmit_dma();
	__pcm_disable_record();
	__pcm_disable_replay();
	__pcm_flush_fifo();
	__pcm_clear_ror();
	__pcm_clear_tur();
	__pcm_set_receive_trigger(7);
	__pcm_set_transmit_trigger(8);
	__pcm_disable_overrun_intr();
	__pcm_disable_underrun_intr();
	__pcm_disable_transmit_intr();
	__pcm_disable_receive_intr();
	__pcm_set_msb_one_shift_in();
	__pcm_set_msb_one_shift_out();
	/* play zero or last sample when underflow */
	__pcm_play_lastsample();
	//__pcm_enable();

	return 0;

__err_pcmclk:
	clk_put(pcmclk);
	free_irq(pcm_priv->irq,NULL);
__err_irq:
	clk_disable(pcmclk);
	clk_put(pcm_sysclk);
	vfree(pcm_pipe_in);
__err_init_pipein:
	vfree(pcm_pipe_out);
__err_init_pipeout:
	iounmap(pcm_iomem);
__err_ioremap:
	release_mem_region(pcm_resource->start,resource_size(pcm_resource));
	return ret;
}

static int pcm_init(struct platform_device *pdev)
{
	int ret = -EINVAL;

#ifdef CONFIG_ANDROID
	register_early_suspend(&jz_i2s_early_suspend);
#endif
	ret = pcm_global_init(pdev);

	return ret;
}

static void pcm_shutdown(struct platform_device *pdev)
{
	/* close pcm and current codec */
	free_irq(pcm_priv->irq,NULL);
	return;
}

static int pcm_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int pcm_resume(struct platform_device *pdev)
{
	return 0;
}

struct snd_dev_data pcm_data = {
	.dev_ioctl	   	= pcm_ioctl,
	.ext_data		= &pcm_endpoints,
	.minor			= SND_DEV_DSP1,
	.init			= pcm_init,
	.shutdown		= pcm_shutdown,
	.suspend		= pcm_suspend,
	.resume			= pcm_resume,
};

struct snd_dev_data snd_mixer1_data = {
        .dev_ioctl              = pcm_ioctl,
        .minor                  = SND_DEV_MIXER1,
};

static int __init init_pcm(void)
{
	pcm_priv = (struct pcm_board_info *)vmalloc(sizeof(struct pcm_board_info));
	if (!pcm_priv)
		return -1;
	pcm_priv->rate = 0;
	pcm_priv->replay_format = 0;
	pcm_priv->record_format = 0;
	pcm_priv->endpoint= &pcm_endpoints;

	return 0;
}
device_initcall(init_pcm);
