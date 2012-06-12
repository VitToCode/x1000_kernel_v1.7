/**
 * xb_snd_i2s0.c
 *
 * jbbi <jbbi@ingenic.cn>
 *
 * 24 APR 2012
 *
 */
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/clk.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>
#include <mach/jzdma.h>
#include "../xb_snd_dsp.h"
#include "xb47xx_i2s0.h"

/**
 * global variable
 **/
static LIST_HEAD(codecs_head);
static int is_i2s_suspend = 0;
static struct clk* i2s0_clk,codec_sys_clk;
static spinlock_t i2s0_irq_lock;

static struct codec_info {
	struct list_head *list;
	char *name;
	unsigned long record_audio_rate;
	unsigned long replay_audio_rate;
	int record_codec_channel;
	int replay_codec_channel;
	int record_format;
	int replay_format;
	int (*codec_ctl)(unsigned int cmd, unsigned long arg);
	struct dsp_endpoints *dsp_endpoints;
} *cur_codec;

static bool is_init = false;

/*##################################################################*\
 |* suspand func
\*##################################################################*/
static int i2s0_suspend(struct platform_device *, pm_message_t state);
static int i2s0_resume(struct platform_device *);
static void i2s0_shutdown(struct platform_device *);

#ifdef CONFIG_ANDROID
static int is_i2s_suspended = 0;

static void i2s0_late_resume(struct early_suspend *h)
{
	if (is_i2s_suspended && cur_codec)
		cur_codec->codec_ctl(CODEC_RESUME, 0)

	is_i2s_suspended = 0;
}

static struct early_suspend jz_i2c_early_suspend = {
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

int i2s0_register_codec(char *name, void *codec_ctl)
{
	struct codec_info *tmp = kmalloc(sizeof(struct codec_info), GFP_KERNEL);
	struct list_head  *list = kmalloc(sizeof(struct list_head),GFP_KERNEL);
	if ((name != NULL) && (codec_ctl != NULL)) {
		tmp->list = list;
		tmp->name = name;
		tmp->codec_ctl = codec_ctl;
		tmp->dsp_endpoints = i2s0_endpoints;
		list_add_tail(tmp->list, &codecs_head);

		/* set the first registered codec as the default(current) */
		if (cur_codec == NULL)
			cur_codec = tmp;
		return 0;
	} else {
		return -1;
	}
}
EXPORT_SYMBOL(i2s0_register_codec);

struct void i2s0_match_codec(char *name)
  {
  struct codec_info *codec_info;
  struct list_head  *list,*tmp;

  list_for_each_safe(list,tmp,&codecs_head) {
  codec_info = container_of(list,struct codec_info,list);
  if (!strcmp(codec_info->name,name))
	  cur_codec = codec_info;
  }
}

/*#define i2s0_match_codec()	\
	codecs_head.next &&	\
	codecs_head.next != &codecs_head ?	\
	container_of(&codecs_head.next,struct codec_info,list) :	\
	NULL*/

bool i2s0_get_init()
{
	return is_init;
}
EXPORT_SYMBOL(i2s0_get_init);
/*##################################################################*\
|* dev_ioctl
\*##################################################################*/
static int dsp_set_fmt(unsigned long *format,int mode)
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
		data_width = 8
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
		__i2s0_enable_signadj();		//??
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
	case AFMT_S16_BE:
		data_width = 16;
		if (mode & CODEC_WMODE) {
			__i2s0_set_oss_sample_size(1);
			__i2s0_enable_byteswap();
		}
		if (mode == COEDC_RMODE) {
			__i2s0_set_iss_sample_size(1);
			*format = AFMT_S16_LE;
		}
		__i2s0_disable_signadj();
		break;
	default :
		printk("I2S0: there is unknown format %p.\n",*format);
		return = -EINVAL;
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
			ret |= NEED_RECONF_TRIGGER | NEED_RECONF_FILTER;
			//ret |= NEED_RECONF_DMA;
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
			//ret |= NEED_RECONF_DMA;
		}
	}

	return ret;
}

static int dsp_set_channel(int* channel,int mode)
{
	int ret = 0;

	if (!cur_codec)
		return -ENODEV;

	if (mode & CODEC_WMODE) {
		ret = cur_codec->codec_ctl(CODEC_SET_RECORD_CHANNEL,channel);
		if (ret < 0) {
			cur_codec->record_codec_channel = *channel;
			return ret;
		}
		if (*channel ==2 || *channel == 4||
			*channel ==6 || *channel == 8) {
			__i2s0_out_channel_select(*channels - 1);
			__i2s0_disable_mono2stereo();
		} else if (channel == 1) {
			__i2s0_out_channel_select(*channels - 1);
			__i2s0_enable_mono2stereo();
		} else
			return -EINVAL;
		if (cur_codec->replay_codec_channel != *channel) {
			cur_codec->replay_codec_channel = *channel;
			ret |= NEED_RECONF_FILTER;
		}
	}
	if (mode & CODEC_RMODE) {
		ret = cur_codec->codec_ctl(CODEC_SET_RECORD_CHANNEL,channel);
		if (ret < 0)
			return ret;
		if (cur_codec->record_codec_channel != *channel) {
			cur_codec->record_codec_channel = *channel;
			ret |= NEED_RECONF_FILTER;
		}
	}
	return ret;
}

