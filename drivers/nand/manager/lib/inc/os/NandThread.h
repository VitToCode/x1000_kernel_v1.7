#ifndef _NANDTHREAD_H_
#define _NANDTHREAD_H_

#define NAND_MIN_PRIO 0  //IDLE
#define NAND_MAX_PRIO 1  //NORMAL

#ifdef  __KERNEL__
typedef struct task_struct* PNandThread;
typedef int RESULT;
typedef RESULT (*PThreadFunction)(void *data);
#else
typedef pthread_t PNandThread;
typedef void* RESULT;
typedef RESULT (*PThreadFunction)(void *data);
#endif

PNandThread CreateThread(PThreadFunction fn,void *data,int prio,char *name);
int ExitThread(PNandThread *thread);
void SetThreadPrio(PNandThread *thread,int prio);

#endif /* _NANDTHREAD_H_ */
