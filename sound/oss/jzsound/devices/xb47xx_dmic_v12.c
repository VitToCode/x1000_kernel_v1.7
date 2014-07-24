/**
 * xb_snd_dmic.c
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
#include <linux/wait.h>
#include <mach/jzdma.h>
#include <mach/jzsnd.h>
#include <soc/irq.h>
#include <soc/base.h>
#include "xb47xx_dmic_v12.h"
#include "xb47xx_i2s_v12.h"
#include <linux/delay.h>
#include <soc/cpm.h>
/**
 * global variable
 **/
void volatile __iomem *volatile dmic_iomem;
//static volatile bool dmic_is_incall_state = false;
static LIST_HEAD(codecs_head);


//static spinlock_t dmic_irq_lock;
#define DMIC_IRQ
struct jz_dmic {
	unsigned long  rate_type;
};

//struct clk *dmic_clk = NULL;

struct dmic_device {

	int dmic_irq;
	spinlock_t dmic_irq_lock;

	char name[20];
	struct resource * res;
	struct clk * i2s_clk; /*i2s_clk*/
	struct clk * clk; /*dmic clk*/
	struct clk * pwc_clk;
	volatile bool dmic_is_incall_state;
	void __iomem * dmic_iomem;

	struct jz_dmic * cur_dmic;
};


static int dmic_set_private_data(struct snd_dev_data *ddata, struct dmic_device * dmic_dev)
{
	ddata->priv_data = (void *)dmic_dev;
	return 0;
}
struct dmic_device * dmic_get_private_data(struct snd_dev_data *ddata)
{
	return (struct dmic_device * )ddata->priv_data;
}

struct snd_dev_data * dmic_get_ddata(struct platform_device *pdev) {
	return pdev->dev.platform_data;
}

static struct dsp_endpoints dmic_endpoints;
static int dmic_global_init(struct platform_device *pdev);
bool dmic_is_incall(struct dmic_device * dmic_dev);

/*##################################################################*\
 | dump
\*##################################################################*/

static void dump_dmic_reg(struct dmic_device *dmic_dev)
{
	int i;
	unsigned long reg_addr[] = {
		DMICCR0, DMICGCR, DMICIMR, DMICINTCR, DMICTRICR, DMICTHRH,
		DMICTHRL, DMICTRIMMAX, DMICTRINMAX, DMICFTHR, DMICFSR, DMICCGDIS
	};

	for (i = 0;i < 12; i++) {
		printk("##### dmic reg0x%x,=0x%x.\n",
	   	(unsigned int)reg_addr[i],dmic_read_reg(reg_addr[i]));
	}
	printk("##### intc,=0x%x.\n",*((volatile unsigned int *)(0xb0001000)));
}

/*######DMICTRIMMAX############################################################*\
 |* suspDMICTRINMAXand func
\*##################################################################*/
static int dmic_suspend(struct platform_device *, pm_message_t state);
static int dmic_resume(struct platform_device *);
static void dmic_shutdown(struct platform_device *);

bool dmic_is_incall(struct dmic_device * dmic_dev)
{
	return dmic_dev->dmic_is_incall_state;
}

/*##################################################################*\
|* dev_ioctl
\*##################################################################*/
static int dmic_set_fmt(struct dmic_device * dmic_dev, unsigned long *format,int mode)
{

	int ret = 0;
	struct dsp_pipe *dp = NULL;

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
	debug_print("format = %d",*format);
	switch (*format) {
	case AFMT_S16_LE:
		break;
	case AFMT_S16_BE:
		break;
	default :
		printk("DMIC: dmic only support format 16bit 0x%x.\n",(unsigned int)*format);
		return -EINVAL;
	}

	if (mode & CODEC_RMODE) {
		dp = dmic_endpoints.in_endpoint;
		dp->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
		ret |= NEED_RECONF_TRIGGER;
		ret |= NEED_RECONF_DMA;
	}
	if (mode & CODEC_WMODE) {
		printk("DMIC: unsurpport replay!\n");
		ret = -1;
	}

	return ret;
}

