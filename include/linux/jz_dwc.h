#ifndef __JZ_DWC_H__
#define __JZ_DWC_H__

#include <linux/spinlock.h>
#include <linux/workqueue.h>

struct jzdwc_pin {
	short				num;
#define LOW_ENABLE			0
#define HIGH_ENABLE			1
	short 				enable_level;
};

struct dwc_jz_pri {
	struct clk		*clk;

	int 			irq;
	struct jzdwc_pin 	*dete;
	struct delayed_work	work;

	struct regulator 	*vbus;
	struct regulator 	*ucharger;

	spinlock_t		lock;
	struct mutex		mutex;
};

#endif
