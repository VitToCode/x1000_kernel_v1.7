#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "testfunc.h"
#include "cachedata.h"

#define L1_UNITLEN  128 * 1024 * 1024 / 512	//128M
#define L2_UNITLEN  2 * 1024 * 1024 / 512	//2M
#define L3_UNITLEN  16 * 1024 / 512		//16K
#define L4_UNITLEN  1				//512B

#define L1_INDEXCOUNT 2 * 1024 / UNIT_SIZE	//2K l1info
#define L2_INDEXCOUNT 256 / UNIT_SIZE		//256B l2info
#define L3_INDEXCOUNT 512 / UNIT_SIZE		//512B l3info
#define L4_INDEXCOUNT 1024 / UNIT_SIZE		//1K l4info

#define INDEXCOUNT 10
#define UNITLEN L4_UNITLEN

static void dumpCacheData(CacheData *cachedata)
{
	int i;

	printf("=========================\n");
	for (i = 0; i < cachedata->IndexCount; i++) {
		if (cachedata->Index[i] != (unsigned int)(-1))
			printf("--[%d]--%d--\n", i, cachedata->Index[i]);
	}
	printf("=========================\n");
}

static int Handle(int argc, char *argv[]){
	int i;
	unsigned int pageid;
	unsigned int sectorid;
	CacheData *cachedata;

	cachedata = CacheData_Init(INDEXCOUNT, UNITLEN);
	if (!cachedata) {
		printf("CacheData_Init failed!\n");
		return -1;
	}
	printf("CacheData_Init OK!\n");
	dumpCacheData(cachedata);
	cachedata->IndexID = 0;
	for (i = 0; i < INDEXCOUNT; i++) {
		CacheData_set(cachedata, i, i);
		dumpCacheData(cachedata);
	}
	printf("CacheData_set %d OK!\n\n", INDEXCOUNT);

	for (i = 0; i < INDEXCOUNT; i++) {
		pageid = CacheData_get(cachedata, i);
		if (pageid == -1)
			printf("Cache miss!\n");
		else
			printf("get pageid = %d,sectorid = %d\n", pageid, i);
	}
	dumpCacheData(cachedata);

	for (i = 0; i < INDEXCOUNT; i++) {
		sectorid = CacheData_find(cachedata, i);
		if (sectorid == (unsigned int)(-1))
			printf("CacheData_find failed!\n");
		else
			printf("find sectorid = %d, pageid = %d\n", sectorid, i);
	}
	dumpCacheData(cachedata);

	CacheData_DeInit(cachedata);
	printf("CacheData_Deinit OK!\n");
	dumpCacheData(cachedata);

	printf("============CacheData test OK!============\n");
	
	return 0;
}



static char *mycommand="cd";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
