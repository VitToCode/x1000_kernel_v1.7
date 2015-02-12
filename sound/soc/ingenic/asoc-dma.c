/*
 *  sound/soc/ingenic/asoc-dma.c
 *  ALSA Soc Audio Layer -- ingenic audio dma platform driver
 *
 *  Copyright 2014 Ingenic Semiconductor Co.,Ltd
 *	cli <chen.li@ingenic.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/mm.h>
#include <linux/dmaengine.h>
#include <linux/dma-mapping.h>
#include <linux/hrtimer.h>
#include <sound/soc.h>
#include <sound/pcm.h>
#include <sound/soc-dai.h>
#include <sound/pcm_params.h>
#include <mach/jzdma.h>
#include "asoc-aic.h"

static int asoc_dma_debug = 0;
module_param(asoc_dma_debug, int, 0644);
#define DMA_DEBUG_MSG(msg...)			\
	do {					\
		if (asoc_dma_debug)		\
			printk(KERN_DEBUG"ADMA: " msg);	\
	} while(0)

struct jz_pcm_runtime_data {
	struct snd_pcm_substream *substream;
	struct dma_chan *dma_chan;
	dma_cookie_t cookie;
	unsigned int pos;
#ifndef CONFIG_JZ_ASOC_STANDARD_CYCLIC_DMA
	struct hrtimer hr_timer;
	ktime_t expires;
	atomic_t stopped;
#endif
};
/*
 * fake using a continuous buffer
 */
static inline void *
snd_pcm_get_ptr(struct snd_pcm_substream *substream, unsigned int ofs)
{
	return substream->runtime->dma_area + ofs;
}

static size_t
snd_pcm_get_pos_algin_period(struct snd_pcm_substream *substream, dma_addr_t addr)
{
	return (addr - substream->runtime->dma_addr -
			(addr - substream->runtime->dma_addr)%
			snd_pcm_lib_period_bytes(substream));
}

static size_t
snd_pcm_get_pos(struct snd_pcm_substream *substream, dma_addr_t addr)
{
	return (addr - substream->runtime->dma_addr);
}


static int jz_pcm_hw_params(struct snd_pcm_substream *substream,
		struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct jz_pcm_runtime_data *prtd = substream->runtime->private_data;
	struct jz_pcm_dma_params *dma_params =
		snd_soc_dai_get_dma_data(rtd->cpu_dai, substream);
	struct dma_slave_config slave_config;
	int ret;

	DMA_DEBUG_MSG("%s enter\n", __func__);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		slave_config.direction = DMA_TO_DEVICE;
		slave_config.dst_addr = dma_params->dma_addr;
	} else {
		slave_config.direction = DMA_FROM_DEVICE;
		slave_config.src_addr = dma_params->dma_addr;
	}
	slave_config.dst_addr_width = dma_params->buswidth;
	slave_config.dst_maxburst = dma_params->max_burst;
	slave_config.src_addr_width = dma_params->buswidth;	/*jz dmaengine soft buge*/
	slave_config.src_maxburst = dma_params->max_burst;
	ret = dmaengine_slave_config(prtd->dma_chan, &slave_config);
	if (ret)
		return ret;
#ifndef CONFIG_JZ_ASOC_STANDARD_CYCLIC_DMA
	{
		unsigned long long time_ns;
		time_ns = 1000 * 1000 * 1000 * params_period_size(params)/params_rate(params);
		prtd->expires = ns_to_ktime(time_ns);
	}
#endif
	return snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(params));
}

#if defined(CONFIG_JZ_ASOC_STANDARD_CYCLIC_DMA)
static void jz_asoc_dma_callback(void *data)
{
	struct snd_pcm_substream *substream = data;
	struct jz_pcm_runtime_data *prtd = substream->runtime->private_data;
	DMA_DEBUG_MSG("%s enter\n", __func__);
#if defined(CONFIG_JZ_ASOC_DMA_AUTO_CLR_DRT_MEM)
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		DMA_DEBUG_MSG("dma start %x pos %x size %d\n",
				substream->runtime->dma_addr,
				snd_pcm_get_ptr(substream, prtd->pos),
				snd_pcm_lib_period_bytes(substream));
		memset(snd_pcm_get_ptr(substream, prtd->pos),
				0, snd_pcm_lib_period_bytes(substream));
	}