/*##################################################################*\
|* filter opt
\*##################################################################*/
static void dmic_set_filter(struct dmic_device * dmic_dev, int mode , uint32_t channels)
{
	struct dsp_pipe *dp = NULL;

	if (mode & CODEC_RMODE)
		dp = dmic_endpoints.in_endpoint;
	else
		return;

	if (channels == 1) {
#if 0
		dp->filter = convert_16bits_stereomix2mono;
		printk("dp->filter convert_16bits_stereomix2mono\n");
		dp->filter = convert_32bits_stereo2_16bits_mono;
#else
		dp->filter = convert_16bits_stereo2mono;
		/*dp->filter = convert_32bits_stereo2_16bits_mono;*/
	//	printk("dp->filter convert_32bits_stereo2_16bits_mono\n");
#endif
	} else if (channels == 3){
		dp->filter = convert_32bits_2_20bits_tri_mode;
	} else if (channels == 2){
		dp->filter = NULL;
	} else {
		dp->filter = NULL;
		printk("dp->filter null\n");
	}
}

#if 0
#define I2S_RX_FIFO_DEPTH       32

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
#endif
static int dmic_set_trigger(struct dmic_device * dmic_dev, int mode)
{

	if (mode & CODEC_RMODE) {

		__dmic_enable_rdms();
#if 0
		burst_length = get_burst_length((int)dp->paddr|(int)dp->fragsize|
				dp->dma_config.src_maxburst|dp->dma_config.dst_maxburst);
		if (I2S_RX_FIFO_DEPTH <= burst_length/data_width)
			__i2s_set_receive_trigger(((burst_length/data_width +1) >> 1) - 1);
		else
			__i2s_set_receive_trigger(8);
#endif
		__dmic_set_request(65);
		return 0;
	}


	if (mode & CODEC_WMODE) {
		return -1;
	}

	return 0;
}

static int dmic_set_voice_trigger(struct dmic_device * dmic_dev, unsigned long *THR ,int mode)
{
	int ret = 0;

	printk("THR = %ld\n",*THR);
	 if(mode & CODEC_RMODE) {

		clk_enable(dmic_dev->clk);
		__dmic_clear_tur();
		__dmic_clear_trigger();
		__dmic_disable_empty_int();
		__dmic_disable_interface();
		__dmic_set_tri_mode(2);
	//	__dmic_set_thr_high(40000);
		if(*THR != 0)
			__dmic_set_thr_low(*THR);
		else
			__dmic_set_thr_low(5000);
		//__dmic_set_m_max(500);
		//__dmic_set_n_max(2);
		__dmic_enable_hpf2();
		__dmic_enable_wake_int();
		__dmic_enable_tri_int();
		__dmic_enable_prerd_int();
		__dmic_enable_full_int();
		__dmic_clear_tur();
		__dmic_clear_trigger();
		__dmic_reset();
		while(__dmic_get_reset());
		__dmic_enable_rdms();
		//__dmic_set_request(65);
		__dmic_enable_tri();
		clk_disable(dmic_dev->clk);
	}

	if (mode & CODEC_WMODE) {
		return -1;

	}

	return ret;
}

static int dmic_set_channel(struct dmic_device * dmic_dev, int* channel,int mode)
{
	int ret = 0;

	debug_print("channel = %d",*channel);
	if (mode & CODEC_RMODE) {
		if (*channel == 2) {
			__dmic_select_stereo();
			__dmic_disable_pack();
			__dmic_disable_unpack_dis();
			dmic_set_filter(dmic_dev, CODEC_RMODE, 0);
			//__dmic_enable_split_lr();
		} else  {
			__dmic_select_mono();
			__dmic_enable_pack();
			__dmic_enable_unpack_dis();
			//dmic_set_filter(CODEC_RMODE, 1);
		}
	}
	if (mode & CODEC_WMODE) {
		return -1;
	}

	return ret;
}

/***************************************************************\
 *  Use codec slave mode clock rate list
 *  We do not hope change EPLL,so we use 270.67M (fix) epllclk
 *  for minimum error
 *  270.67M ---	M:203 N:9 OD:1
 *	 rate	 dmicdr	 cguclk		 dmicdv.div	samplerate/error
 *	|192000	|1		|135.335M	|10			|+0.12%
 *	|96000	|3		|67.6675M	|10			|+0.12%
 *	|48000	|7		|33.83375M	|10			|-0.11%
 *	|44100	|7		|33.83375M	|11			|-0.10%
 *	|32000	|11		|22.555833M	|10			|+0.12%
 *	|24000	|15		|16.916875M	|10			|+0.12%
 *	|22050	|15		|16.916875M	|11			|-0.12%
 *	|16000	|23		|11.277916M	|10			|+0.12%
 *	|12000  |31		|8.458437M	|10			|+0.12%
 *	|11025	|31		|8.458437M	|11			|-0.10%
 *	|8000	|47		|5.523877M	|10			|+0.12%
 *	HDMI:
 *	sysclk 11.2896M (theoretical)
 *	dmicdr  23
 *	cguclk 11.277916M (practical)
 *	error  -0.10%
\***************************************************************/
/*static unsigned long calculate_cgu_dmic_rate(unsigned long *rate)
{
	int i;
	unsigned long mrate[10] = {
		8000, 16000 ,48000,
	};
	for (i=0; i<3; i++) {
		if (*rate <= mrate[i]) {
			*rate = mrate[i];
			break;
		}
	}
	if (i >= 3) {
		*rate = 8000; [>unsupport rate use default<]
		return mrate[0];
	}

	return mrate[i];
}*/

