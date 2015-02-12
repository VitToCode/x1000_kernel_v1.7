 /*
 * Copyright (C) 2014 Ingenic Semiconductor Co., Ltd.
 *	http://www.ingenic.com
 * Author: cli <chen.li@ingenic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/gpio.h>
#include "icodec/dlv4780.h"

static struct snd_soc_ops dorado_i2s_ops = {

};

#if defined(CONFIG_BOARD_DORADO_V21) && defined(CONFIG_GPIO_PCA953X)
#define DORADO_SPK_GPIO   (177 + 5)
#define DORADO_SPK_EN	1
#define DORADO_HAVE_SPK_EN
#elif defined(CONFIG_BOARD_DORADO_V20)
#include <asm/arch/gpio.h>
#define DORADO_SPK_GPIO  GPIO_PA(12)
#define DORADO_SPK_EN	1
#define DORADO_HAVE_SPK_EN
#elif defined(CONFIG_BOARD_DORADO_V30)
#include <asm/arch/gpio.h>
#define DORADO_SPK_GPIO	 GPIO_PE(23)
#define DORADO_SPK_EN	1
#define DORADO_HAVE_SPK_EN
#endif

static int dorado_spk_power(struct snd_soc_dapm_widget *w,
				struct snd_kcontrol *kcontrol, int event)
{
#ifdef DORADO_HAVE_SPK_EN
	if (SND_SOC_DAPM_EVENT_ON(event)) {
		gpio_direction_output(DORADO_SPK_GPIO, DORADO_SPK_EN);
		printk("gpio speaker enable %d\n", gpio_get_value(DORADO_SPK_GPIO));
	} else {
		gpio_direction_output(DORADO_SPK_GPIO, !DORADO_SPK_EN);
		printk("gpio speaker disable %d\n", gpio_get_value(DORADO_SPK_GPIO));
	}
#endif
	return 0;
}

static const struct snd_soc_dapm_widget dorado_dapm_widgets[] = {
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_SPK("Speaker", dorado_spk_power),
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_MIC("Mic Buildin", NULL),
};

static struct snd_soc_jack dorado_dlv4780_hp_jack;
static struct snd_soc_jack_pin dorado_dlv4780_hp_jack_pins[] = {
	{
		.pin = "Headphone Jack",
		.mask = SND_JACK_HEADPHONE,
	},
	{
		.pin = "Mic Jack",
		.mask = SND_JACK_MICROPHONE,
	},
};

/* dorado machine audio_map */
static const struct snd_soc_dapm_route audio_map[] = {
	/* headphone connected to AOHPL/R */
	{"Headphone Jack", NULL, "AOHPL"},
	{"Headphone Jack", NULL, "AOHPR"},

	/* ext speaker connected to AOLOP/N  */
	{"Speaker", NULL, "AOLOP"},
	{"Speaker", NULL, "AOLON"},

	/* mic is connected to AIP/N1 */
	{"AIP1", NULL, "Mic Buildin"},
	{"AIN1", NULL, "Mic Buildin"},
	{"AIP2", NULL, "Mic Jack"},

	{"Mic Buildin", NULL, "MICBIAS1"},
	{"Mic Jack", NULL, "MICBIAS1"},
};

static int dorado_dlv_dai_link_init(struct snd_soc_pcm_runtime *rtd)
{
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	int err;

	err = gpio_request(DORADO_SPK_GPIO, "Speaker_en");
	if (err)
		return err;

	err = snd_soc_dapm_new_controls(dapm, dorado_dapm_widgets,
			ARRAY_SIZE(dorado_dapm_widgets));
	if (err)
		return err;

	/* Set up rx1950 specific audio path audio_mapnects */
	err = snd_soc_dapm_add_routes(dapm, audio_map,
			ARRAY_SIZE(audio_map));
	if (err)
		return err;

	snd_soc_jack_new(codec, "Headset Jack", SND_JACK_HEADSET, &dorado_dlv4780_hp_jack);
	snd_soc_jack_add_pins(&dorado_dlv4780_hp_jack,
			ARRAY_SIZE(dorado_dlv4780_hp_jack_pins),
			dorado_dlv4780_hp_jack_pins);

	dlv4780_hp_detect(codec, &dorado_dlv4780_hp_jack, SND_JACK_HEADSET);

	snd_soc_dapm_force_enable_pin(dapm, "Speaker");
	snd_soc_dapm_force_enable_pin(dapm, "Mic Buildin");

	snd_soc_dapm_sync(dapm);
	return 0;
}

static struct snd_soc_dai_link dorado_dais[] = {
	[0] = {
		.name = "DORADO-DLV-4780",
		.stream_name = "DORADO ICDC",
		.platform_name = "jz-asoc-aic-dma",
		.cpu_dai_name = "jz-asoc-aic-i2s",
		.init = dorado_dlv_dai_link_init,
		.codec_dai_name = "dlv4780-hifi",
		.codec_name = "dlv4780",
		.ops = &dorado_i2s_ops,
	},
};

static struct snd_soc_card dorado = {
	.name = "dorado",
	.dai_link = dorado_dais,
	.num_links = ARRAY_SIZE(dorado_dais),
};

static struct platform_device *dorado_snd_device;

static int dorado_init(void)
{
	/*struct jz_aic_gpio_func *gpio_info;*/
	int ret;

	dorado_snd_device = platform_device_alloc("soc-audio", -1);
	if (!dorado_snd_device)
		return -ENOMEM;

	platform_set_drvdata(dorado_snd_device, &dorado);
	ret = platform_device_add(dorado_snd_device);
	if (ret)
		platform_device_put(dorado_snd_device);

	return ret;
}

static void dorado_exit(void)
{
	platform_device_unregister(dorado_snd_device);
}

module_init(dorado_init);
module_exit(dorado_exit);

MODULE_AUTHOR("cli<chen.li@ingenic.com>");
MODULE_DESCRIPTION("ALSA SoC Dorado Snd Card");
MODULE_LICENSE("GPL");
