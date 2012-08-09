#include "clib.h"
#include <NandThread.h>

PNandThread CreateThread(PThreadFunction fn,void *data,int prio,char *name){
	PNandThread thread;
	pthread_attr_t attr; 
	struct sched_param param; /* 线程优先级结构体 */

    pthread_attr_init(&attr); /* 初始化线程属性对象，这时是默认值 */

	/* 设置线程绑定 */

	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
    /* 修改线程优先级 */

	pthread_attr_getschedparam(&attr, &param);

	param.sched_priority = 20;

	pthread_attr_setschedparam(&attr, &param);
	
	pthread_attr_setschedpolicy(&attr,SCHED_FIFO);/*设置线程调度策略*/ 

	pthread_attr_setdetachstate(&attr,PTHREAD_CREATE_DETACHED);

	if (pthread_create(&thread, NULL, fn, data) ) {
		printf("error creating thread.");
		exit(-1);
	}
	pthread_attr_destroy(&attr);
	return thread;
}
int ExitThread(PNandThread *thread){
	pthread_cancel(*thread);
	return 0;
}
void SetThreadPrio(PNandThread *thread,int prio){
	//struct sched_param param = { .sched_priority = 1 };
	switch(prio)
	{
	case 0:
		
		//sched_setscheduler(thread->thread, SCHED_IDLE, &param);
		break;
	case 1:
		//sched_setscheduler(thread->thread, SCHED_FIFO, &param);
		break;
	}
}

