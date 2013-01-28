#ifndef _NANDTHREAD_H_
#define _NANDTHREAD_H_

#define NAND_MIN_PRIO 0  //IDLE
#define NAND_MAX_PRIO 1  //NORMAL

#ifdef  __KERNEL__
typedef int RESULT;
#else
typedef void* RESULT;
#endif

typedef int PNandThread;
typedef RESULT (*PThreadFunction)(void *data);

PNandThread CreateThread(PThreadFunction fn,void *data,int prio,char *name);
int ExitThread(PNandThread *thread);
void SetThreadPrio(PNandThread *thread,int prio);

#endif /* _NANDTHREAD_H_ */
