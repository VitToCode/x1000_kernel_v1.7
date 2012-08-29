#ifndef __CACHELIST_H__
#define __CACHELIST_H__

#include "cachedata.h"

typedef struct _CacheList CacheList;
struct _CacheList {
	struct list_head top;
	int listCount;	//node count of cachelist
};

CacheList* CacheList_Init (void);
void CacheList_DeInit ( CacheList *cachelist );
CacheData *CacheList_get ( CacheList *cachelist, unsigned int indexid );
CacheData *CacheList_getTop ( CacheList *cachelist );

unsigned int CacheList_find ( CacheList *cachelist, unsigned int data );
void CacheList_Insert ( CacheList *cachelist, CacheData *data );
CacheData *CacheList_getTail ( CacheList *cachelist );
void CacheList_Dump(CacheList *cachelist);

#endif
