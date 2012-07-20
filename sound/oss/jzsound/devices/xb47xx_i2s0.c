/**
 * xb_snd_i2s0.c
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
#include "xb47xx_i2s0.h"
/**
 * global variable
 **/
void volatile __iomem *volatile i2s0_iomem;
static LIST_HEAD(codecs_head);
static spinlock_t i2s0_irq_lock;
static struct snd_switch_data switch_data;
static volatile int jz_switch_state = 0;
static spinlock_t i2s0_hp_detect_state;

static struct dsp_endpoints i2s0_endpoints;

static struct workqueue_struct *i2s0_work_queue;
static struct work_struct	i2s0_codec_work;

static struct codec_info {
	struct list_head list;
	char *name;
	unsigned long record_rate;
	unsigned long replay_rate;
	int record_codec_channel;
	int replay_codec_channel;
	int record_format;
	int replay_format;
	enum codec_mode codec_mode;
	unsigned long codec_clk;
	int (*codec_ctl)(unsigned int cmd, unsigned long arg);
	struct dsp_endpoints *dsp_endpoints;
} *cur_codec;

/*##################################################################*\
 |* suspand func
\*##################################################################*/
static int i2s0_suspend(struct platform_device *, pm_message_t state);
static int i2s0_resume(struct platform_device *);
static void i2s0_shutdown(struct platform_device *);

#ifdef CONFIG_ANDROID
static int is_i2s0_suspended = 0;

static void i2s0_late_resume(struct early_suspend *h)
{
	if (is_i2s0_suspended && cur_codec)
		cur_codec->codec_ctl(CODEC_RESUME, 0);

	is_i2s0_suspended = 0;
}

static struct early_suspend jz_i2s_early_suspend = {
	.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN,
	.resume = i2s0_late_resume,
};
#endif

/*##################################################################*\
  |* codec control
\*##################################################################*/
/**
 * register the codec
 **/

int i2s0_register_codec(char *name, void *codec_ctl,unsigned long codec_clk,enum codec_mode mode)
{
	struct codec_info *tmp = vmalloc(sizeof(struct codec_info));
	if ((name != NULL) && (codec_ctl != NULL)) {
		tmp->name = name;
		tmp->codec_ctl = codec_ctl;
		tmp->codec_clk = codec_clk;
		tmp->codec_mode = mode;
		tmp->dsp_endpoints = &i2s0_endpoints;
		list_add_tail(&tmp->list, &codecs_head);
		return 0;
	} else {
		return -1;
	}
}
EXPORT_SYMBOL(i2s0_register_codec);

