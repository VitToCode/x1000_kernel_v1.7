#ifndef __TASK_H__
#define __TASK_H__

#include "singlelist.h"
#include "NandSemaphore.h"
#include "zonememory.h"
#include "NandThread.h"

#define PRAM_ERROR     (-1)

typedef struct _Message         Message;
typedef struct _MessageList     MessageList;

typedef struct _HandleFunc      HandleFunc;
typedef struct _HandleFuncList  HandleFuncList; 

typedef struct _TaskManager    TaskManager;

struct _Message {
    unsigned int msgid;
    int prio;
    unsigned int data;
};

struct _MessageList {
    Message msg;
	int msghandle;
	int type;
	int bexit;
    struct singlelist head;
};

struct _HandleFunc {
	int  (*fun)(int);
	int        msgid;
};
	
struct _HandleFuncList {
	struct singlelist head;
	HandleFunc hfunc;	
};

struct _TaskManager{
	NandSemaphore    tasksem;
	NandMutex        taskmutex;
	
	struct singlelist msgtop;
	struct singlelist finishtop;

	struct singlelist functop;
	int uniqueid;
	
	PNandThread      qid;
	PNandThread      idleid;
	int              zid;
  	void *prData;
	
	int  (*IdleFunc)(int);
};

enum Type {
	NOWAIT,
	WAIT,	
};

enum MessageId {
	IDLE_MSG_ID = -2,
	NORMAL_MSG_ID = 0,
	FOLLOW_RECYCLE_ID,
	BOOT_RECYCLE_ID,
	WRITE_READ_ECC_ERROR_ID,
	FORCE_RECYCLE_ID,
	READ_FIRST_PAGEINFO_ERROR_ID,
	READ_PAGE0_ERROR_ID,
	READ_PAGE1_ERROR_ID,
	READ_PAGE2_ERROR_ID,
	READ_ECC_ERROR_ID,
};

enum Prio {
	FOLLOW_RECYCLE_PRIO,
	BOOT_RECYCLE_PRIO,
	READ_FIRST_PAGEINFO_ERROR_PRIO,
	READ_PAGE0_ERROR_PRIO,
	READ_PAGE1_ERROR_PRIO,
	READ_PAGE2_ERROR_PRIO,
	READ_ECC_ERROR_PRIO,
	FORCE_RECYCLE_PRIO,//write before FORCE_RECYCLE_PRIO when add other prio
};

int Task_RegistMessageHandle (int handle, int (*func)(int), int msgid );
int Message_Post (int handle,Message *msg, int type );
int Task_Init(int context);
int Message_Recieve (int handle,int msghandle);
void Task_Deinit(int handle);


#endif
