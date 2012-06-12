/**
 * jzsnd.h
 *
 **/
#ifndef __MACH_JZSND_H__
#define __MACH_JZSND_H__

#include <linux/list.h>
#include <linux/sound.h>
#include <linux/switch.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>

#include <mach/jzdma.h>

/*####################################################*\
 * sound pipe and command used for dsp device
\*####################################################*/
/**
 * sound device
 **/
enum snd_device_t {
	SND_DEVICE_DEFAULT = 0,
	SND_DEVICE_CURRENT,
	SND_DEVICE_HANDSET,
	SND_DEVICE_HEADSET,
	SND_DEVICE_SPEAKER,
	SND_DEVICE_BT,
	SND_DEVICE_BT_EC_OFF,
	SND_DEVICE_HEADSET_AND_SPEAKER,
	SND_DEVICE_TTY_FULL,
	SND_DEVICE_CARKIT,
	SND_DEVICE_FM_SPEAKER,
	SND_DEVICE_FM_HEADSET,
	SND_DEVICE_NO_MIC_HEADSET,
	SND_DEVICE_HDMI,
	SND_DEVICE_COUNT
};

/**
 * extern ioctl command for dsp device
 **/
struct direct_info {
	int bytes;
	int offset;
};

enum spipe_mode_t {
	SPIPE_MODE_RECORD,
	SPIPE_MODE_REPLAY,
};

struct spipe_info {
	int spipe_id;
	enum spipe_mode_t spipe_mode;
};

#define SNDCTL_EXT_SET_DEVICE 	   			_SIOR ('P', 99, int)
#define SNDCTL_EXT_SET_STANDBY 				_SIOR ('P', 98, int)
#define SNDCTL_EXT_START_BYPASS_TRANS 		_SIOW ('P', 97, struct spipe_info)
#define SNDCTL_EXT_STOP_BYPASS_TRANS  		_SIOW ('P', 96, struct spipe_info)
#define SNDCTL_EXT_DIRECT_GETINODE			_SIOR ('P', 95, struct direct_info)
#define SNDCTL_EXT_DIRECT_PUTINODE			_SIOW ('P', 94, struct direct_info)
#define SNDCTL_EXT_DIRECT_GETONODE			_SIOR ('P', 93, struct direct_info)
#define SNDCTL_EXT_DIRECT_PUTONODE			_SIOW ('P', 92, struct direct_info)

/**
 * dsp device control command
 **/
enum snd_dsp_command {
	/**
	 * the command flowed is used to enable/disable
	 * replay/record.
	 **/
	SND_DSP_ENABLE_REPLAY,
	SND_DSP_DISABLE_REPLAY,
	SND_DSP_ENABLE_RECORD,
	SND_DSP_DISABLE_RECORD,
	/**
	 * the command flowed is used to enable/disable the
	 * dma transfer on device.
	 **/
	SND_DSP_ENABLE_DMA_RX,
	SND_DSP_DISABLE_DMA_RX,
	SND_DSP_ENABLE_DMA_TX,
	SND_DSP_DISABLE_DMA_TX,
	/**
	 * @SND_DSP_SET_XXXX_RATE is used to set replay/record rate
	 **/
	SND_DSP_SET_REPLAY_RATE,
	SND_DSP_SET_RECORD_RATE,
	/**
	 * @SND_DSP_SET_XXXX_CHANNELS is used to set replay/record
	 * channels, when channels changed, filter maybe also need
	 * changed to a fix value.
	 **/
	SND_DSP_SET_REPLAY_CHANNELS,
	SND_DSP_SET_RECORD_CHANNELS,
	/**
	 * @SND_DSP_GET_XXXX_FMT_CAP is used to get formats that
	 * replay/record supports.
	 * @SND_DSP_GET_XXXX_FMT used to get current replay/record
	 * format.
	 * @SND_DSP_SET_XXXX_FMT is used to set replay/record format,
	 * if the format changed, trigger,dma max_tsz and filter maybe
	 * also need changed to a fix value. and let them effect.
	 **/
	SND_DSP_GET_REPLAY_FMT_CAP,
	SND_DSP_GET_REPLAY_FMT,
	SND_DSP_SET_REPLAY_FMT,
	SND_DSP_GET_RECORD_FMT_CAP,
	SND_DSP_GET_RECORD_FMT,
	SND_DSP_SET_RECORD_FMT,
	/**
	 * @SND_DSP_SET_DEVICE is used to set audio route
	 * @SND_DSP_SET_STANDBY used to set into/release from stanby
	 **/
	SND_DSP_SET_DEVICE,
	SND_DSP_SET_STANDBY,
};

