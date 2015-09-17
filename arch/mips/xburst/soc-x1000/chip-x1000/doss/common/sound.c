
#include <mach/jzsnd.h>
#include "board_base.h"

struct snd_codec_data codec_data = {
	.codec_sys_clk = 0 ,  //0:12M  1:13M
	.codec_dmic_clk = 1,
	/* volume */
	.mic_volume_base = 50,

	.replay_volume_base = 50,
	.replay_mixer_volume_base = 50,
	.replay_aic_mixer_volume_base = 50,

	.record_volume_base = 50,
	.record_aic_mixer_volume_base = 50,
	.record_mixer_volume_base = 50,

	/* default route */
	.replay_def_route = {
		.route = SND_ROUTE_REPLAY_SPK,
		.gpio_spk_en_stat = STATE_ENABLE,
		.replay_volume_base = 50,
	},

	.record_def_route = {
		.route = SND_ROUTE_RECORD_AMIC,
		.gpio_spk_en_stat = STATE_DISABLE,
		.record_volume_base = 50,
	},

	.record_buildin_mic_route = {
		.route = SND_ROUTE_NONE,
		.record_volume_base = 50,
	},

	.record_linein_route = {
		.route = SND_ROUTE_LINEIN_REPLAY_MIXER_LOOPBACK,
		.gpio_spk_en_stat = STATE_ENABLE,
		.record_volume_base = 50,
	},

	.replay_speaker_route = {
		.route = SND_ROUTE_REPLAY_SOUND_MIXER_LOOPBACK,
		.gpio_spk_en_stat = STATE_ENABLE,
		.replay_volume_base = 50,
	},

	.replay_speaker_record_buildin_mic_route = {
		.route = SND_ROUTE_RECORD_AMIC_AND_REPLAY_SPK,
		.gpio_spk_en_stat = STATE_ENABLE,
		.replay_volume_base = 50,
		.record_volume_base = 50,
	},

	.gpio_spk_en = {.gpio = GPIO_SPEAKER_EN, .active_level = GPIO_SPEAKER_EN_LEVEL},
        .gpio_hp_mute = {.gpio = GPIO_HP_MUTE, .active_level = GPIO_HP_MUTE_LEVEL},
        .gpio_hp_detect = {.gpio = GPIO_HP_DETECT, .active_level = GPIO_HP_INSERT_LEVEL},
};
