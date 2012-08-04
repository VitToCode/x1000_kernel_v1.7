#ifndef _NANDSEMPHORE_H_
#define _NANDSEMPHORE_H_

#include <linux/errno.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>


typedef struct semaphore NandSemaphore;
//val 1 is unLock val 0 is Lock
static inline void InitSemaphore(NandSemaphore*sem,int val){
	sema_init(sem,val);
}
static inline void DeinitSemaphore(NandSemaphore* sem){
	int ret;
	up(sem)
	ret = down_killable(sem);
	if (ret == -EINTR)
		printk("waring: %s, %d\n", __func__, __LINE__);;
}

static inline void Semaphore_wait(NandSemaphore* sem){
	down(sem);
}
// timeout return < 0 
static inline int Semaphore_waittimeout(NandSemaphore* sem,long jiffies){
	return down_timeout(sem,jiffies);
}

static inline void Semaphore_signal(NandSemaphore* sem){
	up(sem);
}

#define DEBUG_NDMUTEX

typedef struct mutex NandMutex;
static inline void InitNandMutex(NandMutex *mutex){
	mutex_init(mutex);
}
static inline void DeinitNandMutex(NandMutex *mutex){
	mutex_destroy(mutex);
}
#ifdef DEBUG_NDMUTEX
#define NandMutex_Lock(x) do{mutex_lock(x);printk("%s %d lock\n",__FILE__,__LINE__);}while(0)
#define NandMutex_Unlock(x) do{printk("%s %d unlock\n",__FILE__,__LINE__);mutex_unlock(x);}while(0)
#else
static inline void NandMutex_Lock(NandMutex *mutex){
	mutex_lock(mutex);
}
static inline void NandMutex_Unlock(NandMutex* mutex){
	mutex_unlock(mutex);
}
#endif
#endif /* _NANDSEMPHORE_H_ */