#endif
	prtd->pos += snd_pcm_lib_period_bytes(substream);
	if (prtd->pos >= snd_pcm_lib_buffer_bytes(substream))
		prtd->pos = 0;
	snd_pcm_period_elapsed(substream);
	return;
}
#else	/*!CONFIG_JZ_ASOC_STANDARD_CYCLIC_DMA*/
static enum hrtimer_restart jz_asoc_hrtimer_callback(struct hrtimer *hr_timer) {
	struct jz_pcm_runtime_data *prtd = container_of(hr_timer,
			struct jz_pcm_runtime_data, hr_timer);
	struct snd_pcm_substream *substream = prtd->substream;
	struct dma_chan *dma_chan = prtd->dma_chan;
	dma_addr_t pdma_addr = 0;
	size_t buffer_bytes = snd_pcm_lib_buffer_bytes(substream);
	size_t curr_pos = 0;
	enum dma_data_direction direction = substream->stream == SNDRV_PCM_STREAM_PLAYBACK ?
		DMA_TO_DEVICE : DMA_FROM_DEVICE;

	if (atomic_read(&prtd->stopped))
		goto out;
	hrtimer_start(&prtd->hr_timer, prtd->expires , HRTIMER_MODE_REL);
	pdma_addr = dma_chan->device->get_current_trans_addr(dma_chan,
			NULL,
			NULL,
			direction);
	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		curr_pos = snd_pcm_get_pos_algin_period(substream, pdma_addr);
		if (curr_pos == prtd->pos)
			goto out;
#ifdef CONFIG_JZ_ASOC_DMA_AUTO_CLR_DRT_MEM
		if (prtd->pos < curr_pos) {
			memset(snd_pcm_get_ptr(substream, prtd->pos), 0 , (curr_pos - prtd->pos));
		}
		if (prtd->pos > curr_pos) {
			memset(snd_pcm_get_ptr(substream, prtd->pos), 0, (buffer_bytes - prtd->pos));
			memset(snd_pcm_get_ptr(substream, 0), 0, curr_pos);
		}
#endif
		prtd->pos = curr_pos;
	} else {
		curr_pos = snd_pcm_get_pos(substream, pdma_addr);
		if (curr_pos == prtd->pos)
			goto out;
		prtd->pos = curr_pos;
	}
	//printk(KERN_DEBUG"curr_pos = %d buffer_bytes = %d\n", curr_pos, buffer_bytes);
	snd_pcm_period_elapsed(substream);
out:
	return HRTIMER_NORESTART;
}
#endif

static int jz_asoc_dma_prepare_and_submit(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct jz_pcm_runtime_data *prtd = substream->runtime->private_data;
	struct dma_async_tx_descriptor *desc;
	enum dma_data_direction direction =
		(substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
		DMA_TO_DEVICE :
		DMA_FROM_DEVICE;

	prtd->pos = 0;
	desc = prtd->dma_chan->device->device_prep_dma_cyclic(prtd->dma_chan,
			substream->runtime->dma_addr,
			snd_pcm_lib_buffer_bytes(substream),
			snd_pcm_lib_period_bytes(substream),
			direction);
	if (!desc) {
		dev_err(&rtd->dev, "cannot prepare slave dma\n");
		return -EINVAL;
	}
#ifdef CONFIG_JZ_ASOC_STANDARD_CYCLIC_DMA
	desc->callback = jz_asoc_dma_callback;
#endif
	desc->callback_param = substream;
	prtd->cookie = dmaengine_submit(desc);
	return 0;
}

static int jz_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct jz_pcm_runtime_data *prtd = substream->runtime->private_data;
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	size_t buffer_bytes = snd_pcm_lib_buffer_bytes(substream);
	int ret;

	DMA_DEBUG_MSG("%s enter cmd %d\n", __func__, cmd);
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		ret = jz_asoc_dma_prepare_and_submit(substream);
		if (ret)
			return ret;
		dma_async_issue_pending(prtd->dma_chan);
#ifndef CONFIG_JZ_ASOC_STANDARD_CYCLIC_DMA
		atomic_set(&prtd->stopped, 0);
		hrtimer_start(&prtd->hr_timer, prtd->expires , HRTIMER_MODE_REL);
#endif
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
#ifndef CONFIG_JZ_ASOC_STANDARD_CYCLIC_DMA
		atomic_set(&prtd->stopped, 1);
#endif
		/* For JZ DMA we stop i2s dma request
		 * and wait tur or ror happen to
		 * make sure dma is not transfer data on AHB bus,
		 * then we can stop the dma.
		 */
		if (cpu_dai->driver->ops->trigger) {
			ret = cpu_dai->driver->ops->trigger(substream, cmd, cpu_dai);
			if (ret < 0)
				return ret;
		}
		dmaengine_terminate_all(prtd->dma_chan);
#ifdef CONFIG_JZ_ASOC_DMA_AUTO_CLR_DRT_MEM
		printk(KERN_DEBUG"show the time memset1 %d\n", buffer_bytes);
		memset(snd_pcm_get_ptr(substream, 0), 0, buffer_bytes);
		printk(KERN_DEBUG"show the time memset2 %d\n", buffer_bytes);
#endif
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static snd_pcm_uframes_t jz_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct jz_pcm_runtime_data *prtd = substream->runtime->private_data;

	DMA_DEBUG_MSG("%s: %d %ld\n", __func__, prtd->pos,
			bytes_to_frames(substream->runtime, prtd->pos));
	return bytes_to_frames(substream->runtime, prtd->pos);
}