static int dmic_set_rate(struct dmic_device * dmic_dev, unsigned long *rate,int mode)
{
	int ret = 0;
	struct jz_dmic * cur_dmic = dmic_dev->cur_dmic;
	debug_print("rate = %ld",*rate);
	if (mode & CODEC_WMODE) {
		/*************************************************\
		|* WARING:dmic have only one mode,               *|
		|* So we should not to care write mode.          *|
		\*************************************************/
		printk("dmic unsurpport replay!\n");
		return -EPERM;
	}
	if (mode & CODEC_RMODE) {
		if (*rate == 8000){
			cur_dmic->rate_type = 0;
			__dmic_select_8k_mode();
			__dmic_select_prefetch_8k();
		}
		else if (*rate == 16000){
			cur_dmic->rate_type = 1;
			__dmic_select_16k_mode();
			__dmic_select_prefetch_16k();
		}
		else if (*rate == 48000){
			__dmic_select_48k_mode();
		}
		else
			printk("DMIC: unsurpport samplerate: %ld\n", *rate);
	}

	return ret;
}
#define DMIC_TX_FIFO_DEPTH		64

/*static int get_burst_length(unsigned long val)
{
	[> burst bytes for 1,2,4,8,16,32,64 bytes <]
	int ord;

	ord = ffs(val) - 1;
	if (ord < 0)
		ord = 0;
	else if (ord > 6)
		ord = 6;

	[> if tsz == 8, set it to 4 <]
	return (ord == 3 ? 4 : 1 << ord)*8;
}*/

static int dmic_record_deinit(struct dmic_device * dmic_dev, int mode)
{
	__dmic_clear_trigger();
	__dmic_clear_tur();
	__dmic_disable_rdms();

	return 0;
}

static int dmic_record_init(struct dmic_device * dmic_dev, int mode)
{
	int rst_test = 50000;

	if (mode & CODEC_WMODE) {
		printk("-----> unsurport !\n");
		return -1;
	}

	__dmic_select_8k_mode();

	__dmic_reset();
	while(__dmic_get_reset()) {
		if (rst_test-- <= 0) {
			printk("-----> rest dmic failed!\n");
			return -1;
		}
	}

	printk("=====> %s success\n", __func__);

	return 0;
}

static int dmic_enable(struct dmic_device * dmic_dev, int mode)
{
	unsigned long record_rate = 8000;
	unsigned long record_format = 16;
	/*int record_channel = DEFAULT_RECORD_CHANNEL;*/
	struct dsp_pipe *dp_other = NULL;
	int ret = 0;
	clk_enable(dmic_dev->clk);
	if (mode & CODEC_WMODE)
		return -1;

	ret = dmic_record_init(dmic_dev, mode);
	if (ret)
		return -1;

	if (mode & CODEC_RMODE) {
		printk("come to %s %d set dp_other\n", __func__, __LINE__);
		dp_other = dmic_endpoints.in_endpoint;
		dmic_set_fmt(dmic_dev, &record_format,mode);
		dmic_set_rate(dmic_dev, &record_rate,mode);
	}

	__dmic_set_gm(9);
	__dmic_disable_empty_int();
//	__dmic_disable_lp_mode();
	__dmic_enable_hpf();
	__dmic_disable_prerd_int();
	__dmic_disable_prefetch();
	__dmic_disable_tri_int();
	__dmic_disable_wake_int();
	__dmic_enable_hpf2();
	__dmic_clear_trigger();
	__dmic_clear_tur();

#ifdef CONFIG_JZ_DMIC1
	__dmic_enable_split_lr();
#endif
	__dmic_enable();

	printk("4444444444444\n");
	dmic_dev->dmic_is_incall_state = false;

	return 0;
}

