#ifndef _NANDSEMPHORE_H_
#define _NANDSEMPHORE_H_

#include "clib.h"

typedef sem_t NandSemaphore;
//val 1 is unLock val 0 is Lock
void InitSemaphore(NandSemaphore *sem,int val);
void DeinitSemaphore(NandSemaphore* sem);
void Semaphore_wait(NandSemaphore* sem);
int Semaphore_waittimeout(NandSemaphore* sem,long jiffies);
int Semaphore_signal(NandSemaphore* sem);

typedef pthread_mutex_t NandMutex;
void InitNandMutex(NandMutex *mutex);
void DeinitNandMutex(NandMutex *mutex);

void NandMutex_Lock(NandMutex *mutex);
void NandMutex_Unlock(NandMutex* mutex);

#endif /* _NANDSEMPHORE_H_ */
