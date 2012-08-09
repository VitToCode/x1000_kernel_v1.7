#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "testfunc.h"
#include "taskmanager.h"
#include "NandThread.h"
#include "zonememory.h"

extern  MessageList*  messagelist;
extern  HandleFuncList*  handlefunclist;
static PNandThread   testid,waitid;
Message msg0,msg1,msg2,msg3,msg4;

int thandle;

int test0(int para)
{
	printf("%s  para_0 = %d\n",__func__,para);
	return 0;
}
int test1(int para)
{
	printf("%s  para_1 = %d\n",__func__,para);
	return 0;
}
int test2(int para)
{
	printf("%s  para_2 = %d\n",__func__,para);
	return 0;
}
int test3(int para)
{
	printf("--------------%s  para_2 = %d----------\n",__func__,para);
	return 0;
}
int msgid[8];
void* Testtask(void *v)
{
	v = v;
	
	Task_RegistMessageHandle(thandle,test0,0);
	Task_RegistMessageHandle(thandle,test1,4);
	Task_RegistMessageHandle(thandle,test0,1);
	Task_RegistMessageHandle(thandle,test1,2);	  
	Task_RegistMessageHandle(thandle,test2,3);

	Task_RegistMessageHandle(thandle,test3,-2);

	while(1)
	{
		printf("++++++++++Test++++++++msg11 recieve+++++++++++++++++++\n");
		msgid[0] = Message_Post(thandle,&msg1,1);
		Message_Recieve(thandle,msgid[0]);
		printf("++++++++++Test++++++++msg1 recieve+++++++++++++++++++\n");
		msgid[1] = Message_Post(thandle,&msg2,1);

		Message_Recieve(thandle,msgid[1]);
		printf("++++++++++Test++++++++msg2 recieve+++++++++++++++++++\n");
		msgid[3] = Message_Post(thandle,&msg3,1);
		Message_Recieve(thandle,msgid[3]);
		printf("++++++++++Test++++++++msg3 recieve+++++++++++++++++++\n");
		Message_Post(thandle,&msg3,0);	 
		sleep(100);
	}
	return 0;
}

void* Waittask(void *v)
{
	v = v;
	while(1)
	{
		printf("+++++++++Wait+++++++++msg00 recieve+++++++++++++++++++ 0x%08x\n",thandle);
				
		msgid[5] = Message_Post(thandle,&msg0,1);
		Message_Recieve(thandle,msgid[5]);
		printf("+++++++++Wait+++++++++msg0 recieve+++++++++++++++++++ 0x%08x\n",thandle);
		msgid[6] = Message_Post(thandle,&msg4,1);
		printf("+++++++++Wait+++++++++msg0ss recieve+++++++++++++++++++ 0x%08x\n",thandle);

		Message_Recieve(thandle,msgid[6]);
		printf("+++++++++Wait+++++++++msg4 recieve+++++++++++++++++++\n");
		sleep(1);
	}
	return 0;
}


static int Handle(int argc, char *argv[]){
	//MessageList *pcurr = NULL;
	//int (*fun)(int)= NULL;
	//int i;

	msg0.msgid = 0;
	msg0.prio = 0;
	msg0.data = 0;

	msg1.msgid = 1;
	msg1.prio = 1;
	msg1.data = 1;

	msg2.msgid = 2;
	msg2.prio = 2;
	msg2.data = 2;

	msg3.msgid = 3;
	msg3.prio = 3;
	msg3.data = 3;

	msg4.msgid = 4;
	msg4.prio = 4;
	msg4.data = 4;

	thandle = Task_Init(0);
	testid = CreateThread(Testtask,NULL,0,"Testtask");
printf("%s(%s) %d\n",__FUNCTION__,__FILE__,__LINE__);

	waitid = CreateThread(Waittask,NULL,0,"Waittask");
printf("%s(%s) %d\n",__FUNCTION__,__FILE__,__LINE__);

	pthread_join(testid,NULL);
	pthread_join(waitid,NULL);
	printf("Thread exit\n");

	return 0;
}



static char *mycommand="task";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
