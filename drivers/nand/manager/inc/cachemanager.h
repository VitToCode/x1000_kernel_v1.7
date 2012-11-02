#ifndef __CACHEMANAGER_H__
#define __CACHEMANAGER_H__
#include "vnandinfo.h"
#include "cachelist.h"
#include "cachedata.h"
#include "pageinfo.h"
#include "nandpageinfo.h"
#include "NandSemaphore.h"

typedef struct _CacheManager CacheManager;
typedef struct _LockCacheDataTable LockCacheDataTable;
typedef struct _PageCache PageCache;

struct _LockCacheDataTable{
	unsigned int sectorid;
	CacheData *L1;    //correspond CacheManager L1Info; 
	CacheData *L2;    //correspond CacheManager L2Info; 
	CacheData *L3;    //correspond CacheManager L3Info; 
	CacheData *L4;    //correspond CacheManager L4Info; 
};
struct _PageCache{
    	unsigned char *pageinfobuf;
	NandPageInfo *nandpageinfo;
	unsigned int pageid;
	VNandInfo *vnand;
	int bufferlistid;
};
struct _CacheManager {
	CacheData *L1Info;	//L1 cache
	CacheList *L2Info;	//L2 cache
	CacheList *L3Info;	//L3 cache
	CacheList *L4Info;	//L4 cache


	unsigned int L1InfoLen;	//how mang Bytes of one cachedata' index
	unsigned int L2InfoLen;
	unsigned int L3InfoLen;
	unsigned int L4InfoLen;
	unsigned int L1UnitLen;	//how mang sectors one pageid indicate
	unsigned int L2UnitLen;
	unsigned int L3UnitLen;
	unsigned int L4UnitLen;
	NandMutex mutex;

	PageCache pagecache;

	LockCacheDataTable lct;
	PageInfo pageinfo;
};

enum Update_Type {
	UPDATE_L2,
	UPDATE_L3,
};

int CacheManager_Init ( int context );
void CacheManager_DeInit ( int context );
unsigned int CacheManager_getPageID ( int context, unsigned int sectorid );
void CacheManager_lockCache ( int context, unsigned int sectorid, PageInfo **pi);
void CacheManager_unlockCache ( int context,PageInfo *pi);
int CacheManager_CheckIsCacheMem ( int context,unsigned int startpageid,unsigned int count);
int CacheManager_CheckCacheAll ( int context,unsigned int startpageid,unsigned int count);
#endif