static int dsp_set_rate(unsigned long *rate,int mode)
{
	int ret = 0;
	if (!cur_codec)
		return -ENODEV;
#if defined(CONFIG_INTERNAL_CODEC)
	if (mode & CODEC_WMODE) {
		ret = cur_codec->codec_ctl(CODEC_SET_REPLAY_RATE,rate);
		cur_codec->replay_rate = *rate;
	}
	if (mode & CODEC_RMODE) {
		ret = cur_codec->codec_ctl(CODEC_SET_RECORD_RATE,rate);
		cur_codec->record_rate = *rate;
	}
#else
	/*external codec*/
#endif
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

static void dsp_set_trigger(int mode)
{
	int data_width = 0;
	struct dsp_pipe *dp = NULL;
	int burst_length = 0;

	if (!cur_codec)
		return -ENODEV;

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
		burst_length = get_burst_length(sg_dma_address(sg)|sg_dma_len(len)|dp->dma_slave->max_tsz);
		if (I2S0_TX_FIFO_DEPTH - burst_length/data_width >= 60)
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
		burst_length = get_burst_length(sg_dma_address(sg)|sg_dma_len(len)|dp->dma_slave->max_tsz);
		__i2s0_set_receive_trigger(((I2S0_TX_FIFO_DEPTH - burst_length/data_width) >> 1) - 1);
	}

	return;
}
static void dsp_set_default_route(int mode)		//FIXME
{

	if (!cur_codec)
		return -ENODEV;

	if (mode & CODEC_WRMODE) {
		cur_codec->codec_ctl(CODEC_SET_ROUTE, DEFAULT_REPLAY_ROUTE);
	} else if (mode & CODEC_WMODE) {
		cur_codec->codec_ctl(CODEC_SET_ROUTE, DEFAULT_REPLAY_ROUTE);
	} else if (mode & CODEC_RMODE){
		cur_codec->codec_ctl(CODEC_SET_ROUTE, DEFAULT_RECORD_ROUTE);
	}

	return;
}

static int dsp_enable(int mode)
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
		dsp_set_fmt(&replay_format,mode);
		dsp_set_channel(&replay_rate,mode);
		dsp_set_rate(&rate,mode);
		dsp_set_trigger(mode);
		dp_other = cur_codec->dsp_endpoints->in_endpoint;
	}
	if (mode & CODEC_RMODE) {
		dsp_set_fmt(&record_format,mode);
		dsp_set_channel(&record_rate,mode);
		dsp_set_rate(&rate,mode);
		dsp_set_trigger(mode);
		dp_other = cur_codec->dsp_endpoints->out_endpoint;
	}

	dsp_set_default_route(mode);
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

static int dsp_disable(int mode)			//CHECK codec is suspend?
{
/*	if (!cur_codec)
		return -ENODEV;*/
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


static int dsp_dma_enable(int mode)		//CHECK
{
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
		__i2s0_flush_rfifo();
		__i2s0_enable_record();
		/* read the first sample and ignore it */
		__i2s0_read_rfifo();
		__i2s0_enable_receive_dma();
	}

	return 0;
}

static int dsp_dma_disable(int mode)		//CHECK seq dma and func
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

static int dsp_get_fmt_cap(unsigned long *fmt_cap,int mode)
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


static int dsp_get_fmt(unsigned long *fmt, int mode)
{
	if (!cur_codec)
			return -ENODEV;

	if (mode & CODEC_WMODE)
		*fmt = cur_codec->replay_format;
	if (mode & CODEC_RMODE)
		*fmt = cur_codec->record_format;

	return 0;
}

