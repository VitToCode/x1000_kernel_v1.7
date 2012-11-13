#ifndef __XB_SND_DETECT_H__
#define __XB_SND_DETECT_H__

#include <linux/workqueue.h>
#include <linux/switch.h>
#include <linux/wait.h>

/*detect id and name*/
#define DEV_DSP_HP_DET_NAME     "hp_detect"
#define DEV_DSP_DOCK_DET_NAME   "dock_detect"

#define SND_DEV_DETECT0_ID  0
#define SND_DEV_DETECT1_ID  1
#define SND_DEV_DETECT2_ID  2
#define SND_DEV_DETECT3_ID  3

#define SND_SWITCH_TYPE_GPIO    0x1
#define SND_SWITCH_TYPE_CODEC   0x2

enum {
	LOW_VALID =0,
	HIGH_VALID,
	INVALID,
};

enum mic_route{
	CODEC_HEADSET_ROUTE,
	CODEC_HEADPHONE_ROUTE,
};

struct snd_switch_data {
	struct switch_dev sdev;
	wait_queue_head_t wq;
	int type;
	const char *name_headset_on;
	const char *name_headphone_on;
	const char *name_off;
	const char *state_headset_on;
	const char *state_headphone_on;
	const char *state_off;
	int irq;
	struct work_struct work;
	int hp_gpio;
	int hp_valid_level;

	/*mic detect and select*/
	int mic_gpio;
	int	mic_vaild_level;

	int (*codec_get_sate)(void);
};

#endif /*__XB_SND_DETECT_H__*/
