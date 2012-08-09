#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "testfunc.h"
#include "bufflistmanager.h"
#include "context.h"

static void dumpSectorList(SectorList *h)
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
		printf("--startSector = %d, sectorCount = %d--\n", top->startSector, top->sectorCount);
	}

	printf("======================================\n\n");
}

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
		printf("--startPageID = %d, OffsetBytes = %d, Bytes = %d--\n", top->startPageID, top->OffsetBytes, top->Bytes);
	}

	printf("======================================\n\n");
}

static void dumpBlockList(BlockList *h)
{
	struct singlelist *pos;
	BlockList *top;

	if (!h) {
		printf("no content!\n\n");
		return;
	}

	printf("======================================\n");

	singlelist_for_each (pos, &(h->head)) {
		top = singlelist_entry(pos, BlockList, head);
		printf("--startBlock = %d, BlockCount = %d--\n", top->startBlock, top->BlockCount);
	}

	printf("======================================\n\n");
}

static void dumpLPartitionList(LPartitionList *h)
{
	struct singlelist *pos;
	LPartitionList *top;

	if (!h) {
		printf("no content!\n\n");
		return;
	}

	printf("======================================\n");

	singlelist_for_each (pos, &(h->head)) {
		top = singlelist_entry(pos, LPartitionList, head);
		printf("--startSector = %d, sectorCount = %d, name:%s, mode:%d--\n", top->startSector, top->sectorCount, top->Name, top->mode);
	}

	printf("======================================\n\n");
}
/*
static void dumpPPartition(PPartition *h)
{
	struct singlelist *pos;
	PPartition *top;

	if (!h) {
		printf("no content!\n\n");
		return;
	}

	printf("======================================\n");

	singlelist_for_each (pos, &(h->head)) {
		top = singlelist_entry(pos, PPartition, head);
		printf("--startPage = %d, PageCount = %d, name:%s, mode:%d--\n", top->startPage, top->PageCount, top->name, top->mode);
	}

	printf("======================================\n\n");
}
*/
static void SectorList_Test()
{
	int i;
	int handle;
	SectorList *sl_top1, *sl_next1[9];
	SectorList *sl_top2, *sl_next2;

	handle = BuffListManager_BuffList_Init();
	printf("BuffList_init OK in SectorList_Test----->start\n");
	
	sl_top1 = (SectorList *)BuffListManager_getTopNode(handle,sizeof(SectorList));
	sl_top1->startSector = 1;
	sl_top1->sectorCount = 1;

	sl_top2 = (SectorList *)BuffListManager_getTopNode(handle,sizeof(SectorList));
	sl_top2->startSector = 1;
	sl_top2->sectorCount = 1;

	printf("add 10 node\n");
	for (i = 0; i < 9; i++) {
		sl_next1[i] = (SectorList *)BuffListManager_getNextNode(handle, (void *)sl_top1,sizeof(SectorList));
		sl_next1[i]->startSector = i + 2;
		sl_next1[i]->sectorCount = i + 2;
	}
	dumpSectorList(sl_top1);

	for (i = 0; i < 9; i++) {
		sl_next2 = (SectorList *)BuffListManager_getNextNode(handle, (void *)sl_top2,sizeof(SectorList));
		sl_next2->startSector = i + 2;
		sl_next2->sectorCount = i + 2;
	}

	printf("delete 4 node\n");
	for (i = 8; i > 4; i--)
		BuffListManager_freeList(handle, (void **)(&sl_top1), (void *)sl_next1[i],sizeof(SectorList));
	dumpSectorList(sl_top1);
	
	printf("***BuffListManager_mergerList***\n");
	BuffListManager_mergerList(handle, (void *)sl_top1, (void *)sl_top2);
	dumpSectorList(sl_top1);

	BuffListManager_freeAllList(handle, (void **)&sl_top1,sizeof(SectorList));
	printf("***BuffListManager_freeAllList***\n");
	dumpSectorList(sl_top1);

	BuffListManager_BuffList_DeInit(handle);
	printf("BuffList_Deinit OK in SectorList_Test----->end\n\n");
}

static void PageList_Test()
{
	int i;
	int handle;
	PageList *pl_top1, *pl_next1[9];
	PageList *pl_top2, *pl_next2;

	handle = BuffListManager_BuffList_Init();
	printf("BuffList_init OK in PageList_Test----->start\n");
	
	pl_top1 = (PageList *)BuffListManager_getTopNode(handle,sizeof(PageList));
	pl_top1->startPageID = 1;
	pl_top1->OffsetBytes = 0;
	pl_top1->Bytes = 512;

	pl_top2 = (PageList *)BuffListManager_getTopNode(handle,sizeof(PageList));
	pl_top2->startPageID = 1;
	pl_top2->OffsetBytes = 0;
	pl_top2->Bytes = 512;

	printf("add 10 node\n");
	for (i = 0; i < 9; i++) {
		pl_next1[i] = (PageList *)BuffListManager_getNextNode(handle, (void *)pl_top1,sizeof(PageList));
		pl_next1[i]->startPageID = i + 2;
		pl_next1[i]->OffsetBytes = 0;
		pl_next1[i]->Bytes = 512;
	}
	dumpPageList(pl_top1);

	for (i = 0; i < 9; i++) {
		pl_next2 = (PageList *)BuffListManager_getNextNode(handle, (void *)pl_top2,sizeof(PageList));
		pl_next2->startPageID = i + 2;
		pl_next2->OffsetBytes = 0;
		pl_next2->Bytes = 512;
	}

	printf("delete 4 node\n");
	for (i = 8; i > 4; i--)
		BuffListManager_freeList(handle, (void **)(&pl_top1), (void *)pl_next1[i],sizeof(PageList));
	dumpPageList(pl_top1);
	
	printf("***BuffListManager_mergerList***\n");
	BuffListManager_mergerList(handle, (void *)pl_top1, (void *)pl_top2);
	dumpPageList(pl_top1);

	BuffListManager_freeAllList(handle, (void **)&pl_top1,sizeof(PageList));
	printf("***BuffListManager_freeAllList***\n");
	dumpPageList(pl_top1);

	BuffListManager_BuffList_DeInit(handle);
	printf("BuffList_Deinit OK in PageList_Test----->end\n\n");
}

