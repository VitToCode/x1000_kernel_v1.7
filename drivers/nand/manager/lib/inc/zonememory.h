#ifndef __ZONEMEMORY_H__
#define __ZONEMEMORY_H__
#include "os/NandAlloc.h"
#include "singlelist.h"
#include "os/NandSemaphore.h"
typedef struct _ZoneBuffer ZoneBuffer;

struct _ZoneBuffer {
    void* mBuffer;
    unsigned int* bitmap;
	int bitsize;
	struct singlelist head;
};

typedef struct _ZoneMemory ZoneMemory;

struct _ZoneMemory {
    ZoneBuffer* top;
	int usize;
	NandMutex mutex;
};
int ZoneMemory_Init (int unitsize );
void* ZoneMemory_NewUnit(int zid);
void ZoneMemory_DeleteUnit(int zid,void * pu );

void* ZoneMemory_NewUnits(int zid,int count);
void ZoneMemory_DeleteUnits(int zid,void * pu,int count);
void ZoneMemory_DeInit(int zid);
#endif