static void i2s0_match_codec(char *name)
{
	struct codec_info *codec_info;
	struct list_head  *list,*tmp;

	list_for_each_safe(list,tmp,&codecs_head) {
		codec_info = container_of(list,struct codec_info,list);
		if (!strcmp(codec_info->name,name))
			cur_codec = codec_info;
	}
}
/*##################################################################*\
|* filter opt
\*##################################################################*/
#ifdef CONFIG_ANDROID
static void i2s0_set_filter(int mode)
{
	struct dsp_pipe *dp = NULL;

	if (mode & CODEC_RMODE)
		dp = cur_codec->dsp_endpoints->in_endpoint;
	else
		return;

	switch(cur_codec->record_rate) {
		case AFMT_U8:
		case AFMT_S8:
			dp->filter = convert_16bits_stereomix2mono;
			break;
		case AFMT_S16_LE:
			break;
		default :
			dp->filter = NULL;
			printk("AUDIO DEVICE :filter set error.\n");
	}
}
#else
static void i2s0_set_filter(int mode) {;}
#endif
/*##################################################################*\
|* dev_ioctl
\*##################################################################*/
static int i2s0_set_fmt(unsigned long *format,int mode)
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
		data_width = 8;
		if (mode & CODEC_WMODE) {
			__i2s0_set_oss_sample_size(0);
			__i2s0_disable_byteswap();
		}
		if (mode & CODEC_RMODE)
			__i2s0_set_iss_sample_size(0);
		__i2s0_disable_signadj();
		break;
	case AFMT_S8:
		data_width = 8;
		if (mode & CODEC_WMODE) {
			__i2s0_set_oss_sample_size(0);
			__i2s0_disable_byteswap();
		}
		if (mode & CODEC_RMODE)
			__i2s0_set_iss_sample_size(0);
		__i2s0_enable_signadj();	//??
		break;
	case AFMT_S16_LE:
		data_width = 16;
		if (mode & CODEC_WMODE) {
			__i2s0_set_oss_sample_size(1);
			__i2s0_disable_byteswap();
		}
		if (mode & CODEC_RMODE)
			__i2s0_set_iss_sample_size(1);
		__i2s0_disable_signadj();
		break;
	case AFMT_S16_BE:
		data_width = 16;
		if (mode & CODEC_WMODE) {
			__i2s0_set_oss_sample_size(1);
			__i2s0_enable_byteswap();
		}
		if (mode == CODEC_RMODE) {
			__i2s0_set_iss_sample_size(1);
			*format = AFMT_S16_LE;
		}
		__i2s0_disable_signadj();
		break;
	default :
		printk("I2S0: there is unknown format 0x%x.\n",(unsigned int)*format);
		return -EINVAL;
	}
	if (!cur_codec)
		return -ENODEV;

	if (mode & CODEC_WMODE) {
		if ((ret = cur_codec->codec_ctl(CODEC_SET_REPLAY_DATA_WIDTH, data_width)) != 0) {
			printk("JZ I2S: CODEC ioctl error, command: CODEC_SET_REPLAY_FORMAT");
			return ret;
		}
		if (cur_codec->replay_format != *format) {
			cur_codec->replay_format = *format;
			ret |= NEED_RECONF_TRIGGER;
			ret |= NEED_RECONF_DMA;
		}
	}

	if (mode & CODEC_RMODE) {
		if ((ret = cur_codec->codec_ctl(CODEC_SET_RECORD_DATA_WIDTH, data_width)) != 0) {
			printk("JZ I2S: CODEC ioctl error, command: CODEC_SET_RECORD_FORMAT");
			return ret;
		}
		if (cur_codec->record_format != *format) {
			cur_codec->record_format = *format;
			ret |= NEED_RECONF_TRIGGER | NEED_RECONF_FILTER;
			ret |= NEED_RECONF_DMA;
		}
	}

	return ret;
}

static int i2s0_set_channel(int* channel,int mode)
{
	int ret = 0;

	if (!cur_codec)
		return -ENODEV;

	if (mode & CODEC_WMODE) {
		ret = cur_codec->codec_ctl(CODEC_SET_RECORD_CHANNEL,(unsigned long)channel);
		if (ret < 0) {
			cur_codec->record_codec_channel = *channel;
			return ret;
		}
		if (*channel ==2 || *channel == 4||
			*channel ==6 || *channel == 8) {
			__i2s0_out_channel_select(*channel - 1);
			__i2s0_disable_mono2stereo();
		} else if (*channel == 1) {
			__i2s0_out_channel_select(*channel - 1);
			__i2s0_enable_mono2stereo();
		} else
			return -EINVAL;
		if (cur_codec->replay_codec_channel != *channel)
			cur_codec->replay_codec_channel = *channel;
	}
	if (mode & CODEC_RMODE) {
#ifdef CONFIG_ANDROID
		*channel = 2;
#endif
		ret = cur_codec->codec_ctl(CODEC_SET_RECORD_CHANNEL,(unsigned long)channel);
		if (ret < 0)
			return ret;
#ifdef CONFIG_ANDROID
		*channel = 1;
#endif
		if (cur_codec->record_codec_channel != *channel)
			cur_codec->record_codec_channel = *channel;
	}
	return ret;
}

