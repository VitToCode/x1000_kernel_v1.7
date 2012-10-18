/*
 * This file is subject to handle the messages ,in order to guarantee the 
 * threads polled the messages can  work together concerted.
 *
 *
 */
#include "clib.h"
#include "taskmanager.h"
#include "NandAlloc.h"
#include "zonememory.h"
#include "nanddebug.h"

/******************MACRO definition**************/
#define UNITSIZE     16
#define UNITDIM(x)  (((x) + UNITSIZE - 1 ) / UNITSIZE)

/*  It inserts the message into the messagelist from high prio to low prio. 
 *  @msg:
 *  @type: 1--The caller will wait the msg ,until the msg is completely handled.
 *            After handle,the Qtask will release the relative semaphore.
 *         0--The caller will not wait .
 */
int Message_Post (int handle, Message *msg, int type ){
	int zid;
	TaskManager* task = (TaskManager*)handle;
	MessageList  *msglist = NULL;
	NandSemaphore *sem = NULL;
	
	NandMutex_Lock(&task->taskmutex);
	zid = task->zid;
	msglist = (MessageList*)ZoneMemory_NewUnits(zid,UNITDIM(sizeof(MessageList)));
	if (msglist == NULL){
		ndprint(TASKMANAGER_ERROR, "ERROR:Nand alloc continue memory error func %s line %d \n",__FUNCTION__,__LINE__);
		NandMutex_Unlock(&task->taskmutex);
		return -1;
	}
	memcpy(&msglist->msg,msg,sizeof(Message));
	msglist->type = type;
	if(type == WAIT)
	{
		sem = (NandSemaphore *)ZoneMemory_NewUnits(zid,UNITDIM(sizeof(NandSemaphore)));
		InitSemaphore(sem,0);
		msglist->msghandle = (int)sem;

	}else{
		msglist->msghandle = task->uniqueid;
		task->uniqueid++;
	}
	msglist->head.next = NULL;

	singlelist_add_tail(&task->msgtop,&msglist->head);
		
  	NandMutex_Unlock(&task->taskmutex);
	
	/*Insert one msg,the task_sem will add 1*/
	Semaphore_signal(&task->tasksem);	
	return msglist->msghandle;
}

/*  It will be called after post() with the type=1.
 *  When recieved the relative semaphore, it's node will be delete. If you want to 
 *  recieve the semaphore again ,please post it again  with the type 1.
 */
int  Message_Recieve (int handle,int msghandle){
	TaskManager* task = (TaskManager*)handle;
	MessageList  *msglist;
	NandSemaphore *sem = (NandSemaphore *)msghandle;
	struct singlelist *head;
	
	int havemsg = 0;
	int zid;
	int ret = -1;
	zid = task->zid;
	
	NandMutex_Lock(&task->taskmutex);
	
	singlelist_for_each(head,task->msgtop.next){
		msglist = singlelist_entry(head,MessageList,head);
		if(msglist->type == WAIT && msglist->msghandle == msghandle){
			havemsg = 1;
			break;
		} 
	}
	if(havemsg == 0){
		singlelist_for_each(head,task->finishtop.next){
			msglist = singlelist_entry(head,MessageList,head);
			if(msglist->type == WAIT && msglist->msghandle == msghandle){
				havemsg = 1;
				break;
			}
		}
	}
	NandMutex_Unlock(&task->taskmutex);
	if(havemsg == 0){
		ndprint(TASKMANAGER_INFO,"WARNING:Not find wait msg handle = %d \n", msghandle);
	}
	else {
		Semaphore_wait(sem);		
		NandMutex_Lock(&task->taskmutex);
		ret = msglist->bexit;
		DeinitSemaphore(sem);
		
		singlelist_del(&task->finishtop,&msglist->head);

		ZoneMemory_DeleteUnits(zid,sem,UNITDIM(sizeof(NandSemaphore)));
		ZoneMemory_DeleteUnits(zid,msglist,UNITDIM(sizeof(MessageList)));
		NandMutex_Unlock(&task->taskmutex);	
	}			
	return ret;
}


/*  This function registe the msgid and function. After registe, all the nodes
 *   will not be delete,so don't register the same msgid with different function.                               
 */
