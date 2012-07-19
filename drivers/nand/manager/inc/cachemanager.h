#ifndef __CACHEMANAGER_H__
#define __CACHEMANAGER_H__

#include "cachelist.h"
#include "cachedata.h"
#include "pageinfo.h"
#include "NandSemaphore.h"

typedef struct _CacheManager CacheManager;
struct _CacheManager {
	CacheData *L1Info;	//L1 cache
	CacheData *L2Info;	//L2 cache
	CacheList *L3Info;		//L3 cache
	CacheList *L4Info;		//L4 cache
	CacheData *cachedata;	//all cachedata of L1, L2, L3 and L4 
	CacheData *locked_data;	//find locked cachedata
	unsigned int L1InfoLen;	//how mang Bytes of one cachedata' index
	unsigned int L2InfoLen;
	unsigned int L3InfoLen;
	unsigned int L4InfoLen;
	unsigned short L1UnitLen;	//how mang sectors one pageid indicate
	unsigned short L2UnitLen;
	unsigned short L3UnitLen;
	unsigned short L4UnitLen;
	NandMutex mutex;
};

int CacheManager_Init ( int context );
void CacheManager_DeInit ( int context );
void CacheManager_updateCache ( int context, unsigned int sectorid, unsigned int pageid );
unsigned int CacheManager_getPageID ( int context, unsigned int sectorid );
void CacheManager_lockCache ( int context, unsigned int sectorid, PageInfo **pi );
void CacheManager_unlockCache ( int context, PageInfo *pi);

#endif
