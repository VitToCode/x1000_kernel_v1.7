
#include <mach/jzsnd.h>
#include "grus.h"

struct snd_codec_data codec_data = {
	.codec_sys_clk = 0 ,
	.codec_dmic_clk = 0,
	/* volume */
	.replay_volume_base = 0,
	.record_volume_base = 0,
	.record_digital_volume_base = 23,
	.replay_digital_volume_base = 0,
	.replay_hp_output_gain_base = 0,

	/* default route */
	.replay_def_route = {
#ifdef CONFIG_BOARD_GRUS
					.route = SND_ROUTE_REPLAY_DACRL_TO_HPRL,
#else
					.route = SND_ROUTE_REPLAY_DACRL_TO_LO,
#endif
					.gpio_hp_mute_stat = 0,
					.gpio_spk_en_stat = 1,
					.gpio_handset_en_stat = -1,
					.gpio_buildin_mic_en_stat = -1},
	.record_def_route = {.route = SND_ROUTE_RECORD_MIC1_AN1,
					.gpio_hp_mute_stat = -1,
					.gpio_spk_en_stat = -1,
					.gpio_handset_en_stat = -1,
					.gpio_buildin_mic_en_stat = 0},
	/* device <-> route map */
	.record_headset_mic_route = {
#ifdef CONFIG_BOARD_GRUS
					.route = SND_ROUTE_RECORD_MIC2_SIN_AN3,
#else
					.route = SND_ROUTE_RECORD_MIC1_SIN_AN2,
#endif
					.gpio_hp_mute_stat = -1,
					.gpio_spk_en_stat = -1,
					.gpio_handset_en_stat = -1,
					.gpio_buildin_mic_en_stat = 0},

	.record_buildin_mic_route = {.route = SND_ROUTE_RECORD_MIC1_AN1,
					.gpio_hp_mute_stat = 0,
					.gpio_spk_en_stat = 1,
					.gpio_handset_en_stat = 0,
					.gpio_buildin_mic_en_stat = 1},

	.replay_headset_route = {.route = SND_ROUTE_REPLAY_DACRL_TO_HPRL,
					.gpio_hp_mute_stat = 0,
					.gpio_spk_en_stat = 0,
					.gpio_handset_en_stat = 0,
					.gpio_buildin_mic_en_stat = -1},

	.replay_speaker_route = {
#ifdef CONFIG_BOARD_GRUS
					.route = SND_ROUTE_REPLAY_DACRL_TO_HPRL,
#else
					.route = SND_ROUTE_REPLAY_DACRL_TO_LO,
#endif
					.gpio_hp_mute_stat = 0,
					.gpio_spk_en_stat = 1,
					.gpio_handset_en_stat = 0,
					.gpio_buildin_mic_en_stat = -1},

	.replay_headset_and_speaker_route = {.route = SND_ROUTE_REPLAY_DACRL_TO_ALL,
						.gpio_hp_mute_stat = 0,
						.gpio_spk_en_stat = 1,
						.gpio_handset_en_stat = 0,
						.gpio_buildin_mic_en_stat = -1},

	.fm_speaker_route = {.route = SND_ROUTE_NONE,
						 .gpio_hp_mute_stat = 0,
						 .gpio_spk_en_stat = 1,
						 .gpio_handset_en_stat = 0,
						 .gpio_buildin_mic_en_stat = -1},

	.fm_headset_route = {.route = SND_ROUTE_NONE,
						 .gpio_hp_mute_stat = 0,
						 .gpio_spk_en_stat = 0,
						 .gpio_handset_en_stat = 0,
						 .gpio_buildin_mic_en_stat = -1},

	.downlink_handset_route = {.route = SND_ROUTE_REPLAY_LINEIN2_BYPASS_TO_LINEOUT,
					.gpio_hp_mute_stat = 0,
					.gpio_spk_en_stat = 0,
					.gpio_handset_en_stat = 1,
					.gpio_buildin_mic_en_stat = -1},

	.downlink_headset_route = {.route = SND_ROUTE_NONE,
						 .gpio_hp_mute_stat = 0,
						 .gpio_spk_en_stat = 0,
						 .gpio_handset_en_stat = 0,
						 .gpio_buildin_mic_en_stat = -1},

	.downlink_speaker_route = {.route = SND_ROUTE_NONE,
						 .gpio_hp_mute_stat = 0,
						 .gpio_spk_en_stat = 1,
						 .gpio_handset_en_stat = 0,
						 .gpio_buildin_mic_en_stat = -1},

	.uplink_buildin_mic_route = {.route = SND_ROUTE_NONE,
						 .gpio_hp_mute_stat = -1,
						 .gpio_spk_en_stat = -1,
						 .gpio_handset_en_stat = -1,
						 .gpio_buildin_mic_en_stat = 1},

	.uplink_headset_mic_route = {.route = SND_ROUTE_NONE,
						 .gpio_hp_mute_stat = -1,
						 .gpio_spk_en_stat = -1,
						 .gpio_handset_en_stat = -1,
						 .gpio_buildin_mic_en_stat = 0},

	.record_incall_route = {.route = SND_ROUTE_NONE,
						 .gpio_hp_mute_stat = -1,
						 .gpio_spk_en_stat = -1,
						 .gpio_handset_en_stat = -1,
						 .gpio_buildin_mic_en_stat = -1},
	/* gpio */
	.gpio_hp_mute = {.gpio = GPIO_HP_MUTE, .active_level = GPIO_HP_MUTE_LEVEL},
	.gpio_spk_en = {.gpio = GPIO_SPEAKER_EN, .active_level = GPIO_SPEAKER_EN_LEVEL},
	.gpio_handset_en = {.gpio = GPIO_HANDSET_EN, .active_level = GPIO_HANDSET_EN_LEVEL},
	.gpio_hp_detect = {.gpio = GPIO_HP_DETECT, .active_level = GPIO_HP_INSERT_LEVEL},
	.gpio_mic_detect = {.gpio = GPIO_MIC_DETECT,.active_level = GPIO_MIC_INSERT_LEVEL},
	.gpio_buildin_mic_select = {.gpio = GPIO_MIC_SELECT,.active_level = GPIO_BUILDIN_MIC_LEVEL},
	.gpio_mic_detect_en = {.gpio = GPIO_MIC_DETECT_EN,.active_level = GPIO_MIC_DETECT_EN_LEVEL},

	.hpsense_active_level = 1,
};
