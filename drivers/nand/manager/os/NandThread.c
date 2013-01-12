#include <linux/sched.h>
#include <linux/kthread.h>

#include "os/NandThread.h"

PNandThread CreateThread(PThreadFunction fn,void *data,int prio,char *name)
{
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

	index ++;
	return thread;
}

int ExitThread(PNandThread *thread)
{
	return kthread_stop(*thread);
}

void SetThreadPrio(PNandThread *thread,int prio)
{
	return;
}
