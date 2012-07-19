#ifndef __RECYCLE_H__
#define __RECYCLE_H__

#include "hash.h"
#include "zone.h"
#include "pagelist.h"
#include "pageinfo.h"
#include "cachemanager.h"
#include "NandSemaphore.h"

typedef struct _Recycle Recycle;

struct _Recycle {
	Hash *hashTable;
	unsigned int pageID;
	int taskStep;
	Zone *rZone;
	PageInfo *prevpageinfo;
	PageInfo *curpageinfo;
	PageInfo *nextpageinfo;
	PageInfo *prev_writepageinfo;
	PageInfo *writepageinfo;
	unsigned int startsectorID;
	unsigned int write_cursor;
	unsigned int alloc_num;
	unsigned int *record_writeadd;
	PageList *pagelist;
	PageList *read_pagelist;
	PageList *write_pagelist;
	unsigned int  buflen;
	unsigned int end_findnextpageinfo;
	NandMutex mutex;
	unsigned int force; 
	int context;

	/**** force recycle ****/
	Zone *force_rZone;
	PageInfo *force_prevpageinfo;
	PageInfo *force_curpageinfo;
	PageInfo *force_nextpageinfo;
	PageInfo *force_prev_writepageinfo;
	PageInfo *force_writepageinfo;
	unsigned int force_startsectorID;
	unsigned int force_write_cursor;
	unsigned int force_alloc_num;
	unsigned int *force_record_writeadd;
	PageList *force_pagelist;
	PageList *force_read_pagelist;
	PageList *force_write_pagelist;
	unsigned int force_buflen;
	unsigned int force_end_findnextpageinfo;
};

enum TaskStep{
	RECYIDLE,
	RECYSTART,
	GETZONE,
	READFIRSTINFO,
	FINDVAILD,
	MERGER,
	RECYCLE,
	READNEXTINFO,
	FINISH,
};

int Recycle_Init(int context);
void Recycle_DeInit(int context);
int Recycle_OnFragmentHandle ( int context );
int Recycle_OnBootRecycle ( int context );
int Recycle_OnForceRecycle ( int context );
int Recycle_OnNormalRecycle ( int context );
/* for test add interface  */
int Recycle_getRecycleZone ( Recycle *rep);
int Recycle_FindFirstPageInfo ( Recycle *rep);
int Recycle_FindValidSector ( Recycle *rep);
int Recycle_MergerSectorID ( Recycle *rep);
int Recycle_RecycleReadWrite ( Recycle *rep);
int Recycle_FindNextPageInfo ( Recycle *rep);
int Recycle_FreeZone ( Recycle *rep );

#endif
