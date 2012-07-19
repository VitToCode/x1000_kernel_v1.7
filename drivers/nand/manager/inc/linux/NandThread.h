#ifndef _NANDTHREAD_H_
#define _NANDTHREAD_H_

#include <linux/sched.h>
#include <linux/kthread.h>

#define NAND_MIN_PRIO 0  //IDLE
//#define NAND_MIN_PRIO 1  //NORMAL

typedef struct task_struct* PNandThread;
typedef int (*PThreadFunction)(void *data);

static inline PNandThread CreateThread(PThreadFunction fn,void *data,int prio,char *name){
	PNandThread thread;
	struct sched_param param = { .sched_priority = 1 };
	
	thread = kthread_create(fn, (void *)thread, "dthread");
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

static inline int ExitThread(PNandThread thread){
	
	return kthread_stop(thread);
}

static inline void SetThreadPrio(PNandThread thread,int prio){
	struct sched_param param = { .sched_priority = 1 };
	switch(prio)
	{
	case 0:
		sched_setscheduler(thread, SCHED_IDLE, &param);
		break;
	case 1:
		sched_setscheduler(thread, SCHED_FIFO, &param);
		break;
	}
}

#endif /* _NANDTHREAD_H_ */