static int dmic_disable(struct dmic_device * dmic_dev, int mode)
{
	dmic_record_deinit(dmic_dev, mode);
	__dmic_disable();

	return 0;
}

static int dmic_dma_enable(struct dmic_device * dmic_dev, int mode)		//CHECK
{
	if (mode & CODEC_RMODE) {
//		__dmic_reset();
//		while(__dmic_get_reset());

		__dmic_set_request(65);
		__dmic_enable_rdms();
		__dmic_clear_trigger();
		__dmic_clear_tur();
	}
	if (mode & CODEC_WMODE) {
		printk("DMIC: dmic unsurpport replay\n");
		return -1;
	}

	return 0;
}

static int dmic_dma_disable(struct dmic_device * dmic_dev, int mode)		//CHECK seq dma and func
{
	if (mode & CODEC_RMODE) {
		__dmic_disable_rdms();
	}
	else
		return -1;

	return 0;
}

static int dmic_get_fmt_cap(struct dmic_device * dmic_dev, unsigned long *fmt_cap,int mode)
{
	unsigned long dmic_fmt_cap = 0;
	if (mode & CODEC_WMODE) {
		return -1;
	}
	if (mode & CODEC_RMODE) {
		dmic_fmt_cap |= AFMT_S16_LE|AFMT_S16_BE;
	}

	if (*fmt_cap == 0)
		*fmt_cap = dmic_fmt_cap;
	else
		*fmt_cap &= dmic_fmt_cap;

	return 0;
}


static int dmic_get_fmt(struct dmic_device * dmic_dev, unsigned long *fmt, int mode)
{
	if (mode & CODEC_WMODE)
		return -1;
	if (mode & CODEC_RMODE)
		*fmt = AFMT_S16_LE;

	return 0;
}

static void dmic_dma_need_reconfig(struct dmic_device * dmic_dev, int mode)
{
	struct dsp_pipe	*dp = NULL;

	if (mode & CODEC_RMODE) {
		dp = dmic_endpoints.in_endpoint;
		dp->need_reconfig_dma = true;
	}
	if (mode & CODEC_WMODE) {
		printk("DMIC: unsurpport replay mode\n");
	}

	return;
}


static int dmic_set_device(struct dmic_device * dmic_dev, unsigned long device)
{
	return 0;
}

