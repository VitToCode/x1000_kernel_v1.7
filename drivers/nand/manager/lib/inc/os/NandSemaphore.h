#ifndef _NANDSEMPHORE_H_
#define _NANDSEMPHORE_H_

/************* NandSemaphore **********/
#ifdef  __KERNEL__
#include <linux/semaphore.h>
typedef struct semaphore NandSemaphore;
#else
#include <semaphore.h>
typedef sem_t NandSemaphore;
#endif
void InitSemaphore(NandSemaphore*sem,int val); //val 1 is unLock val 0 is Lock
void DeinitSemaphore(NandSemaphore* sem);
void Semaphore_wait(NandSemaphore* sem);
int Semaphore_waittimeout(NandSemaphore* sem,long jiffies); //timeout return < 0
void Semaphore_signal(NandSemaphore* sem);

/************** andMutex **************/
#ifdef  __KERNEL__
#include <linux/mutex.h>
typedef struct mutex NandMutex;
#else
#include <pthread.h>
typedef pthread_mutex_t NandMutex;
#endif
void InitNandMutex(NandMutex *mutex);
void DeinitNandMutex(NandMutex *mutex);
void NandMutex_Lock(NandMutex *mutex);
void NandMutex_Unlock(NandMutex* mutex);

#endif /* _NANDSEMPHORE_H_ */