static void BlockList_Test()
{
	int i;
	int handle;
	BlockList *bl_top1, *bl_next1[9];
	BlockList *bl_top2, *bl_next2;

	handle = BuffListManager_BuffList_Init();
	printf("BuffList_init OK in BlockList_Test----->start\n");
	
	bl_top1 = (BlockList *)BuffListManager_getTopNode(handle,sizeof(BlockList));
	bl_top1->startBlock = 1;
	bl_top1->BlockCount = 1;

	bl_top2 = (BlockList *)BuffListManager_getTopNode(handle,sizeof(BlockList));
	bl_top2->startBlock = 1;
	bl_top2->BlockCount = 1;

	printf("add 10 node\n");
	for (i = 0; i < 9; i++) {
		bl_next1[i] = (BlockList *)BuffListManager_getNextNode(handle, (void *)bl_top1,sizeof(BlockList));
		bl_next1[i]->startBlock = i + 2;
		bl_next1[i]->BlockCount = i + 2;
	}
	dumpBlockList(bl_top1);

	for (i = 0; i < 9; i++) {
		bl_next2 = (BlockList *)BuffListManager_getNextNode(handle, (void *)bl_top2,sizeof(BlockList));
		bl_next2->startBlock = i + 2;
		bl_next2->BlockCount = i + 2;
	}

	printf("delete 4 node\n");
	for (i = 8; i > 4; i--)
		BuffListManager_freeList(handle, (void **)(&bl_top1), (void *)bl_next1[i],sizeof(BlockList));
	dumpBlockList(bl_top1);
	
	printf("***BuffListManager_mergerList***\n");
	BuffListManager_mergerList(handle, (void *)bl_top1, (void *)bl_top2);
	dumpBlockList(bl_top1);

	BuffListManager_freeAllList(handle, (void **)&bl_top1,sizeof(BlockList));
	printf("***BuffListManager_freeAllList***\n");
	dumpBlockList(bl_top1);

	BuffListManager_BuffList_DeInit(handle);
	printf("BuffList_Deinit OK in BlockList_Test----->end\n\n");
}

static void LPartitionList_Test()
{
	int i;
	int handle;
	LPartitionList *ll_top1, *ll_next1[9];
	LPartitionList *ll_top2, *ll_next2;

	handle = BuffListManager_BuffList_Init();
	printf("BuffList_init OK in LPartitionList_Test----->start\n");
	
	ll_top1 = (LPartitionList *)BuffListManager_getTopNode(handle,sizeof(LPartitionList));
	ll_top1->startSector = 1;
	ll_top1->sectorCount = 1;
	ll_top1->Name = "ll1";
	ll_top1->mode = 0;

	ll_top2 = (LPartitionList *)BuffListManager_getTopNode(handle,sizeof(LPartitionList));
	ll_top2->startSector = 1;
	ll_top2->sectorCount = 1;
	ll_top2->Name = "ll2";
	ll_top2->mode = 1;

	printf("add 10 node\n");
	for (i = 0; i < 9; i++) {
		ll_next1[i] = (LPartitionList *)BuffListManager_getNextNode(handle, (void *)ll_top1,sizeof(LPartitionList));
		ll_next1[i]->startSector = i + 2;
		ll_next1[i]->sectorCount = i + 2;
		ll_next1[i]->Name = "ll1";
		ll_next1[i]->mode = 0;
	}
	dumpLPartitionList(ll_top1);

	for (i = 0; i < 9; i++) {
		ll_next2 = (LPartitionList *)BuffListManager_getNextNode(handle, (void *)ll_top2,sizeof(LPartitionList));
		ll_next2->startSector = i + 2;
		ll_next2->sectorCount = i + 2;
		ll_next2->Name = "ll2";
		ll_next2->mode = 1;
	}

	printf("delete 4 node\n");
	for (i = 8; i > 4; i--)
		BuffListManager_freeList(handle, (void **)(&ll_top1), (void *)ll_next1[i],sizeof(LPartitionList));
	dumpLPartitionList(ll_top1);
	
	printf("***BuffListManager_mergerList***\n");
	BuffListManager_mergerList(handle, (void *)ll_top1, (void *)ll_top2);
	dumpLPartitionList(ll_top1);

	BuffListManager_freeAllList(handle, (void **)&ll_top1,sizeof(LPartitionList));
	printf("***BuffListManager_freeAllList***\n");
	dumpLPartitionList(ll_top1);

	BuffListManager_BuffList_DeInit(handle);
	printf("BuffList_Deinit OK in LPartitionList_Test----->end\n\n");
}

static int Handle(int argc, char *argv[])
{
	SectorList_Test();
	sleep(1);
#if 1
	PageList_Test();
	sleep(1);

	BlockList_Test();
	sleep(1);

	LPartitionList_Test();
	sleep(1);

//	PPartition_Test();
#endif
	return 0;
}


static char *mycommand="blm";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
