#include <linux/errno.h>

#include "os/NandSemaphore.h"

//val 1 is unLock val 0 is Lock
void InitSemaphore(NandSemaphore *sem,int val)
{
	sema_init(sem,val);
}

void DeinitSemaphore(NandSemaphore *sem)
{
	int ret;
	up(sem);
	ret = down_killable(sem);
	if (ret == -EINTR)
		printk("waring: %s, %d\n", __func__, __LINE__);
}

void Semaphore_wait(NandSemaphore *sem)
{
	down(sem);
}

//timeout return < 0
int Semaphore_waittimeout(NandSemaphore *sem,long jiffies)
{
	return down_timeout(sem,jiffies);
}

void Semaphore_signal(NandSemaphore *sem)
{
	up(sem);
}

//#define DEBUG_NDMUTEX
void InitNandMutex(NandMutex *mutex)
{
	mutex_init(mutex);
}

void DeinitNandMutex(NandMutex *mutex)
{
	mutex_destroy(mutex);
}

void NandMutex_Lock(NandMutex *mutex)
{
	mutex_lock(mutex);
}

void NandMutex_Unlock(NandMutex* mutex)
{
	mutex_unlock(mutex);
}