/********************************************************\
 * dev_ioctl
\********************************************************/
static long dmic_ioctl(struct snd_dev_data *ddata, unsigned int cmd, unsigned long arg)
{
	long ret = 0;
	struct dmic_device * dmic_dev = dmic_get_private_data(ddata);
	switch (cmd) {
	case SND_DSP_ENABLE_REPLAY:
		/* enable dmic replay */
		/* set dmic default record format, channels, rate */
		/* set default replay route */
		printk("dmic not support replay!\n");
		ret = -1;
		break;

	case SND_DSP_DISABLE_REPLAY:
		printk("dmic not support replay!\n");
		ret = -1;
		/* disable dmic replay */
		break;

	case SND_DSP_ENABLE_RECORD:
		/* enable dmic record */
		/* set dmic default record format, channels, rate */
		/* set default record route */
		printk("  dmic ioctl enable cmd\n");
		ret = dmic_enable(dmic_dev, CODEC_RMODE);
		printk("  dmic ioctl enable cmd complete\n");
		break;

	case SND_DSP_DISABLE_RECORD:
		/* disable dmic record */
		ret = 0;
		ret = dmic_disable(dmic_dev, CODEC_WMODE);
		break;

	case SND_DSP_ENABLE_DMA_RX:
		ret = dmic_dma_enable(dmic_dev, CODEC_RMODE);
		break;

	case SND_DSP_DISABLE_DMA_RX:
		ret = 0;
		ret = dmic_dma_disable(dmic_dev, CODEC_RMODE);
		break;

	case SND_DSP_ENABLE_DMA_TX:
		printk("dmic not support replay!\n");
		ret = -1;
		break;

	case SND_DSP_DISABLE_DMA_TX:
		ret = -1;
		printk("dmic not support replay!\n");
		break;

	case SND_DSP_SET_REPLAY_RATE:
		ret = -1;
		printk("dmic not support replay!\n");
		break;

	case SND_DSP_SET_RECORD_RATE:
		ret = 0;
		ret = dmic_set_rate(dmic_dev, (unsigned long *)arg,CODEC_RMODE);
		break;

	case SND_DSP_GET_REPLAY_RATE:
		ret = -1;
		printk("dmic not support replay!\n");
		break;

	case SND_DSP_GET_RECORD_RATE:
		ret = 0;
		break;


	case SND_DSP_SET_REPLAY_CHANNELS:
		/* set replay channels */
		printk("dmic not support replay!\n");
		ret = -1;
		break;

	case SND_DSP_SET_RECORD_CHANNELS:
		ret = 0;
		ret = dmic_set_channel(dmic_dev, (int *)arg, CODEC_RMODE);
		/* set record channels */
		break;

	case SND_DSP_GET_REPLAY_CHANNELS:
		printk("dmic not support record!\n");
		ret = -1;
		break;

	case SND_DSP_GET_RECORD_CHANNELS:
		ret = 0;
		break;

	case SND_DSP_GET_REPLAY_FMT_CAP:
		ret = -1;
		printk("dmic not support replay!\n");
		/* return the support replay formats */
		break;

	case SND_DSP_GET_REPLAY_FMT:
		/* get current replay format */
		ret = -1;
		printk("dmic not support replay!\n");
		break;

	case SND_DSP_SET_REPLAY_FMT:
		/* set replay format */
		printk("dmic not support replay!\n");
		ret = -1;
		break;

	case SND_DSP_GET_RECORD_FMT_CAP:
		/* return the support record formats */
		ret = 0;
		ret = dmic_get_fmt_cap(dmic_dev, (unsigned long *)arg,CODEC_RMODE);
		break;

	case SND_DSP_GET_RECORD_FMT:
		ret = 0;
		ret = dmic_get_fmt(dmic_dev, (unsigned long *)arg,CODEC_RMODE);
		/* get current record format */

		break;

	case SND_DSP_SET_RECORD_FMT:
		/* set record format */
		ret = dmic_set_fmt(dmic_dev, (unsigned long *)arg,CODEC_RMODE);
		if (ret < 0)
			break;
	//	[> if need reconfig the trigger, reconfig it <]
		if (ret & NEED_RECONF_TRIGGER)
			dmic_set_trigger(dmic_dev, CODEC_RMODE);
	//	[> if need reconfig the dma_slave.max_tsz, reconfig it and
	//	   set the dp->need_reconfig_dma as true <]
		if (ret & NEED_RECONF_DMA)
			dmic_dma_need_reconfig(dmic_dev, CODEC_RMODE);
		ret = 0;

		break;

	case SND_MIXER_DUMP_REG:
		dump_dmic_reg(dmic_dev);
		break;
	case SND_MIXER_DUMP_GPIO:
		break;

	case SND_DSP_SET_STANDBY:
		break;

	case SND_DSP_SET_DEVICE:
		ret = dmic_set_device(dmic_dev, arg);
		break;
	case SND_DSP_SET_RECORD_VOL:
		break;
	case SND_DSP_SET_REPLAY_VOL:
		break;
	case SND_DSP_SET_MIC_VOL:
		break;
	case SND_DSP_CLR_ROUTE:
		break;
	case SND_DSP_SET_VOICE_TRIGGER:
		ret = 0;
		ret = dmic_set_voice_trigger(dmic_dev, (unsigned long *)arg,CODEC_RMODE);
		break;
	default:
		printk("SOUND_ERROR: %s(line:%d) unknown command!\n",
				__func__, __LINE__);
		ret = -EINVAL;
	}

	return ret;
}

/*##################################################################*\
  |* functions
  \*##################################################################*/