struct jz_dma_pcm {
	struct dma_chan *chan[2];
	enum jzdma_type dma_type;
};

#define JZ_DMA_BUFFERSIZE (256 * PAGE_SIZE)
static const struct snd_pcm_hardware jz_pcm_hardware = {
	.info = SNDRV_PCM_INFO_MMAP |
		SNDRV_PCM_INFO_PAUSE |
		SNDRV_PCM_INFO_RESUME |
		SNDRV_PCM_INFO_MMAP_VALID |
		SNDRV_PCM_INFO_INTERLEAVED |
		SNDRV_PCM_INFO_BLOCK_TRANSFER,
	.formats = SNDRV_PCM_FMTBIT_S24_LE |
		SNDRV_PCM_FMTBIT_S20_3LE |
		SNDRV_PCM_FMTBIT_S18_3LE |
		SNDRV_PCM_FMTBIT_S16_LE |
		SNDRV_PCM_FMTBIT_S8,
	.rates                  = SNDRV_PCM_RATE_8000_192000,
	.rate_min               = 8000,
	.rate_max               = 192000,
	.channels_min           = 1,
	.channels_max           = 2,
	.buffer_bytes_max       = JZ_DMA_BUFFERSIZE,
	.period_bytes_min       = PAGE_SIZE,      /* 4K */
	.period_bytes_max       = PAGE_SIZE * 16, /* 64K */
	.periods_min            = 2,
	.periods_max            = 256,
	.fifo_size              = 0,
};

static int jz_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_substream_chip(substream);
	struct jz_dma_pcm *jz_pcm = snd_soc_platform_get_drvdata(rtd->platform);
	struct dma_chan *chan = jz_pcm->chan[substream->stream];
	struct jz_pcm_runtime_data *prtd = NULL;
	int ret;

	DMA_DEBUG_MSG("%s enter\n", __func__);
	ret = snd_soc_set_runtime_hwparams(substream, &jz_pcm_hardware);
	if (ret)
		return ret;

	ret = snd_pcm_hw_constraint_integer(substream->runtime,
			SNDRV_PCM_HW_PARAM_PERIODS);
	if (ret < 0)
		return ret;

	prtd = kzalloc(sizeof(*prtd), GFP_KERNEL);
	if (!prtd)
		return -ENOMEM;

#ifndef CONFIG_JZ_ASOC_STANDARD_CYCLIC_DMA
	atomic_set(&prtd->stopped, 0);
	hrtimer_init(&prtd->hr_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	prtd->hr_timer.function = jz_asoc_hrtimer_callback;
#endif
	prtd->dma_chan = chan;
	prtd->substream = substream;
	substream->runtime->private_data = prtd;
	return 0;
}

static int jz_pcm_close(struct snd_pcm_substream *substream)
{
	struct jz_pcm_runtime_data *prtd = substream->runtime->private_data;

	DMA_DEBUG_MSG("%s enter\n", __func__);
	kfree(prtd);
	substream->runtime->private_data = NULL;
	return 0;
}

struct snd_pcm_ops jz_pcm_ops = {
	.open		= jz_pcm_open,
	.close		= jz_pcm_close,
	.ioctl		= snd_pcm_lib_ioctl,
	.hw_params	= jz_pcm_hw_params,
	.hw_free	= snd_pcm_lib_free_pages,
	.trigger	= jz_pcm_trigger,
	.pointer	= jz_pcm_pointer,
};

static bool filter(struct dma_chan *chan, void *filter_param)
{
	struct jz_dma_pcm *jz_pcm = (struct jz_dma_pcm*)filter_param;
	return jz_pcm->dma_type == (int)chan->private;
}

static void jz_pcm_free(struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_chip(pcm);
	struct jz_dma_pcm *jz_pcm = snd_soc_platform_get_drvdata(rtd->platform);
	struct snd_pcm_substream *substream;
	int i;

	DMA_DEBUG_MSG("%s enter\n", __func__);

	for (i = SNDRV_PCM_STREAM_PLAYBACK; i <= SNDRV_PCM_STREAM_CAPTURE; i++) {
		substream = pcm->streams[i].substream;
		if (!substream)
			continue;
		if (jz_pcm && jz_pcm->chan[i])
			dma_release_channel(jz_pcm->chan[i]);
		snd_pcm_lib_preallocate_free(substream);
	}
	return;
}

