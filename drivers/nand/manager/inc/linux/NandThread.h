#ifndef _NANDTHREAD_H_
#define _NANDTHREAD_H_

#include <linux/sched.h>
#include <linux/kthread.h>

#define NAND_MIN_PRIO 0  //IDLE
//#define NAND_MIN_PRIO 1  //NORMAL

typedef struct task_struct* PNandThread;
typedef int RESULT;
typedef RESULT (*PThreadFunction)(void *data);

static inline PNandThread CreateThread(PThreadFunction fn,void *data,int prio,char *name){
	PNandThread thread = NULL;
	struct sched_param param = { .sched_priority = 1 };
	char threadName[80];
	static int index = 1;

	sprintf(threadName, "%s_%d", name, index);
	thread = kthread_create(fn, data, threadName);
	switch(prio)
	{
	case 0:
		sched_setscheduler(thread, SCHED_IDLE, &param);
		break;
	default:
		sched_setscheduler(thread, SCHED_FIFO, &param);
		break;
	}
	if (!IS_ERR(thread))
		wake_up_process(thread);

	return thread;
}

static inline int ExitThread(PNandThread *thread){	
	return kthread_stop(*thread);
}

#endif /* _NANDTHREAD_H_ */
