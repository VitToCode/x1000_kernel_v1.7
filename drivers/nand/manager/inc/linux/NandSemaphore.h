#ifndef _NANDSEMPHORE_H_
#define _NANDSEMPHORE_H_

#include <linux/errno.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
typedef struct semaphore NandSemaphore;
//val 1 is unLock val 0 is Lock
static inline void InitSemphore(NandSemaphore*sem,int val){
	sema_init(sem,val);
}
static inline void DeinitSemphore(NandSemaphore* sem){
	int ret = down_killable(sem);
	if (ret == -EINTR)
		printk("waring: %s, %d\n", __func__, __LINE__);;
}

static inline void Semphore_wait(NandSemaphore* sem){
	down(sem);
}
// timeout return < 0 
static inline int Semphore_waittimeout(NandSemaphore* sem,long jiffies){
	return down_timeout(sem,jiffies);
}

static inline void Semphore_signal(NandSemaphore* sem){
	up(sem);
}

typedef struct mutex NandMutex;
static inline void InitNandMutex(NandMutex *mutex){
	mutex_init(mutex);
}
static inline void DeinitNandMutex(NandMutex *mutex){
	mutex_destroy(mutex);
}

static inline void NandMutex_Lock(NandMutex *mutex){
	mutex_lock(mutex);
}
static inline void NandMutex_Unlock(NandMutex* mutex){
	mutex_unlock(mutex);
}
#endif /* _NANDSEMPHORE_H_ */
