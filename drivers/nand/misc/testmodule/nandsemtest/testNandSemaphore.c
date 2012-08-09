/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2007 by dsqiu (dsqiu@ingenic.cn)
 */

#include "testfunc.h"
#include <NandSemaphore.h>
#include <NandThread.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

volatile int waittime = 0;
NandSemaphore sem;
void *waitthread(void *param)
{
	int i;
	int receivecount = 0;
	for(i = 0;i < 10;i++){
		printf("===============================  wait count = %d\n",i);
		if(waittime == 0){
			Semaphore_wait(&sem);
			printf("%d receive\n",i);
		}else{
			if(Semaphore_waittimeout(&sem,(long)waittime*10) < 0)
				printf("%d timeout receive\n",i);
			else
			{
				receivecount++;
				printf("%d receive\n",i);
			}
		}
		
	}
	printf("receivecount = %d\n",receivecount);
	return 0;
}

void *postthread(void *param)
{
	int i;
	for(i = 0;i < 10;i++){
		usleep(5000 * 100);
		printf("signal %d\n",i);
		Semaphore_signal(&sem);
	}
	return 0;
}

static int Handle(int argc, char *argv[]){
	PNandThread tid1, tid2;
	if(argc < 3)
	{
		printf("./test ns 0 --wait\n");
		printf("./test ns 1 --waittimeout 0.1s\n");
		printf("./test ns 2 --waittimeout 0.2s\n");
		return -1;
	}
	printf("NandSemaphoreTest...\n");
	printf("waittime = %s\n",argv[2]);
	waittime = strtoul(argv[2],NULL,0);
	InitSemaphore(&sem,0);

	tid2 = CreateThread(waitthread,NULL,2,"waitthread");
	tid1 = CreateThread(postthread,&tid2,1,"postthread");
	
	printf("Parent pending\n");
	pthread_join(tid1,NULL);
	DeinitSemaphore(&sem);
	printf("tid1 exit\n");
	pthread_join(tid2,NULL);
	printf("tid2 exit\n");

	printf("finish!\n");
	return 0;
}



static char *mycommand="ns";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