static void dsp_dma_need_reconfig(int mode)
{
	struct dsp_pipe	*dp = NULL;

	if (!cur_codec)
			return -ENODEV;
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
		ret = dsp_enable(CODEC_WMODE);
		break;

	case SND_DSP_DISABLE_REPLAY:
		/* disable i2s0 replay */
		ret = dsp_disable(CODEC_WMODE);
		break;

	case SND_DSP_ENABLE_RECORD:
		/* enable i2s0 record */
		/* set i2s0 default record format, channels, rate */
		/* set default record route */
		ret = dsp_enable(CODEC_RMODE);
		break;

	case SND_DSP_DISABLE_RECORD:
		/* disable i2s0 record */
		ret = dsp_disable(CODEC_RMODE);
		break;

	case SND_DSP_ENABLE_DMA_RX:
		ret = dsp_dma_enable(CODEC_RMODE);
		break;

	case SND_DSP_DISABLE_DMA_RX:
		ret = dsp_dma_disable(CODEC_RMODE);
		break;

	case SND_DSP_ENABLE_DMA_TX:
		ret = dsp_dma_enable(CODEC_WMODE);
		break;

	case SND_DSP_DISABLE_DMA_TX:
		ret = dsp_dma_disable(CODEC_WMODE);
		break;

	case SND_DSP_SET_REPLAY_RATE:
		ret = dsp_set_rate((unsigned long *)arg,CODEC_WMODE);
		break;

	case SND_DSP_SET_RECORD_RATE:
		ret = dsp_set_rate((unsigned long *)arg,CODEC_RMODE);
		break;

	case SND_DSP_SET_REPLAY_CHANNELS:
		/* set replay channels */
		ret = dsp_set_channel((int *)arg,CODEC_WMODE);
		if (ret < 0)
			break;
		/* if need reconfig the filter, reconfig it */
		/* dont need filter */
		if (ret & NEED_RECONF_FILTER)
			ret = 0;
		break;

	case SND_DSP_SET_RECORD_CHANNELS:
		/* set record channels */
		ret = dsp_set_channel((int*)arg,CODEC_RMODE);
		if (ret < 0)
			break;
		/* if need reconfig the filter, reconfig it */
		if (ret & NEED_RECONF_FILTER)
			ret = 0;
		break;

	case SND_DSP_GET_REPLAY_FMT_CAP:
		/* return the support replay formats */
		ret = dsp_get_fmt_cap((unsigned long *)arg,CODEC_WMODE);
		break;

	case SND_DSP_GET_REPLAY_FMT:
		/* get current replay format */
		dsp_get_fmt((unsigned long *)arg,CODEC_WMODE);
		break;

	case SND_DSP_SET_REPLAY_FMT:
		/* set replay format */
		ret = dsp_set_fmt((unsigned long *)arg,CODEC_WMODE);
		if (ret < 0)
			break;
		/* if need reconfig the trigger, reconfig it */
		if (ret & NEED_RECONF_TRIGGER)
			dsp_set_trigger(CODEC_WMODE);
		/* if need reconfig the dma_slave.max_tsz, reconfig it and
		   set the dp->need_reconfig_dma as true */
		if (ret & NEED_RECONF_DMA)
			dsp_dma_need_reconfig(CODEC_WMODE);
		/* if need reconfig the filter, reconfig it */
		if (ret & NEED_RECONF_FILTER)
			;
		ret = 0;
		break;

	case SND_DSP_GET_RECORD_FMT_CAP:
		/* return the support record formats */
		ret = dsp_get_fmt_cap((unsigned long *)arg,CODEC_RMODE);
		break;

	case SND_DSP_GET_RECORD_FMT:
		/* get current record format */
		dsp_get_fmt((unsigned long *)arg,CODEC_RMODE);

		break;

	case SND_DSP_SET_RECORD_FMT:
		/* set record format */
		ret = dsp_set_fmt((unsigned long *)arg,CODEC_RMODE);
		if (ret < 0)
			break;
		/* if need reconfig the trigger, reconfig it */
		if (ret & NEED_RECONF_TRIGGER)
			dsp_set_trigger(CODEC_RMODE);
		/* if need reconfig the dma_slave.max_tsz, reconfig it and
		   set the dp->need_reconfig_dma as true */
		if (ret & NEED_RECONF_DMA)
			dsp_dma_need_reconfig(CODEC_RMODE);
		/* if need reconfig the filter, reconfig it */
		if (ret & NEED_RECONF_FILTER)
			;
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
#define INTERNAL_CODEC_SYSCLK 12000000

static irqreturn_t i2s0_irq_handler(int irq, void *dev_id)
{
	unsigned long flags;
	irqreturn_t ret = IRQ_HANDLED;
	int reval;
	spin_lock_irqsave(&i2s0_irq_lock,flags);
	/* check the irq source */
	/* if irq source is codec, call codec irq handler */
	if (cur_codec->codec_ctl(CODEC_IRQ_DETECT,0)) {
		reval = cur_codec->codec_ctl(CODEC_IRQ_HANDLE,(unsigned long *)(&(switch_data.work)));
		if (reval)
			ret = IRQ_NONE;
	}
	/* if irq source is aic0, process it here */
	/*noting to do*/

	spin_unlock_irqrestore(&i2s0_irq_lock,flags);

	return ret;
}

int dsp_global_init(platform_device *pdev)
{
	/* map io address */
	if (!request_mem_region(I2S0_IOBASE, I2S0_IOSIZE, pdev->name))
		return = -EBUSY;

	i2s0_iomem = ioremap(I2S0_IOBASE, I2S0_IOSIZE);
	if (!i2s0_iomem)
		return  = -ENOMEM;

	i2s0_match_codec();
	if (cur_codec == NULL)
		return 0;

	/* request clk */
	i2s0_clk = clk_get(&pdev->dev, "aic");
	clk_set_rate(i2s0_clk, JZ_EXTAL1);
	clk_enable(i2s0_clk);

	spin_lock_init(&i2s0_irq_lock);
	/* request irq */
	ret = request_irq(IRQ_AIC0, i2s0_irq_handler,
					  IRQF_DISABLED, "AIC0_irq", NULL);
	if (ret < 0)
		goto error;

	__i2s0_disable();
	schedule_timeout(5);
	__i2s0_disable();

#if defined(CONFIG_INTERNAL_CODEC)
	__i2s0_as_slave();
	__i2s0_internal_codec();
#elif defined(CONFIG_EXTERNAL_CLOCK)
	/*__i2s0_external_codec();*/
#endif

	__aic0_select_i2s();

	__i2s0_select_i2s();
#if defined(CONFIG_INTERNAL_CODEC)
	/*sysclk output*/
	__i2s0_enable_sysclk_output();

	/*set sysclk output for codec*/
	codec_sys_clk = clk_get(&pdev->dev,"cgu_i2s");
	clk_set_rate(codec_sys_clk,INTERNAL_CODEC_SYSCLK);
	clk_enable(codec_sys_clk);

	/*bclk and sync input from codec*/
	__i2s0_internal_clkset();
#elif defined(CONFIG_EXTERNAL_CLOCK)
	/**/
#endif

	/* internal or external codec */

	__i2s0_disable_receive_dma();
	__i2s0_disable_transmit_dma();
	__i2s0_disable_record();
	__i2s0_disable_replay();
	__i2s0_disable_loopback();
	__i2s0_set_receive_trigger(3);
	__i2s0_set_transmit_trigger(4);

	__i2s0_send_rfirst();

	/* play zero or last sample when underflow */
	__i2s0_play_lastsample();
	__i2s0_enable();

	is_init = true;
	return  cur_codec->codec_ctl(CODEC_INIT,0);
}

EXPORT_SYMBOL(dsp_global_init);

static int i2s0_init(struct platform_device *pdev)
{
	int ret = -EINVAL;
	int fragsize,fragtotal;

#ifdef CONFIG_ANDROID
	register_early_suspend(&jz_i2s_early_suspend);
#endif
	spin_lock_init(&i2s0_hp_detect_state);
	ret = dsp_global_init();

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
	is_i2s_suspend = 1;
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
void set_switch_state(int state)
{
	spin_lock(&i2s0_hp_detect_state);
	jz_switch_state = state;
	spin_unlock(&i2s0_hp_detect_state);
}

static int jz_get_switch_state(void)
{
	return jz_switch_state;
}

struct snd_switch_data switch_data = {
	.sdev = {
		.name           = "i2s0_hp_detect",
	},
	.state_on	=	"1",
	.state_off	=	"0",
	.codec_get_sate	=	jz_get_switch_data,
	.type	=	SND_SWITCH_TYPE_CODEC,
};

static platform_device xb47xx_i2s0_switch = {
	.name	= DEV_DSP_HP_DET_NAME,
	.id		= SND_DEV_DETECT0_ID,
	.dev	= {
		.platform_data	= &switch_data;
	},
}

static struct dsp_pipe i2s0_pipe_out = {
	.dma_slave = {
		.reg_width = 2,
		.max_tsz = 64,
		.tx_reg = AIC_IOBASE + AIC0DR,
		.req_type_tx = JZDMA_REQ_I2S0_TX,
	},
	.dma_direction = DMA_TO_DEVICE;
	.fragsize = FRAGSIZE_M,
	.fragcnt = FRAGCNT_M,
	.is_non_block = true,
	.can_mmap = true,
};

static struct dsp_pipe i2s0_pipe_in = {
	.dma_slave = {
		.reg_width = 2,
		.max_tsz = 32,
		.rx_reg = AIC_IOBASE + AIC0DR,
		.req_type_rx = JZDMA_REQ_I2S0_RX,
	},
	.dma_direction = DMA_FROM_DEVICE;
	.fragsize = FRAGSIZE_M,
	.fragcnt = FRAGCNT_M,
	.is_non_block = true,
	.can_mmap = true,
};

static struct dsp_endpoints i2s0_endpoints = {
	.out_endpoint = &i2s0_pipe_out,
	.in_endpoint = &i2s0_pipe_in,
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
	return platform_device_register(&xb47xx_i2s0_switch);
}

arch_initcall(init_i2s0);