static int jz_pcm_new(struct snd_card *card, struct snd_soc_dai *dai,
		struct snd_pcm *pcm)
{
	struct snd_soc_pcm_runtime *rtd = snd_pcm_chip(pcm);
	struct jz_dma_pcm *jz_pcm = snd_soc_platform_get_drvdata(rtd->platform);
	struct snd_pcm_substream *substream;
	dma_cap_mask_t mask;
	size_t buffer_size = JZ_DMA_BUFFERSIZE;
	size_t buffer_bytes_max = JZ_DMA_BUFFERSIZE;
	int ret = -EINVAL;
	int i;

	DMA_DEBUG_MSG("%s enter\n", __func__);

	for (i = 0; i < 2; i++) {
		substream = pcm->streams[i].substream;
		if (!substream)
			continue;
		if (!jz_pcm->chan[i]) {
			dma_cap_zero(mask);
			dma_cap_set(DMA_SLAVE, mask);
			dma_cap_set(DMA_CYCLIC, mask);
			jz_pcm->chan[i] = dma_request_channel(mask, filter, jz_pcm);

			if (!jz_pcm->chan[i])
				goto out;

			ret = snd_pcm_lib_preallocate_pages(substream,
					SNDRV_DMA_TYPE_DEV,
					jz_pcm->chan[i]->device->dev,
					buffer_size,
					buffer_bytes_max);
			if (ret)
				goto out;
		}
	}
	return 0;
out:
	dev_err(dai->dev, "Failed to alloc dma buffer %d\n", ret);
	jz_pcm_free(pcm);
	return ret;
}

static struct snd_soc_platform_driver jz_pcm_platform = {
	.ops            = &jz_pcm_ops,
	.pcm_new        = jz_pcm_new,
	.pcm_free       = jz_pcm_free,
};

static int jz_pcm_platform_probe(struct platform_device *pdev)
{
	struct jz_dma_pcm *jz_pcm;
	struct resource *res;
	int ret;

	jz_pcm  = kzalloc(sizeof(*jz_pcm), GFP_KERNEL);
	if (!jz_pcm)
		return -ENOMEM;

	res = platform_get_resource(pdev, IORESOURCE_DMA, 0);
	if (!res) {
		ret = -ENOENT;
		dev_err(&pdev->dev, "Failed to get platfrom dma resource\n");
		goto err_get_dma_res;
	}
	jz_pcm->dma_type = GET_MAP_TYPE(res->start);

	platform_set_drvdata(pdev, jz_pcm);

	ret = snd_soc_register_platform(&pdev->dev, &jz_pcm_platform);
	if (ret) {
		dev_err(&pdev->dev, "Failed to register platfrom\n");
		goto err_register_platform;
	}

	dev_info(&pdev->dev, "Audio dma platfrom probe success\n");

	return 0;

err_register_platform:
	platform_set_drvdata(pdev, NULL);
err_get_dma_res:
	kfree(jz_pcm);
	return ret;
}

static int jz_pcm_platform_remove(struct platform_device *pdev)
{
	struct jz_dma_pcm *jz_pcm = platform_get_drvdata(pdev);

	dev_info(&pdev->dev, "Audio dma platfrom removed\n");
	snd_soc_unregister_platform(&pdev->dev);
	platform_set_drvdata(pdev, NULL);
	kfree(jz_pcm);
	return 0;
}

static const  struct platform_device_id jz_dma_id_table[] = {
	{
		.name = "jz-asoc-aic-dma",	/*aic dma*/
	},
	{
		.name = "jz-asoc-pcmc-dma",	/*pcmc dma*/
	},
	{},
};

struct platform_driver jz_pcm_platfrom_driver = {
	.probe  = jz_pcm_platform_probe,
	.remove = jz_pcm_platform_remove,
	.driver = {
		.name   = "jz-asoc-dma",
		.owner  = THIS_MODULE,
	},
	.id_table = jz_dma_id_table,
};

static int jz_pcm_init(void)
{
	return platform_driver_register(&jz_pcm_platfrom_driver);
}
module_init(jz_pcm_init);

static void jz_pcm_exit(void)
{
	platform_driver_unregister(&jz_pcm_platfrom_driver);
}
module_exit(jz_pcm_exit);

MODULE_DESCRIPTION("JZ ASOC Platform driver");
MODULE_AUTHOR("cli<chen.li@ingenic.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:jz-asoc-dma");
