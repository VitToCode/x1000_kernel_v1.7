#ifndef __FP8102_DET
#define __FP8102_DET

struct uevent_report{
	unsigned char *report_string[4];
	unsigned int  pin;
	unsigned long  irq_type;
};

struct uevent_platform_data{
	int pin_nums;
	const struct uevent_report * ur;
};

#endif