/**
 * fragsize, must be dived by PAGE_SIZE
 **/
#define FRAGSIZE_S 	(PAGE_SIZE >> 1)
#define FRAGSIZE_M 	(PAGE_SIZE)
#define FRAGSIZE_L 	(PAGE_SIZE << 1)

#define FRAGCNT_S	2
#define FRAGCNT_M	4
#define FRAGCNT_L	6

struct dsp_node {
	struct list_head 	list;
	unsigned long 		pBuf;
	unsigned int 		start;
	unsigned int 		end;
	dma_addr_t 			phyaddr;
	size_t 				size;
};

struct dsp_pipe {
	/* dma */
	struct dma_chan		*dma_chan;
	struct jzdma_slave 	dma_slave;			   /* define by device */
	enum dma_data_direction dma_direction;	   /* define by device */
	unsigned int		sg_len;				   /* size of scatter list */
	struct scatterlist	*sg;				   /* I/O scatter list */
	/* buf */
	unsigned long 		*vaddr;
	dma_addr_t 			*paddr;
	size_t				fragsize;              /* define by device */
	size_t				fragcnt;               /* define by device */
	struct list_head 	free_node_list;
	struct list_head 	use_node_list;
	struct dsp_node		*save_node;
	wait_queue_head_t	wq;
	int					avialable_couter;
	/* state */
	volatile bool		is_trans;
	volatile bool 	   	wait_stop_dma;
	volatile bool		need_reconfig_dma;
	volatile bool  		is_used;
	volatile bool  		is_shared;
	volatile bool  		is_mmapd;
	bool   				is_non_block;          /* define by device */
	bool 				can_mmap;              /* define by device */
	/* callback funs */
	void (*handle)(struct dsp_pipe *endpoint); /* define by device */
	int (*filter)(void *buff, int cnt);        /* define by device */
	/* lock */
	spinlock_t			pipe_lock;
};

struct dsp_endpoints {
	struct dsp_pipe *out_endpoint;
	struct dsp_pipe *in_endpoint;
};

/*####################################################*\
 * used for internal codec
\*####################################################*/
enum snd_codec_route_t {
	ROUTE_NONE = 0,
	ROUTE_ALL_CLEAR,
	ROUTE_REPLAY_CLEAR,
	ROUTE_RECORD_CLEAR,
	RECORD_MIC_STEREO_DIFF_WITH_BIAS_TO_ADCLR,								/*stereo mic recoard*/
	RECORD_LINEIN_STEREO_DIFF_TO_ADCLR,										/*stereo linein recoard*/
	REPLAY_HP_STEREO,														/*stereo headphone replay*/
	REPLAY_LINEOUT_LR,														/*stereo lineout replay*/
	RECORD_MIC1_DIFF_WITH_BIAS_TO_ADCLR_BYPASS_LINEIN2_TO_HP_LR,			/*phone call*/
	RECORD_MIC1_AND_LINE2_DIFF_WITH_BIAS_TO_ADCLR_BYPASS_LINEIN2_TO_HP_LR,	/*recoard in call*/
	ROUTE_COUNT
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


/*detect id and name*/
#define DEV_DSP_HP_DET_NAME		"hp_detect"
#define DEV_DSP_DOCK_DET_NAME	"dock_detect"

#define SND_DEV_DETECT0_ID	0
#define SND_DEV_DETECT1_ID	1
#define SND_DEV_DETECT2_ID	2
#define SND_DEV_DETECT3_ID	3

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
	void *ext_data;
	int minor;
	bool is_suspend;
	long (*dev_ioctl) (unsigned int cmd, unsigned long arg);
	int (*init)(struct platform_device *pdev);
	void (*shutdown)(struct platform_device *pdev);
	int (*suspend)(struct platform_device *pdev, pm_message_t state);
	int (*resume)(struct platform_device *pdev);
};

/*####################################################*\
 * sound detect
\*####################################################*/
#define SND_SWITCH_TYPE_GPIO 	0x1
#define SND_SWITCH_TYPE_CODEC 	0x2

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

#endif //__MACH_JZSND_H__
