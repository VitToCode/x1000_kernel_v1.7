#include <NandSemaphore.h>
#include "clib.h"

void InitSemaphore(NandSemaphore*sem,int val){
	sem_init(sem,0,val);
}
void DeinitSemaphore(NandSemaphore* sem){
	sem_destroy(sem);
}
void Semaphore_wait(NandSemaphore* sem){
	sem_wait(sem);
}
//1 jiffies = 10 msec
int Semaphore_waittimeout(NandSemaphore* sem,long jiffies){
	unsigned long long d;
	struct timespec ts;
	clock_gettime(CLOCK_REALTIME, &ts);
//	printf("111 ts.tv_sec = %d ts.tv_nsec = %d\n",(int)ts.tv_sec,(int)ts.tv_nsec);
	d = (long long)ts.tv_sec * 1000000000L	+ (long long)ts.tv_nsec + (long long)jiffies * 10000000L;
	ts.tv_sec = d / 1000000000L;
	ts.tv_nsec = d % 1000000000L;
//	printf("222 ts.tv_sec = %d ts.tv_nsec = %d\n",(int)ts.tv_sec,(int)ts.tv_nsec);

  	return sem_timedwait(sem,&ts);
}
int Semaphore_signal(NandSemaphore* sem){
	return sem_post(sem);
}


void InitNandMutex(NandMutex *mutex){
	if(pthread_mutex_init(mutex, NULL))
		printf("error: NandMutex InitNandMutex!\n");
}
void DeinitNandMutex(NandMutex *mutex){
	pthread_mutex_destroy(mutex);
}

void NandMutex_Lock(NandMutex *mutex){
	pthread_mutex_lock(mutex);
}
void NandMutex_Unlock(NandMutex* mutex){
	pthread_mutex_unlock(mutex);
}
