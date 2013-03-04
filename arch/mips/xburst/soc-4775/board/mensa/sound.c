
#include <mach/jzsnd.h>
#include "board.h"

struct snd_codec_data codec_data = {
	.codec_sys_clk = 0 ,
	.codec_dmic_clk = 0,
	/* volume */
	.replay_volume_base = 0,
	.record_volume_base = 20,
	.record_digital_volume_base = 15,
	.replay_digital_volume_base = 0,

	/* default route */
	.replay_def_route = {
					.route = REPLAY_HP_STEREO_CAP_LESS,
					.gpio_hp_mute_stat = STATE_DISABLE,
					.gpio_spk_en_stat = STATE_ENABLE,
					.replay_volume_base = 6},
	.record_def_route = {.route = RECORD_MIC1_MONO_DIFF_WITH_BIAS},
	/* device <-> route map */
	.record_buildin_mic_route = {.route = RECORD_MIC1_MONO_DIFF_WITH_BIAS},

	.replay_headset_route = {.route = REPLAY_HP_STEREO_CAP_LESS,
					.gpio_hp_mute_stat = STATE_DISABLE,
					.gpio_spk_en_stat = STATE_DISABLE,
					.replay_volume_base = 0},

	.replay_speaker_route = {
					.route = REPLAY_HP_STEREO_CAP_LESS,
					.gpio_hp_mute_stat = STATE_DISABLE,
					.gpio_spk_en_stat = STATE_ENABLE,
					.replay_volume_base = 6},

	.replay_headset_and_speaker_route = {.route = REPLAY_HP_STEREO_CAP_LESS,
					.gpio_hp_mute_stat = STATE_DISABLE,
					.gpio_spk_en_stat = STATE_ENABLE,
					.replay_volume_base = 6},

	.replay_loop_route = {
					.route = BYPASS_MIC1_DIFF_WITH_BIAS_TO_HP_CAP_LESS,
					.replay_volume_base = 6,
	},
	.gpio_hp_mute = {.gpio = GPIO_HP_MUTE, .active_level = GPIO_HP_MUTE_LEVEL},
	.gpio_spk_en = {.gpio = GPIO_SPEAKER_EN, .active_level = GPIO_SPEAKER_EN_LEVEL},
	.gpio_handset_en = {.gpio = GPIO_HANDSET_EN, .active_level = GPIO_HANDSET_EN_LEVEL},
	.gpio_hp_detect = {.gpio = GPIO_HP_DETECT, .active_level = GPIO_HP_INSERT_LEVEL},
	.gpio_mic_detect = {.gpio = GPIO_MIC_DETECT,.active_level = GPIO_MIC_INSERT_LEVEL},
	.gpio_buildin_mic_select = {.gpio = GPIO_MIC_SELECT,.active_level = GPIO_BUILDIN_MIC_LEVEL},
	.gpio_mic_detect_en = {.gpio = GPIO_MIC_DETECT_EN,.active_level = GPIO_MIC_DETECT_EN_LEVEL},

	/* gpio */
	//hook_active_level = -1,
};
