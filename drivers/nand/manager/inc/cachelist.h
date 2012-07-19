#ifndef __CACHELIST_H__
#define __CACHELIST_H__

#include "cachedata.h"

typedef struct _CacheList CacheList;
struct _CacheList {
	CacheData *top;	//top node of cachelist
	CacheData *tail;	//tail node of cachelist
	int listCount;	//node count of cachelist
};

int CacheList_Init ( CacheList **cachelist );
void CacheList_DeInit ( CacheList **cachelist );
CacheData *CacheList_get ( CacheList *cachelist, unsigned int indexid );
unsigned int CacheList_find ( CacheList *cachelist, unsigned int data );
void CacheList_Insert ( CacheList *cachelist, CacheData *data );
CacheData *CacheList_getTail ( CacheList *cachelist );

#endif
