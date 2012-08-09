/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2007 by dsqiu (dsqiu@ingenic.cn)
 */

#include "clib.h"
#include "testfunc.h"
#include <NandThread.h>
#include <NandSemaphore.h>

/* Producer/consumer program illustrating conditional variables */

/* Size of shared buffer */
#define BUF_SIZE 3

int buffer[BUF_SIZE];	/*shared buffer */
int add=0;		/* place to add next element */
int rem=0;		/* place to remove next element */
int num=0;		/* number elements in buffer */
NandMutex m;	/* mutex lock for buffer */
pthread_cond_t c_cons=PTHREAD_COND_INITIALIZER; /* consumer waits on this cond var */
pthread_cond_t c_prod=PTHREAD_COND_INITIALIZER; /* producer waits on this cond var */

/* Produce value(s) */
void *producer(void *param)
{
	int i;
	for (i=1; i<=20; i++) {
		/* Insert into buffer */
		NandMutex_Lock(&m);
		if (num > BUF_SIZE) exit(1);	/* overflow */
		while (num == BUF_SIZE)	/* block if buffer is full */
			pthread_cond_wait (&c_prod, &m);
		/* if executing here, buffer not full so add element */
		buffer[add] = i;
		add = (add+1) % BUF_SIZE;
		num++;
		NandMutex_Unlock(&m);

		pthread_cond_signal (&c_cons);
		printf ("producer: inserted %d\n", i);  fflush (stdout);
	}
	printf("producer quiting\n");  fflush(stdout);
	ExitThread((PNandThread *)param);
	return 0;
}

/* Consume value(s); Note the consumer never terminates */
void *consumer(void *param)
{
	int i;
	while (1) {
		NandMutex_Lock(&m);
		if (num < 0) exit(1);   /* underflow */
		while (num == 0)	 /* block if buffer empty */
			pthread_cond_wait (&c_cons, &m);
		/* if executing here, buffer not empty so remove element */
		i = buffer[rem];
		rem = (rem+1) % BUF_SIZE;
		num--;
		NandMutex_Unlock(&m);

		pthread_cond_signal (&c_prod);
		printf ("Consume value %d\n", i);  fflush(stdout);
	}
	return 0;
}

static int Handle(int argc, char *argv[]){
	PNandThread tid1, tid2;
	printf("test thread\n");
	InitNandMutex(&m);

	tid2 = CreateThread(consumer,NULL,2,"consumer");
	tid1 = CreateThread(producer,&tid2,1,"producer");

	printf("Parent pending\n");
	pthread_join(tid1,NULL);
	printf("tid1 exit\n");
	pthread_join(tid2,NULL);
	printf("tid2 exit\n");
	DeinitNandMutex(&m);
	printf ("Parent quiting\n");
	return 0;
}



static char *mycommand="tt";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
