#ifndef __VOICE_WAKEUP_MODULE_H__
#define __VOICE_WAKEUP_MODULE_H__


enum open_mode {
	EARLY_SLEEP = 1,
	DEEP_SLEEP,
	NORMAL_RECORD,
	NORMAL_WAKEUP
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

unsigned char wakeup_module_get_resource_addr(void);

int wakeup_module_ioctl(int cmd, unsigned long args);


int wakeup_module_process_data(void);

int wakeup_module_is_cpu_wakeup_by_dmic(void);


int wakeup_module_set_sleep_buffer(unsigned char *, unsigned long);

int wakeup_module_get_sleep_process(void);

#endif
