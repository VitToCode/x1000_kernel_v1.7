#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "testfunc.h"
#include "cachemanager.h"
#include "vnandinfo.h"
#include "nandinterface.h"
#include "pagelist.h"
#include "context.h"
#include "zone.h"
#include "vNand.h"
#include "pageinfo.h"
#include "l1info.h"
#include "nanddebug.h"
#include "nandsigzoneinfo.h"

#define L1_CACHEDATA_COUNT 1
#define L2_CACHEDATA_COUNT 1
#define L3_CACHEDATA_COUNT 16
#define L4_CACHEDATA_COUNT 16

#define BLOCKPERZONE(context)   	8

//#define TEST_FREE_USED

extern NandInterface em_nand_ops;
extern int InitNandTest(int argc, char *argv[]);
extern void DeinitNandTest();
extern int vNand_ScanBadBlocks (VNandManager* vnand);

static void dumpCacheData(CacheData *cachedata)
{
	int i;

	printf("=========================\n");
	printf("indexcount = %d\n", cachedata->IndexCount);
	for (i = 0; i < cachedata->IndexCount; i++) {
		if (cachedata->Index[i] != (unsigned int)(-1))
			printf("--[%d]--%d--\n", i, cachedata->Index[i]);
	}
	printf("=========================\n");
}

static void dumpCacheList(CacheList *cachelist)
{
	struct singlelist *pos;
	CacheData *cachedata;
	
	if (cachelist->listCount == 0) {
		printf("no content!\n");
		return;
	}

	printf("top.index = %d, tail.index = %d, count = %d\n", cachelist->top->IndexID, cachelist->tail->IndexID, cachelist->listCount);
	printf("======================================\n");
	singlelist_for_each(pos,&(cachelist->top->head)) {
		cachedata = singlelist_entry(pos,CacheData,head);
		if (cachedata->IndexID != -1) {
			printf("--indexID = %d -- index[0] = %d --\n", cachedata->IndexID, *(cachedata->Index));
			dumpCacheData(cachedata);
		}
	}
	printf("======================================\n\n");
}

static void dumpCacheManager(CacheManager *cachemanager)
{
	//printf("L1Cache:\n");
	//dumpCacheData(cachemanager->L1Info);
	
	if (cachemanager->L2InfoLen) {
		printf("L2Cache:\n");
		dumpCacheList(cachemanager->L2Info);
	}	

	if (cachemanager->L3InfoLen) {
		printf("L3Cache:\n");
		dumpCacheList(cachemanager->L3Info);
	}
	
	printf("L4Cache:\n");
	dumpCacheList(cachemanager->L4Info);
}

static void init_nand_flash(VNandInfo *vnand)
{
	int i = 0;
	int handle;
	unsigned int zonenum = vnand->TotalBlocks?(vnand->TotalBlocks - 1) / BLOCKPERZONE(vnand) + 1:vnand->TotalBlocks;
	PageList *pl = NULL;

	handle = BuffListManager_BuffList_Init();

	pl = (PageList *)BuffListManager_getTopNode(handle, sizeof(PageList));

	unsigned char *buf = malloc(vnand->BytePerPage);	
	memset(buf,0xff,vnand->BytePerPage);
	NandSigZoneInfo *nandsig = (NandSigZoneInfo *)buf;

	for(i = 0; i < zonenum; i++)
	{		
		nandsig->ZoneID = i;
		nandsig->lifetime = random() % 300;
		nandsig->badblock = 0;

		pl->startPageID = (i * BLOCKPERZONE(vnand) + vnand->startBlockID) * vnand->PagePerBlock;
		pl->OffsetBytes = 0;
		pl->Bytes = vnand->BytePerPage;
		pl->pData = buf;
		pl->retVal = 0;

		vNand_MultiPageWrite(vnand,pl);
	}

	free(buf);
	BuffListManager_freeAllList(handle, (void **)&pl, sizeof(PageList));
	BuffListManager_BuffList_DeInit(handle);
}

extern Context context;
int start_test_nand(int argc, char *argv[]){
	int i, ret;
	unsigned int sectorid, pageid;
	CacheManager *cachemanager;
	PageInfo *pi;
	Context *conptr = &context;

	conptr->blm = (BuffListManager *)BuffListManager_BuffList_Init();

	init_nand_flash(&conptr->vnand);

	ZoneManager_Init((int)conptr);

	/**********************************/
	ret = CacheManager_Init((int)conptr);
	if (ret == -1) {
		printf("CacheManager_Init failed!\n");
		return -1;
	}
	printf("CacheManager_Init OK!\n");

	cachemanager = conptr->cachemanager;
	printf("L1UnitLen = %d, L2UnitLen = %d, L3UnitLen = %d, L4UnitLen = %d\n", cachemanager->L1UnitLen, cachemanager->L2UnitLen, cachemanager->L3UnitLen, cachemanager->L4UnitLen);
	printf("L1InfoLen = %d, L2InfoLen = %d, L3InfoLen = %d, L4InfoLen = %d\n", cachemanager->L1InfoLen, cachemanager->L2InfoLen, cachemanager->L3InfoLen, cachemanager->L4InfoLen);
	cachemanager->L1Info->IndexID = 0;
	for (i = 0; i < cachemanager->L1InfoLen / UNIT_SIZE; i++)
		CacheData_set(cachemanager->L1Info, cachemanager->L1UnitLen * i, cachemanager->L1UnitLen / (conptr->vnand.BytePerPage / SECTOR_SIZE) * i);
	cachemanager->L4Info->top->IndexID = 0;
	for (i = 0; i < cachemanager->L4InfoLen / UNIT_SIZE; i++){
		CacheData_set(cachemanager->L4Info->top, i, i + 1);
	}
	dumpCacheManager(cachemanager);

	for (i = 0; i < 10; i++) {
		//sectorid = cachemanager->L1InfoLen * i + 10;
		sectorid = i * 10;
		//sectorid = i;
		pageid = CacheManager_getPageID((int)cachemanager, sectorid);
		printf("getPageID:pageid = %d, sector = %d\n", pageid, sectorid);
		//dumpCacheManager(cachemanager);
	}

	for (i = 0; i < 10; i++) {
		//sectorid = cachemanager->L1InfoLen * i + 10;
		sectorid = i;
		pageid = i;

		CacheManager_lockCache((int)cachemanager, sectorid, &pi);
		printf("pi->L1Info = %p, pi->L2Info = %p, pi->L3Info = %p, pi->L4Info = %p\n", pi->L1Info, pi->L2Info, pi->L3Info, pi->L4Info);
		
		CacheManager_unlockCache((int)cachemanager, pi);

		dumpCacheManager(cachemanager);
	}
	printf("lock unlock Test finish!\n\n");

	return 0;
}

static int Handle(int argc, char *argv[]){

	int ret = InitNandTest(argc,argv);
	DeinitNandTest();
	return ret;
}



static char *mycommand="cm";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