#ifdef DMIC_IRQ
static irqreturn_t dmic_irq_handler(int irq, void *dev_id)
{
	unsigned long flags;
	irqreturn_t ret = IRQ_HANDLED;
	struct dmic_device * dmic_dev = (struct dmic_device *)dev_id;

	spin_lock_irqsave(&dmic_dev->dmic_irq_lock,flags);
	if (__dmic_test_tri_int() || __dmic_test_wake_int()) {
		printk("AUDIO: trigger!\n");
		clk_enable(dmic_dev->clk);
		__dmic_clear_trigger();
		__dmic_set_rate(dmic_dev->cur_dmic->rate_type);
		__dmic_disable_tri_int();
		__dmic_disable_wake_int();
		__dmic_disable_tri();
		__dmic_clear_tur();
		__dmic_enable_empty_int();
		if (__dmic_test_full_int())
			printk("tri & full\n");
		__dmic_clear_tur();
	} else if (__dmic_test_prerd_int() || __dmic_test_empty_int() || __dmic_test_full_int()) {
		if (!__dmic_get_prerd_int() && __dmic_test_prerd_int()) {
			__dmic_disable_prerd_int();
			 printk("pre~\n\n\n");
		}

		if (!__dmic_get_empty_int() && __dmic_test_empty_int()) {
			__dmic_disable_empty_int();
			__dmic_clear_empty_flag();
		}
		if (__dmic_test_full_int())
			printk("tri & full\n");
	}

	spin_unlock_irqrestore(&dmic_dev->dmic_irq_lock,flags);

	return ret;
}
#endif

static int dmic_init_pipe(struct dsp_pipe **dp , enum dma_data_direction direction,unsigned long iobase)
{
	if (*dp != NULL || dp == NULL)
		return 0;
	*dp = vmalloc(sizeof(struct dsp_pipe));
	if (*dp == NULL) {
		return -ENOMEM;
	}

	(*dp)->dma_config.direction = direction;
	(*dp)->dma_config.src_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	(*dp)->dma_config.dst_addr_width = DMA_SLAVE_BUSWIDTH_4_BYTES;
	(*dp)->dma_type = JZDMA_REQ_I2S1;
	(*dp)->fragsize = FRAGSIZE_M;
	(*dp)->fragcnt = FRAGCNT_L;
	/*(*dp)->fragcnt = FRAGCNT_B;*/

	if (direction == DMA_FROM_DEVICE) {
		(*dp)->dma_config.src_maxburst = 32;
		(*dp)->dma_config.dst_maxburst = 32;
		(*dp)->dma_config.src_addr = iobase + DMICDR;
		(*dp)->dma_config.dst_addr = 0;
	} else	if (direction == DMA_TO_DEVICE) {
		(*dp)->dma_config.src_maxburst = 32;
		(*dp)->dma_config.dst_maxburst = 32;
		(*dp)->dma_config.dst_addr = iobase + AICDR;
		(*dp)->dma_config.src_addr = 0;
	} else
		return -1;

	return 0;
}

