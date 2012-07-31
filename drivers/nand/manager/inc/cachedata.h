#ifndef __CACHEDATA_H__
#define __CACHEDATA_H__

#include "singlelist.h"

#define UNIT_SIZE sizeof(unsigned int)

typedef struct _CacheData CacheData;
struct _CacheData {
	unsigned int IndexID;  	//starting sectorid of this cachedata
	unsigned int *Index; 	//data of cache
	unsigned short IndexCount;	//count of pageid in index
	unsigned int unitLen;	//how mang sectors one pageid indicate
	struct singlelist head;
};

CacheData * CacheData_Init ( unsigned short indexcount, unsigned int unitlen );
void CacheData_DeInit ( CacheData *cachedata );
unsigned int CacheData_get ( CacheData *cachedata, unsigned int indexid );
void CacheData_set ( CacheData *cachedata, unsigned int indexid, unsigned int data );
unsigned int CacheData_find ( CacheData *cachedata, unsigned int data );
void CacheData_update ( CacheData *cachedata, unsigned int startID,unsigned char *data);
#endif