static int i2s0_set_rate(unsigned long *rate,int mode)
{
	int ret = 0;
	if (!cur_codec)
		return -ENODEV;
	if (mode & CODEC_WMODE) {
		if (cur_codec->codec_mode == CODEC_SLAVE)
			*rate = __i2s0_set_sample_rate(cur_codec->codec_clk,*rate);
		ret = cur_codec->codec_ctl(CODEC_SET_REPLAY_RATE,(unsigned long)rate);
		cur_codec->replay_rate = *rate;
	}
	if (mode & CODEC_RMODE) {
		if (cur_codec->codec_mode == CODEC_SLAVE)
			*rate = __i2s0_set_isample_rate(cur_codec->codec_clk,*rate);
		ret = cur_codec->codec_ctl(CODEC_SET_RECORD_RATE,(unsigned long)rate);
		cur_codec->record_rate = *rate;
	}
	return ret;
}
#define I2S0_TX_FIFO_DEPTH		64
#define I2S0_RX_FIFO_DEPTH		32

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

static void i2s0_set_trigger(int mode)
{
	int data_width = 0;
	struct dsp_pipe *dp = NULL;
	int burst_length = 0;

	if (!cur_codec)
		return;

	if (mode & CODEC_WMODE) {
		switch(cur_codec->replay_format) {
		case AFMT_S8:
		case AFMT_U8:
			data_width = 8;
			break;
		case AFMT_S16_BE:
		case AFMT_S16_LE:
			data_width = 16;
			break;
		}
		dp = cur_codec->dsp_endpoints->out_endpoint;
		burst_length = get_burst_length((int)dp->paddr|(int)dp->fragsize|
				dp->dma_config.src_maxburst|dp->dma_config.dst_maxburst);
		if (I2S0_TX_FIFO_DEPTH - burst_length*8/data_width >= 60)
			__i2s0_set_transmit_trigger(30);
		else
			__i2s0_set_transmit_trigger((I2S0_TX_FIFO_DEPTH - burst_length/data_width) >> 1);

	}
	if (mode &CODEC_RMODE) {
		switch(cur_codec->record_format) {
		case AFMT_S8:
		case AFMT_U8:
			data_width = 8;
			break;
		case AFMT_S16_BE:
		case AFMT_S16_LE:
			data_width = 16;
			break;
		}
		dp = cur_codec->dsp_endpoints->in_endpoint;
		burst_length = get_burst_length((int)dp->paddr|(int)dp->fragsize|
				dp->dma_config.src_maxburst|dp->dma_config.dst_maxburst);
		__i2s0_set_receive_trigger(((I2S0_RX_FIFO_DEPTH - burst_length*8/data_width) >> 1) - 1);
	}

	return;
}
static int i2s0_set_default_route(int mode)
{

	if (!cur_codec)
		return -ENODEV;

	if (mode & CODEC_RWMODE) {
		cur_codec->codec_ctl(CODEC_SET_ROUTE, SND_ROUTE_REPLAY_HEADPHONE);
	} else if (mode & CODEC_WMODE) {
		cur_codec->codec_ctl(CODEC_SET_ROUTE, SND_ROUTE_REPLAY_HEADPHONE);
	} else if (mode & CODEC_RMODE){
		cur_codec->codec_ctl(CODEC_SET_ROUTE, SND_ROUTE_RECORD_MIC);
	}

	return 0;
}

static int i2s0_enable(int mode)
{
	unsigned long replay_rate = 44100;
	unsigned long record_rate = 44100;
	unsigned long replay_format = 16;
	unsigned long record_format = 16;
	int replay_channel = 2;
	int record_channel = 2;
	struct dsp_pipe *dp_other = NULL;
	if (!cur_codec)
			return -ENODEV;

	cur_codec->codec_ctl(CODEC_ANTI_POP,mode);

	if (mode & CODEC_WMODE) {
		dp_other = cur_codec->dsp_endpoints->in_endpoint;
		i2s0_set_fmt(&replay_format,mode);
		i2s0_set_channel(&replay_channel,mode);
		i2s0_set_rate(&replay_rate,mode);
	}
	if (mode & CODEC_RMODE) {
		dp_other = cur_codec->dsp_endpoints->out_endpoint;
		if (!dp_other->is_used) {
			i2s0_set_fmt(&record_format,mode);
			i2s0_set_channel(&record_channel,mode);
			i2s0_set_rate(&record_rate,mode);
		}
	}
	i2s0_set_trigger(mode);
	i2s0_set_filter(mode);

	i2s0_set_default_route(mode);
	if (!dp_other->is_used) {
		/*avoid pop FIXME*/
		if (mode & CODEC_WMODE)
			__i2s0_flush_tfifo();
		cur_codec->codec_ctl(CODEC_DAC_MUTE,1);
		__i2s0_enable();
		__i2s0_select_i2s();
		cur_codec->codec_ctl(CODEC_DAC_MUTE,0);
	}
	return 0;
}