static int dmic_global_init(struct platform_device *pdev)
{
	int ret = 0;
	struct dsp_pipe *dmic_pipe_out = NULL;
	struct dsp_pipe *dmic_pipe_in = NULL;

	struct dmic_device * dmic_dev;

	printk("----> start %s\n", __func__);
	dmic_dev = (struct dmic_device *)kzalloc(sizeof(struct dmic_device), GFP_KERNEL);

	if(!dmic_dev) {
		dev_err(&pdev->dev, "failed to alloc dmic dev!\n");
		return -ENOMEM;
	}

	dmic_dev->cur_dmic = kmalloc(sizeof(struct jz_dmic),GFP_KERNEL);
	sprintf(dmic_dev->name, "dmic");

	dmic_dev->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (dmic_dev->res == NULL) {
		printk("%s dmic_resource get failed!\n", __func__);
		return -1;
	}

	/* map io address */
	if (!request_mem_region(dmic_dev->res->start, resource_size(dmic_dev->res), pdev->name)) {
		printk("%s request mem region failed!\n", __func__);
		return -EBUSY;
	}
	dmic_dev->dmic_iomem = ioremap(dmic_dev->res->start, resource_size(dmic_dev->res));
	if (!dmic_dev->dmic_iomem) {
		printk("%s ioremap failed!\n", __func__);
		ret =  -ENOMEM;
		goto __err_ioremap;
	}

/****************************!!!!!!!!*************************/
	dmic_iomem = dmic_dev->dmic_iomem;
	ret = dmic_init_pipe(&dmic_pipe_out,DMA_TO_DEVICE,dmic_dev->res->start);
	if (ret < 0) {
		printk("%s init write pipe failed!\n", __func__);
		goto __err_init_pipeout;
	}
	ret = dmic_init_pipe(&dmic_pipe_in,DMA_FROM_DEVICE,dmic_dev->res->start);
	if (ret < 0) {
		printk("%s init read pipe failed!\n", __func__);
		goto __err_init_pipein;
	}

	dmic_endpoints.out_endpoint = dmic_pipe_out;
	dmic_endpoints.in_endpoint = dmic_pipe_in;
/*****************************!!!!!!!! next ***********************/

	/*request dmic clk */
	dmic_dev->i2s_clk = clk_get(&pdev->dev, "cgu_i2s");
	if (IS_ERR(dmic_dev->i2s_clk)) {
		dev_err(&pdev->dev, "----> dmic cgu_i2s clk get failed\n");
		goto __err_dmic_clk;
	}
	clk_enable(dmic_dev->i2s_clk);

	dmic_dev->clk = clk_get(&pdev->dev, "dmic");
	if (IS_ERR(dmic_dev->clk)) {
		dev_err(&pdev->dev, "----> dmic clk_get failed\n");
		goto __err_dmic_clk;
	}
	clk_enable(dmic_dev->clk);

	/*request dmic pwc */
	dmic_dev->pwc_clk = clk_get(&pdev->dev, "pwc_dmic");
	if (IS_ERR(dmic_dev->pwc_clk)) {
		dev_err(&pdev->dev, "----> dmic pwc_get failed\n");
		goto __err_dmic_clk;
	}
	clk_enable(dmic_dev->pwc_clk);
	spin_lock_init(&dmic_dev->dmic_irq_lock);

#ifdef DMIC_IRQ
	/* request irq */
	dmic_dev->dmic_irq = platform_get_irq(pdev, 0);
	ret = request_irq(dmic_dev->dmic_irq, dmic_irq_handler,
					  IRQF_DISABLED, "dmic_irq", dmic_dev);
	if (ret < 0) {
		printk("----> request irq error\n");
		goto __err_irq;
	}
#endif


	dmic_set_private_data(&dmic_data, dmic_dev); /*dmic_data is global*/
	dev_set_drvdata(&pdev->dev, dmic_dev);

	printk("dmic init success.\n");
	clk_disable(dmic_dev->clk);

	return  0;
__err_dmic_clk:
__err_irq:
	clk_disable(dmic_dev->clk);
	clk_put(dmic_dev->clk);
__err_init_pipein:
	vfree(dmic_pipe_out);
__err_init_pipeout:
	iounmap(dmic_dev->dmic_iomem);
__err_ioremap:
	release_mem_region(dmic_dev->res->start,resource_size(dmic_dev->res));
	return ret;
}

static int dmic_init(struct platform_device *pdev)
{
	int ret = 0;

	ret = dmic_global_init(pdev);
	if (ret)
		printk("dmic init error!\n");

	return ret;
}

static void dmic_shutdown(struct platform_device *pdev)
{
	/* close dmic and current codec */
	struct snd_dev_data *tmp;
	struct dmic_device * dmic_dev;
	tmp = dmic_get_ddata(pdev);
	dmic_dev = dmic_get_private_data(tmp);

	__dmic_disable();

	clk_disable(dmic_dev->clk);
	clk_disable(dmic_dev->pwc_clk);
	clk_disable(dmic_dev->i2s_clk);
	return;
}

static int dmic_suspend(struct platform_device *pdev, pm_message_t state)
{
	unsigned long thr = 0;
	struct snd_dev_data *tmp;
	struct dmic_device * dmic_dev;
	tmp = dmic_get_ddata(pdev);
	dmic_dev = dmic_get_private_data(tmp);

	dmic_set_voice_trigger(dmic_dev, &thr,CODEC_RMODE);
	return 0;
}

static int dmic_resume(struct platform_device *pdev)
{
	struct snd_dev_data *tmp;
	struct dmic_device * dmic_dev;
	tmp = dmic_get_ddata(pdev);
	dmic_dev = dmic_get_private_data(tmp);

	dmic_enable(dmic_dev, CODEC_RMODE);
	return 0;
}

struct snd_dev_data dmic_data = {
	//.dev_ioctl	   	= dmic_ioctl,
	.dev_ioctl_2	= dmic_ioctl,
	.ext_data		= &dmic_endpoints,
	.minor			= SND_DEV_DSP3,
	.init			= dmic_init,
	.shutdown		= dmic_shutdown,
	.suspend		= dmic_suspend,
	.resume			= dmic_resume,
};

struct snd_dev_data snd_mixer3_data = {
	//.dev_ioctl	   	= dmic_ioctl,
	.dev_ioctl_2	= dmic_ioctl,
	.minor			= SND_DEV_MIXER3,
};

static int __init init_dmic(void)
{
	printk("-----> come to this %s\n", __func__);

	return 0;
}
module_init(init_dmic);
