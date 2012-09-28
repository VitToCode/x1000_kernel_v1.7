/**
 * jzsnd.h
 *
 **/
#ifndef __MACH_JZSND_H__
#define __MACH_JZSND_H__

#include <linux/list.h>
#include <linux/sound.h>
#include <linux/platform_device.h>

/*####################################################*\
 * used for codec
\*####################################################*/
enum snd_codec_route_t {
	SND_ROUTE_NONE = 0,
	SND_ROUTE_ALL_CLEAR,
	SND_ROUTE_REPLAY_CLEAR,
	SND_ROUTE_RECORD_CLEAR,
	/*internal codec: linein2 bypass to lineout */
	SND_ROUTE_REPLAY_INCALL_WITH_HANDSET,
	/*internal codec: linein2 bypass to hprl */
	SND_ROUTE_REPLAY_INCALL_WITH_HEADSET,
	/*internal codec: ..*/
	SND_ROUTE_REPLAY_INCALL_WITH_HEADPHONE,
	/*internal codec: dacrl to hprl*/
	SND_ROUTE_REPLAY_SPEAKER,
	/*internal codec: dacrl to lineout*/
	SND_ROUTE_REPLAY_HEADPHONE,
	/*internal codec: ..*/
	SND_ROUTE_REPLAY_SPEAKER_AND_HEADPHONE,
	/*internal codec: ..*/
	SND_ROUTE_REPLAY_FM_SPEAKER,
	/*internal codec: ..*/
	SND_ROUTE_REPLAY_FM_HEADSET,
	/*internal codec: mic 1 to adcrl*/
	SND_ROUTE_RECORD_MIC,
	/*internal codec: linein 1 to adcrl*/
	SND_ROUTE_RECORD_LINEIN,
	/*internal codec: ..*/
	SND_ROUTE_RECORD_INCALL_WITH_HANDSET,
	/*internal codec: ..*/
	SND_ROUTE_RECORD_INCALL_WITH_HEADSET,
	/*internal codec: ..*/
	SND_ROUTE_RECORD_INCALL_WITH_HEADPHONE,
	SND_ROUTE_COUNT,
};

struct snd_board_route {
	enum snd_codec_route_t route;
	int gpio_hp_mute_stat;	/* -1: not avaiable, 0: disable, 1: enable */
	int gpio_spk_en_stat;	/* -1: not avaiable, 0: disable, 1: enable */
};

struct snd_board_gpio {
	int gpio;
	int active_level;
};

struct snd_codec_data {
	/* clk */
	int codec_sys_clk;
	int codec_dmic_clk;
	/* volume */
	int replay_volume_base;
	int record_volume_base;
	int record_digital_volume_base;
	int replay_digital_volume_base;
	int replay_hp_output_gain_base;
	/* default route */
	struct snd_board_route replay_def_route;
	struct snd_board_route record_def_route;
	/* device <-> route map */
	struct snd_board_route handset_route;
	struct snd_board_route headset_route;
	struct snd_board_route speaker_route;
	struct snd_board_route headset_and_speaker_route;
	struct snd_board_route fm_speaker_route;
	struct snd_board_route fm_headset_route;
	/* gpio */
	struct snd_board_gpio gpio_hp_mute;
	struct snd_board_gpio gpio_spk_en;
	struct snd_board_gpio gpio_head_det;
	/* other */
	int hpsense_active_level;
};


/*####################################################*\
* common, used for sound devices
\*####################################################*/
/**
 * device mame and minor
 **/
#define MAX_SND_MINOR  	128
#define SOUND_STEP		16

#define DEV_DSP_NAME	"dsp"

#define SND_DEV_DSP0  	(SND_DEV_DSP + SOUND_STEP * 0)
#define SND_DEV_DSP1  	(SND_DEV_DSP + SOUND_STEP * 1)
#define SND_DEV_DSP2  	(SND_DEV_DSP + SOUND_STEP * 2)
#define SND_DEV_DSP3  	(SND_DEV_DSP + SOUND_STEP * 3)

#define DEV_MIXER_NAME	"mixer"

#define SND_DEV_MIXER0	(SND_DEV_CTL + SOUND_STEP * 0)
#define SND_DEV_MIXER1 	(SND_DEV_CTL + SOUND_STEP * 1)
#define SND_DEV_MIXER2 	(SND_DEV_CTL + SOUND_STEP * 2)
#define SND_DEV_MIXER3 	(SND_DEV_CTL + SOUND_STEP * 3)

#define minor2index(x)	((x) / SOUND_STEP)

/**
 * struct snd_dev_data: sound device platform_data
 * @list: the list is usd to point to next or pre snd_dev_data.
 * @ext_data: used for dsp device, point to
 * (struct dsp_endpoints *) of dsp device.
 * @minor: the minor of the specific device. value will be
 * SND_DEV_DSP(0...), SND_DEV_MIXER(0...) and so on.
 * @is_suspend: indicate if the device is suspend or not.
 * @dev_ioctl: privide by the specific device, used
 * to control the device by command.
 * @init: specific device init.
 * @shutdown: specific device shutdown.
 * @suspend: specific device suspend.
 * @resume: specific device resume.
 **/
struct snd_dev_data {
	struct list_head list;
	struct device *dev;
	void *ext_data;
	int minor;
	bool is_suspend;
	long (*dev_ioctl) (unsigned int cmd, unsigned long arg);
	int (*init)(struct platform_device *pdev);
	void (*shutdown)(struct platform_device *pdev);
	int (*suspend)(struct platform_device *pdev, pm_message_t state);
	int (*resume)(struct platform_device *pdev);
};

extern struct snd_dev_data i2s0_data;
extern struct snd_dev_data i2s1_data;
extern struct snd_dev_data pcm0_data;
extern struct snd_dev_data pcm1_data;
extern struct snd_dev_data snd_mixer0_data;
//extern struct snd_dev_data snd_mixer1_data;
//extern struct snd_dev_data snd_mixer2_data;
//extern struct snd_dev_data snd_mixer3_data;

#endif //__MACH_JZSND_H__