static int i2s0_disable(int mode)			//CHECK codec is suspend?
{
	if (mode & CODEC_WMODE) {
		__i2s0_disable_transmit_dma();
		__i2s0_disable_replay();
	}
	if (mode & CODEC_RMODE) {
		__i2s0_disable_receive_dma();
		__i2s0_disable_record();
	}
	__i2s0_disable();

	return 0;
}


static int i2s0_dma_enable(int mode)		//CHECK
{
	int val;
	if (!cur_codec)
			return -ENODEV;
	if (mode & CODEC_WMODE) {
		__i2s0_disable_transmit_dma();
		cur_codec->codec_ctl(CODEC_DAC_MUTE,1);
		__i2s0_enable_replay();
		while(!__i2s0_test_tur());
		cur_codec->codec_ctl(CODEC_DAC_MUTE,0);
		__i2s0_enable_transmit_dma();
	}
	if (mode & CODEC_RMODE) {
		//__i2s0_flush_rfifo();
		__i2s0_enable_record();
		/* read the first sample and ignore it */
		val = __i2s0_read_rfifo();
		__i2s0_enable_receive_dma();
	}

	return 0;
}

static int i2s0_dma_disable(int mode)		//CHECK seq dma and func
{
	if (mode & CODEC_WMODE) {
		__i2s0_disable_transmit_dma();
		__i2s0_disable_replay();
	}
	if (mode & CODEC_RMODE) {
		__i2s0_disable_receive_dma();
		__i2s0_disable_record();
	}
	return 0;
}

static int i2s0_get_fmt_cap(unsigned long *fmt_cap,int mode)
{
	unsigned long i2s0_fmt_cap = 0;
	if (!cur_codec)
			return -ENODEV;
	if (mode & CODEC_WMODE) {
		i2s0_fmt_cap |= AFMT_S16_LE|AFMT_S16_BE|AFMT_S8|AFMT_U8;
		cur_codec->codec_ctl(CODEC_GET_REPLAY_FMT_CAP, *fmt_cap);

	}
	if (mode & CODEC_RMODE) {
		i2s0_fmt_cap |= AFMT_S16_LE|AFMT_S8|AFMT_U8;
		cur_codec->codec_ctl(CODEC_GET_RECORD_FMT_CAP, *fmt_cap);
	}

	if (*fmt_cap == 0)
		*fmt_cap = i2s0_fmt_cap;
	else
		*fmt_cap &= i2s0_fmt_cap;

	return 0;
}


static int i2s0_get_fmt(unsigned long *fmt, int mode)
{
	if (!cur_codec)
			return -ENODEV;

	if (mode & CODEC_WMODE)
		*fmt = cur_codec->replay_format;
	if (mode & CODEC_RMODE)
		*fmt = cur_codec->record_format;

	return 0;
}

static void i2s0_dma_need_reconfig(int mode)
{
	struct dsp_pipe	*dp = NULL;

	if (!cur_codec)
			return;
	if (mode & CODEC_WMODE) {
		dp = cur_codec->dsp_endpoints->out_endpoint;
		dp->need_reconfig_dma = true;
	}
	if (mode & CODEC_RMODE) {
		dp = cur_codec->dsp_endpoints->in_endpoint;
		dp->need_reconfig_dma = true;
	}
	return;
}

