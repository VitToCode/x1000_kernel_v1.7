#ifndef __VOICE_WAKEUP_MODULE_H__
#define __VOICE_WAKEUP_MODULE_H__


enum open_mode {
	EARLY_SLEEP = 1,
	DEEP_SLEEP,
	NORMAL_RECORD
};



#define DMIC_IOCTL_SET_SAMPLERATE	0x200



#define SYS_WAKEUP_OK		(0x1)
#define SYS_WAKEUP_FAILED	(0x2)
#define SYS_NEED_DATA		(0x3)

int wakeup_module_open(int mode);

int wakeup_module_close(int mode);

void wakeup_module_cache_prefetch(void);

int wakeup_module_handler(int par);

dma_addr_t wakeup_module_get_dma_address(void);

int wakeup_module_ioctl(int cmd, unsigned long args);
#endif
