#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "testfunc.h"
#include "cachelist.h"

#define L1_UNITLEN  128 * 1024 * 1024 / 512	//128M
#define L2_UNITLEN  2 * 1024 * 1024 / 512	//2M
#define L3_UNITLEN  16 * 1024 / 512		//16K
#define L4_UNITLEN  1				//512B

#define L1_INDEXCOUNT 2 * 1024 / UNIT_SIZE	//2K l1info
#define L2_INDEXCOUNT 256 / UNIT_SIZE		//256B l2info
#define L3_INDEXCOUNT 512 / UNIT_SIZE		//512B l3info
#define L4_INDEXCOUNT 1024 / UNIT_SIZE		//1K l4info

#define L1_CACHEDATA_COUNT 1
#define L2_CACHEDATA_COUNT 1
#define L3_CACHEDATA_COUNT 32
#define L4_CACHEDATA_COUNT 32

#define INDEXCOUNT L4_INDEXCOUNT
#define UNITLEN L4_UNITLEN
#define CACHEDATA_COUNT L4_CACHEDATA_COUNT

static void dumpCacheList(CacheList *cachelist)
{
	CacheList_Dump(cachelist);
}

static int Handle(int argc, char *argv[]){
	int i;
	CacheList *cachelist;
	CacheData *cachedata[CACHEDATA_COUNT];
	unsigned int sectorid;
	CacheData *data;

	for (i = 0; i < CACHEDATA_COUNT; i++) {
		cachedata[i] = CacheData_Init(INDEXCOUNT, UNITLEN);
		cachedata[i]->IndexID = INDEXCOUNT * UNITLEN * i;
	}
	
	cachelist = CacheList_Init();
	if (!cachelist) {
		printf("CacheList_Init failed!\n");
		return -1;
	}
	printf("CacheList_Init OK!!!!!!\n");
	dumpCacheList(cachelist);

	for (i = 0; i < CACHEDATA_COUNT; i++) {
		CacheList_Insert(cachelist,cachedata[i]);
		dumpCacheList(cachelist);
	}
	printf("CacheList_Insert %d cachedata successed!!!!!!\n\n", CACHEDATA_COUNT);


	for (i = 0; i < CACHEDATA_COUNT - 5; i++) {
		sectorid = INDEXCOUNT * UNITLEN * i + 10;
		data = CacheList_get(cachelist, sectorid);
		if (!data)
			printf("CacheList_get failed!\n");
		else
			printf("get:indexID = %d, sectorid = %d\n", data->IndexID, sectorid);
		CacheList_Insert(cachelist,data);
		dumpCacheList(cachelist);
	}
	printf("CacheList_get OK!!!!!!!!\n\n");
	dumpCacheList(cachelist);

	for (i = 0; i < CACHEDATA_COUNT; i++) {
		data = CacheList_getTail(cachelist);
		if (!data)
			printf("CacheList_getTail failed!\n");
		else
			printf("gatTail:indexID = %d\n", data->IndexID);
		dumpCacheList(cachelist);
	}
	printf("CacheList_getTail OK!!!!!!!!\n\n");

	CacheList_DeInit(cachelist);
	printf("CacheList_DeInit OK!!!!!!!!!!!!\n\n");

	return 0;
}



static char *mycommand="cl";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