/********************************************************\
 * dev_ioctl
\********************************************************/
static long i2s0_ioctl(unsigned int cmd, unsigned long arg)
{
	long ret = 0;

	switch (cmd) {
	case SND_DSP_ENABLE_REPLAY:
		/* enable i2s0 record */
		/* set i2s0 default record format, channels, rate */
		/* set default replay route */
		ret = i2s0_enable(CODEC_WMODE);
		break;

	case SND_DSP_DISABLE_REPLAY:
		/* disable i2s0 replay */
		ret = i2s0_disable(CODEC_WMODE);
		break;

	case SND_DSP_ENABLE_RECORD:
		/* enable i2s0 record */
		/* set i2s0 default record format, channels, rate */
		/* set default record route */
		ret = i2s0_enable(CODEC_RMODE);
		break;

	case SND_DSP_DISABLE_RECORD:
		/* disable i2s0 record */
		ret = i2s0_disable(CODEC_RMODE);
		break;

	case SND_DSP_ENABLE_DMA_RX:
		ret = i2s0_dma_enable(CODEC_RMODE);
		break;

	case SND_DSP_DISABLE_DMA_RX:
		ret = i2s0_dma_disable(CODEC_RMODE);
		break;

	case SND_DSP_ENABLE_DMA_TX:
		ret = i2s0_dma_enable(CODEC_WMODE);
		break;

	case SND_DSP_DISABLE_DMA_TX:
		ret = i2s0_dma_disable(CODEC_WMODE);
		break;

	case SND_DSP_SET_REPLAY_RATE:
		ret = i2s0_set_rate((unsigned long *)arg,CODEC_WMODE);
		break;

	case SND_DSP_SET_RECORD_RATE:
		ret = i2s0_set_rate((unsigned long *)arg,CODEC_RMODE);
		break;

	case SND_DSP_SET_REPLAY_CHANNELS:
		/* set replay channels */
		ret = i2s0_set_channel((int *)arg,CODEC_WMODE);
		if (ret < 0)
			break;
		ret = 0;
		break;

	case SND_DSP_SET_RECORD_CHANNELS:
		/* set record channels */
		ret = i2s0_set_channel((int*)arg,CODEC_RMODE);
		if (ret < 0)
			break;
		ret = 0;
		break;

	case SND_DSP_GET_REPLAY_FMT_CAP:
		/* return the support replay formats */
		ret = i2s0_get_fmt_cap((unsigned long *)arg,CODEC_WMODE);
		break;

	case SND_DSP_GET_REPLAY_FMT:
		/* get current replay format */
		i2s0_get_fmt((unsigned long *)arg,CODEC_WMODE);
		break;

	case SND_DSP_SET_REPLAY_FMT:
		/* set replay format */
		ret = i2s0_set_fmt((unsigned long *)arg,CODEC_WMODE);
		if (ret < 0)
			break;
		/* if need reconfig the trigger, reconfig it */
		if (ret & NEED_RECONF_TRIGGER)
			i2s0_set_trigger(CODEC_WMODE);
		/* if need reconfig the dma_slave.max_tsz, reconfig it and
		   set the dp->need_reconfig_dma as true */
		if (ret & NEED_RECONF_DMA)
			i2s0_dma_need_reconfig(CODEC_WMODE);
		/* if need reconfig the filter, reconfig it */
		ret = 0;
		break;

	case SND_DSP_GET_RECORD_FMT_CAP:
		/* return the support record formats */
		ret = i2s0_get_fmt_cap((unsigned long *)arg,CODEC_RMODE);
		break;

	case SND_DSP_GET_RECORD_FMT:
		/* get current record format */
		i2s0_get_fmt((unsigned long *)arg,CODEC_RMODE);

		break;

	case SND_DSP_SET_RECORD_FMT:
		/* set record format */
		ret = i2s0_set_fmt((unsigned long *)arg,CODEC_RMODE);
		if (ret < 0)
			break;
		/* if need reconfig the trigger, reconfig it */
		if (ret & NEED_RECONF_TRIGGER)
			i2s0_set_trigger(CODEC_RMODE);
		/* if need reconfig the dma_slave.max_tsz, reconfig it and
		   set the dp->need_reconfig_dma as true */
		if (ret & NEED_RECONF_DMA)
			i2s0_dma_need_reconfig(CODEC_RMODE);
		/* if need reconfig the filter, reconfig it */
		if (ret & NEED_RECONF_FILTER)
			i2s0_set_filter(CODEC_RMODE);
		ret = 0;
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

static void i2s0_codec_work_handler(struct work_struct *work)
{
	cur_codec->codec_ctl(CODEC_IRQ_HANDLE,(unsigned long)(&(switch_data.work)));
}

static irqreturn_t i2s0_irq_handler(int irq, void *dev_id)
{
	unsigned long flags;
	irqreturn_t ret = IRQ_HANDLED;

	spin_lock_irqsave(&i2s0_irq_lock,flags);
	/* check the irq source */
	/* if irq source is codec, call codec irq handler */
#ifdef CONFIG_JZ4780_INTERNAL_CODEC
	if (read_inter_codec_irq()){
		queue_work(i2s0_work_queue, &i2s0_codec_work);
	}
#endif
	/* if irq source is aic0, process it here */
	/*noting to do*/

	spin_unlock_irqrestore(&i2s0_irq_lock,flags);

	return ret;
}

static int i2s0_init_pipe(struct dsp_pipe **dp , enum dma_data_direction direction,unsigned long iobase)
{
	if (*dp != NULL || dp == NULL)
		return 0;
	*dp = vmalloc(sizeof(struct dsp_pipe));
	if (*dp == NULL) {
		printk("i2s0 : init pipe fail vmalloc ");
		return -ENOMEM;
	}

	(*dp)->dma_config.direction = direction;
	(*dp)->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	(*dp)->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_2_BYTES;
	(*dp)->dma_type = JZDMA_REQ_I2S0;

	(*dp)->fragsize = FRAGSIZE_M;
	(*dp)->fragcnt = FRAGCNT_M;
	(*dp)->is_non_block = true;
	(*dp)->is_used = false;
	(*dp)->can_mmap =true;
	INIT_LIST_HEAD(&((*dp)->free_node_list));
	INIT_LIST_HEAD(&((*dp)->use_node_list));

	if (direction == DMA_TO_DEVICE) {
		(*dp)->dma_config.src_maxburst = 64;
		(*dp)->dma_config.dst_maxburst = 64;
		(*dp)->dma_config.dst_addr = iobase + AIC0DR;
		(*dp)->dma_config.src_addr = 0;
	} else if (direction == DMA_FROM_DEVICE) {
		(*dp)->dma_config.src_maxburst = 32;
		(*dp)->dma_config.dst_maxburst = 32;
		(*dp)->dma_config.src_addr = iobase + AIC0DR;
		(*dp)->dma_config.dst_addr = 0;
	} else
		return -1;

	return 0;
}

static int i2s0_global_init(struct platform_device *pdev)
{
	int ret = 0;
	struct resource *i2s0_resource = NULL;
	struct dsp_pipe *i2s0_pipe_out = NULL;
	struct dsp_pipe *i2s0_pipe_in = NULL;
	struct clk *i2s0_clk = NULL;
	struct clk *codec_sysclk = NULL;

	i2s0_resource = platform_get_resource(pdev,IORESOURCE_MEM,0);
	if (i2s0_resource == NULL) {
		printk("i2s0: platform_get_resource fail.\n");
		return -1;
	}

	/* map io address */
	if (!request_mem_region(i2s0_resource->start, resource_size(i2s0_resource), pdev->name)) {
		printk("i2s0: mem region fail busy .\n");
		return -EBUSY;
	}
	i2s0_iomem = ioremap(i2s0_resource->start, resource_size(i2s0_resource));
	if (!i2s0_iomem) {
		printk ("i2s0: ioremap fail.\n");
		ret =  -ENOMEM;
		goto __err_ioremap;
	}

	ret = i2s0_init_pipe(&i2s0_pipe_out,DMA_TO_DEVICE,i2s0_resource->start);
	if (ret < 0)
		goto __err_init_pipeout;
	ret = i2s0_init_pipe(&i2s0_pipe_in,DMA_FROM_DEVICE,i2s0_resource->start);
	if (ret < 0)
		goto __err_init_pipein;

	i2s0_endpoints.out_endpoint = i2s0_pipe_out;
	i2s0_endpoints.in_endpoint = i2s0_pipe_in;

	/* request aic clk */
	i2s0_clk = clk_get(&pdev->dev, "aic0");
	if (IS_ERR(i2s0_clk)) {
		dev_dbg(&pdev->dev, "aic0 clk_get failed\n");
		goto __err_aic_clk;
	}
	clk_enable(i2s0_clk);

	spin_lock_init(&i2s0_irq_lock);

#if defined(CONFIG_JZ_INTERNAL_CODEC)
	i2s0_match_codec("internal_codec");
#elif defined(CONFIG_JZ_EXTERNAL_CODEC)
	i2s0_match_codec("i2s0_external_codec");
#else
	dev_info("WARNING: unless one codec must be select for i2s0.\n");
#endif
	if (cur_codec == NULL) {
		ret = 0;
		goto __err_match_codec;
	}

	/* request irq */
	i2s0_resource = platform_get_resource(pdev,IORESOURCE_IRQ,0);
	if (i2s0_resource == NULL) {
		ret = -1;
		goto __err_irq;
	}

	ret = request_irq(i2s0_resource->start, i2s0_irq_handler,
					  IRQF_DISABLED, "AIC0_irq", NULL);
	if (ret < 0)
		goto __err_irq;


	__i2s0_disable();
	schedule_timeout(5);
	__i2s0_disable();
	__i2s0_stop_bitclk();
	__i2s0_stop_ibitclk();
	/*select i2s trans*/
	__aic0_select_i2s();
	__i2s0_select_i2s();


#if defined(CONFIG_JZ_EXTERNAL_CODEC)
	__i2s0_external_codec();
#endif

	if (cur_codec->codec_mode == CODEC_MASTER) {
#if defined(CONFIG_JZ_INTERNAL_CODEC)
		__i2s0_internal_codec_master();
#endif
		__i2s0_slave_clkset();
	} else if (cur_codec->codec_mode == CODEC_SLAVE) {
#if defined(CONFIG_JZ_INTERNAL_CODEC)
		__i2s0_internal_codec_slave();
#endif
		__i2s0_master_clkset();
	}

	/*sysclk output*/
	__i2s0_enable_sysclk_output();

	/*set sysclk output for codec*/
	codec_sysclk = clk_get(&pdev->dev,"cgu_aic");
	if (IS_ERR(codec_sysclk)) {
		dev_dbg(&pdev->dev, "cgu_aic clk_get failed\n");
		goto __err_codec_clk;
	}
	/*set sysclk output for codec*/
	clk_set_rate(codec_sysclk,cur_codec->codec_clk);
	if (clk_get_rate(codec_sysclk) > cur_codec->codec_clk) {
		printk("codec interface set rate fail.\n");
		goto __err_codec_clk;
	}
	clk_enable(codec_sysclk);


	__i2s0_start_bitclk();
	__i2s0_start_ibitclk();

	__i2s0_disable_receive_dma();
	__i2s0_disable_transmit_dma();
	__i2s0_disable_record();
	__i2s0_disable_replay();
	__i2s0_disable_loopback();
	//__i2s0_flush_rfifo();
	__i2s0_flush_tfifo();
	__i2s0_clear_ror();
	__i2s0_clear_tur();
	__i2s0_set_receive_trigger(3);
	__i2s0_set_transmit_trigger(4);
	__i2s0_disable_overrun_intr();
	__i2s0_disable_underrun_intr();
	__i2s0_disable_transmit_intr();
	__i2s0_disable_receive_intr();
	__i2s0_send_rfirst();
	/* play zero or last sample when underflow */
	__i2s0_play_lastsample();

	__i2s0_enable();
	return  cur_codec->codec_ctl(CODEC_INIT,0);

__err_codec_clk:
	clk_put(codec_sysclk);
__err_irq:
	clk_disable(i2s0_clk);
	clk_put(i2s0_clk);
__err_aic_clk:
__err_match_codec:
	vfree(i2s0_pipe_in);
__err_init_pipein:
	vfree(i2s0_pipe_out);
__err_init_pipeout:
	iounmap(i2s0_iomem);
__err_ioremap:
	release_mem_region(i2s0_resource->start,resource_size(i2s0_resource));
	return ret;
}

static int i2s0_init(struct platform_device *pdev)
{
	int ret = -EINVAL;

#ifdef CONFIG_ANDROID
	register_early_suspend(&jz_i2s_early_suspend);
#endif
	spin_lock_init(&i2s0_hp_detect_state);
	ret = i2s0_global_init(pdev);

	return ret;
}

static void i2s0_shutdown(struct platform_device *pdev)
{
	/* close i2s0 and current codec */

	free_irq(IRQ_AIC0,NULL);
	if (cur_codec) {
		cur_codec->codec_ctl(CODEC_SHUTDOWN,CODEC_RWMODE);
		cur_codec->codec_ctl(CODEC_TURN_OFF,CODEC_RWMODE);
	}

	return;
}

static int i2s0_suspend(struct platform_device *pdev, pm_message_t state)
{
	if (cur_codec)
		cur_codec->codec_ctl(CODEC_SUSPEND,0);
#ifdef CONFIG_ANDROID
		is_i2s0_suspended = 1;
#endif
	return 0;
}

static int i2s0_resume(struct platform_device *pdev)
{
#ifndef CONFIG_ANDROID
	if (cur_codec)
		cur_codec->codec_ctl(CODEC_RESUME,0);
#endif
	return 0;
}

/*
 * headphone detect switch function
 *
 */
void jz_set_hp0_switch_state(int state)
{
	spin_lock(&i2s0_hp_detect_state);
	jz_switch_state = state;
	spin_unlock(&i2s0_hp_detect_state);
}

static int jz_get_hp0_switch_state(void)
{
	return jz_switch_state;
}

void jz_set_hp0_detect_type(int type,unsigned int gpio)
{
	switch_data.type = type;
	if (type == SND_SWITCH_TYPE_GPIO)
		switch_data.gpio = gpio;
	else
		switch_data.gpio = 0;
}

static struct snd_switch_data switch_data = {
	.sdev = {
		.name = "i2s0_hp_detect",
	},
	.state_on	=	"1",
	.state_off	=	"0",
	.codec_get_sate	=	jz_get_hp0_switch_state,
	.valid_level = 0,
	.type	=	SND_SWITCH_TYPE_CODEC,
};

static struct platform_device xb47xx_i2s0_switch = {
	.name	= DEV_DSP_HP_DET_NAME,
	.id		= SND_DEV_DETECT0_ID,
	.dev	= {
		.platform_data	= &switch_data,
	},
};

struct snd_dev_data i2s0_data = {
	.dev_ioctl	   	= i2s0_ioctl,
	.ext_data		= &i2s0_endpoints,
	.minor			= SND_DEV_DSP0,
	.init			= i2s0_init,
	.shutdown		= i2s0_shutdown,
	.suspend		= i2s0_suspend,
	.resume			= i2s0_resume,
};

static int __init init_i2s0(void)
{
	INIT_WORK(&i2s0_codec_work, i2s0_codec_work_handler);

	i2s0_work_queue = create_singlethread_workqueue("i2s0_codec_irq_wq");

	if (!i2s0_work_queue) {
		// this can not happen, if happen, we die!
        BUG();
	}

	return platform_device_register(&xb47xx_i2s0_switch);
}
module_init(init_i2s0);
