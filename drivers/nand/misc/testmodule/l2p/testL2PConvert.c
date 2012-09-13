#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "testfunc.h"
#include "l2pconvert.h"
#include "vnandinfo.h"
#include "nandinterface.h"
#include "pagelist.h"
#include "context.h"
#include "zone.h"
#include "vNand.h"
#include "pageinfo.h"
#include "l1info.h"
#include "zonemanager.h"
#include "taskmanager.h"
#include "recycle.h"
#include "nandmanagerinterface.h"
#include "nandsigzoneinfo.h"

extern PPartition ppt[];

#define BLOCKPERZONE(context)   	8

extern NandInterface em_nand_ops;
extern int InitNandTest(int argc, char *argv[]);
extern void DeinitNandTest();

void dumpSectorList(SectorList *h)
{
	struct singlelist *pos;
	SectorList *top;

	if (!h) {
		printf("no content!\n\n");
		return;
	}

	printf("======================================\n");

	singlelist_for_each (pos, &(h->head)) {
		top = singlelist_entry(pos, SectorList, head);
		//printf("--startSector = %d, sectorCount = %d--\n", top->startSector, top->sectorCount);
		printf("--startSector = %d, sectorCount = %d, pData: %x--\n", top->startSector, top->sectorCount, *(unsigned int *)(top->pData));
		//printf("--startSector = %d, sectorCount = %d, pData: %s--\n", top->startSector, top->sectorCount, (char *)top->pData);
	}

	printf("======================================\n\n");
}

#if 0
static void dumpCacheData(CacheData *cachedata)
{
	int i;

	printf("cachedata: %p IndexID = %d =========================\n", cachedata, cachedata->IndexID);
	if (cachedata->IndexID == 16640) {
	for (i = 0; i < cachedata->IndexCount; i++) {
		if (cachedata->Index[i] != (unsigned int)(-1))
			printf("--[%d]--%d--\n", i, cachedata->Index[i]);
	}
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
		dumpCacheData(cachedata);
	}
	printf("======================================\n\n");
}
#endif

#if 0
static void dumpPageList(PageList *h)
{
	struct singlelist *pos;
	PageList *top;

	if (!h) {
		printf("no content!\n\n");
		return;
	}

	printf("======================================\n\n");

	singlelist_for_each (pos, &(h->head)) {
		top = singlelist_entry(pos, PageList, head);
		printf("--startPageID = %d, OffsetBytes = %d, Bytes = %d, pData: %s--\n", top->startPageID, top->OffsetBytes, top->Bytes, top->pData);
	}

	printf("======================================\n\n");
}
#endif
#if 0
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
#endif
extern Context context;
int start_test_nand(int argc, char *argv[]){
	int i, ret;
	CacheManager *cachemanager;
	Context *conptr = &context;
	SectorList *sl_write, *sl_read, *sl_node;
	int handle;
	BuffListManager *blm;
	PManager pm;

	pm.bufferlist = (BuffListManager *)BuffListManager_BuffList_Init();
	L2PConvert_Init(&pm);

	handle = L2PConvert_ZMOpen(&conptr->vnand, &ppt[0]);
	blm = ((Context *)handle)->blm;

	cachemanager = ((Context *)handle)->cachemanager;
	printf("L1UnitLen = %d, L2UnitLen = %d, L3UnitLen = %d, L4UnitLen = %d\n", cachemanager->L1UnitLen, cachemanager->L2UnitLen, cachemanager->L3UnitLen, cachemanager->L4UnitLen);
	printf("L1InfoLen = %d, L2InfoLen = %d, L3InfoLen = %d, L4InfoLen = %d\n", cachemanager->L1InfoLen, cachemanager->L2InfoLen, cachemanager->L3InfoLen, cachemanager->L4InfoLen);

	sl_write = (SectorList *)BuffListManager_getTopNode((int)blm, sizeof(SectorList));
	for (i = 0; i < 1; i++) {
		sl_node = (SectorList *)BuffListManager_getNextNode((int)blm, (void *)sl_write, sizeof(SectorList));
		sl_node->startSector = i;
		sl_node->sectorCount = 2008*5;
		sl_node->pData = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * sl_node->sectorCount * SECTOR_SIZE);
		memset(sl_node->pData, 0x31 + i % 9, sizeof(unsigned char) * sl_node->sectorCount * SECTOR_SIZE);
	}
	BuffListManager_freeList((int)blm, (void **)&sl_write, (void *)sl_write, sizeof(SectorList));

	sl_read = (SectorList *)BuffListManager_getTopNode((int)blm, sizeof(SectorList));
	for (i = 0; i < 1; i++) {
		sl_node = (SectorList *)BuffListManager_getNextNode((int)blm, (void *)sl_read, sizeof(SectorList));
		sl_node->startSector = i;
		sl_node->sectorCount = 2008*5;
		sl_node->pData = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * sl_node->sectorCount * SECTOR_SIZE);
		memset(sl_node->pData, 0x0, sizeof(unsigned char) * sl_node->sectorCount * SECTOR_SIZE);
	}
	BuffListManager_freeList((int)blm, (void **)&sl_read, (void *)sl_read, sizeof(SectorList));

	ret = L2PConvert_WriteSector(handle, sl_write);
	if (ret == -1) {
		printf("L2PConvert_WriteSector failed!\n");
		return -1;
	}
	printf("L2PConvert_WriteSector finished!\n");

#if 1
	printf("before read:\n");
	dumpSectorList(sl_read);
	ret = L2PConvert_ReadSector(handle, sl_read);
	if (ret == -1) {
		printf("L2PConvert_ReadSector failed!\n");
		return -1;
	}
	printf("L2PConvert_ReadSector finished!\n");
	dumpSectorList(sl_read);
#endif

	return 0;
}


static int Handle(int argc, char *argv[]){

	int ret = InitNandTest(argc,argv);
	DeinitNandTest();
	return ret;
}


static char *mycommand="l2p";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