int Task_RegistMessageHandle (int handle,int (*fun)(int), int msgid ){

	
	TaskManager* task = (TaskManager*)handle;
	HandleFuncList *hfl;
	int zid;
	zid = task->zid;
	if ( NULL == fun )
		return PRAM_ERROR;
	
	if (msgid == IDLE_MSG_ID){
		task->IdleFunc = fun;
		return 0;
	}
	NandMutex_Lock(&task->taskmutex);	
   	hfl = (HandleFuncList*)ZoneMemory_NewUnits(zid,UNITDIM(sizeof(HandleFuncList)));  
	if (hfl == NULL){
		ndprint(TASKMANAGER_ERROR, "ERROR:Nand alloc continue memory error func %s line %d \n",__FUNCTION__,__LINE__);
		NandMutex_Unlock(&task->taskmutex);
		return -1;
	}
	hfl->hfunc.fun = fun;
	hfl->hfunc.msgid = msgid;
	hfl->head.next = NULL;
	singlelist_add_tail(&task->functop,&hfl->head);
	NandMutex_Unlock(&task->taskmutex);

	return 0;
}

static RESULT Qtask(void *handle){
	int  (*fun)(int);
	TaskManager* task = (TaskManager*)handle;
	MessageList  *msglist = NULL;
	struct singlelist *head;
	HandleFuncList *hfl;
	int ret = -1;
	int zid = task->zid;
	while(1){
		Semaphore_wait(&task->tasksem);


		if(task->msgtop.next == NULL){
			continue;
		}
		if(task->functop.next == NULL){
			continue;
		}
		
		NandMutex_Lock(&task->taskmutex);
		msglist = singlelist_entry(task->msgtop.next,MessageList,head);
		singlelist_del(&task->msgtop,&msglist->head);
		msglist->head.next = NULL;
		singlelist_add_tail(&task->finishtop,&msglist->head);	
		fun = NULL;
		singlelist_for_each(head,task->functop.next){
			hfl = singlelist_entry(head,HandleFuncList,head);
			if(hfl->hfunc.msgid == msglist->msg.msgid){
				fun = hfl->hfunc.fun;
				break;
			}
		}
		NandMutex_Unlock(&task->taskmutex);

		if(fun)
			ret = fun(msglist->msg.data);
		else 
			ndprint(TASKMANAGER_INFO, "WARNING:Did not find the corresponding function %d\n",msglist->msg.msgid);
		
		NandMutex_Lock(&task->taskmutex);
		if(msglist->type == WAIT){
			msglist->bexit = ret;
			Semaphore_signal((NandSemaphore *)msglist->msghandle);			
		}else{

			singlelist_del(&task->finishtop,&msglist->head);
			ZoneMemory_DeleteUnits(zid,msglist,UNITDIM(sizeof(MessageList)));			
		}

		NandMutex_Unlock(&task->taskmutex);
	}
	return 0;
}

static RESULT Idletask(void *handle)
{
	TaskManager* task = (TaskManager*)handle;
	int   (*idlefunc)(int); 
	
	while(1){
		if(task->IdleFunc != NULL){
			idlefunc = task->IdleFunc;
			idlefunc((int)(task->prData));
		}
		else
			nm_sleep(1);
	}

	return 0;
}

/* Alloc the structures' memory, init the semaphores and create the Qtask and Idletask.*/
int Task_Init(int context){

	int zid;
	TaskManager* task;
	zid = ZoneMemory_Init(UNITSIZE);
	task = (TaskManager*)ZoneMemory_NewUnits(zid,UNITDIM(sizeof(TaskManager)));
	if (task == NULL){
		ndprint(TASKMANAGER_ERROR, "ERROR:Nand alloc continue memory error func %s line %d \n",__FUNCTION__,__LINE__);
		return -1;
	}
	
	task->msgtop.next = NULL;
	task->functop.next = NULL;
	task->finishtop.next = NULL;
	task->zid = zid;
	task->prData = (void *)context;
	task->IdleFunc = NULL;
	
	InitSemaphore(&task->tasksem,0);
	InitNandMutex(&task->taskmutex);

	task->qid = CreateThread(Qtask,(void *)task,1,"Qtask");
	task->idleid = CreateThread(Idletask,(void *)task,0,"Idletask");
	task->uniqueid = 0;

	return (int)task;
}

/*** Release the memory, deinit the semaphores and exit the two threads.***/
void Task_Deinit(int handle){
	TaskManager* task = (TaskManager*)handle;
	int zid = task->zid;
	DeinitSemaphore(&task->tasksem);
	DeinitNandMutex(&task->taskmutex);
   	ExitThread( &task->qid );
	ExitThread( &task->idleid );
	ZoneMemory_DeInit(zid);
}

