#ifndef __XB_SND_DETECT_H__
#define __XB_SND_DETECT_H__

#include <linux/workqueue.h>
#include <linux/switch.h>

/*detect id and name*/
#define DEV_DSP_HP_DET_NAME     "hp_detect"
#define DEV_DSP_DOCK_DET_NAME   "dock_detect"

#define SND_DEV_DETECT0_ID  0
#define SND_DEV_DETECT1_ID  1
#define SND_DEV_DETECT2_ID  2
#define SND_DEV_DETECT3_ID  3

#define SND_SWITCH_TYPE_GPIO    0x1
#define SND_SWITCH_TYPE_CODEC   0x2

struct snd_switch_data {
	struct switch_dev sdev;
	int type;
	unsigned gpio;
	const char *name_on;
	const char *name_off;
	const char *state_on;
	const char *state_off;
	int irq;
	struct work_struct work;
	int valid_level;
	int (*codec_get_sate)(void);
};

#endif /*__XB_SND_DETECT_H__*/
