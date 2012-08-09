/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2007 by dsqiu (dsqiu@ingenic.cn)
 */
#include <string.h>
#include "testfunc.h"
#include "vnandinfo.h"
#include "nandinterface.h"
#include "pagelist.h"
#include "context.h"
#include "zone.h"
#include "vNand.h"
#include "pageinfo.h"
#include "l1info.h"
#include "recycle.h"
#include "l2pconvert.h"
#include "taskmanager.h"
#include "nandmanagerinterface.h"
#include "nandsigzoneinfo.h"

#define BLOCKPERZONE(context)   	8

extern PPartition ppt[];

//#define TEST_FREE_USED

extern NandInterface em_nand_ops;
extern void test_read_data_from_zone(Recycle *rep,int pagenum);
extern void test_write_data_to_zone(Recycle*rep,Zone *wzone,PageInfo *pi,int pagenum);
extern void test_all_recycle_buflen(Recycle *rep,Zone *zone,int count);
extern void test_part_recycle_buflen(Recycle *rep,Zone *zone,int count);

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
		//printf("--startSector = %d, sectorCount = %d--\n", top->startSector, top->sectorCount);
		printf("--startSector = %d, sectorCount = %d, pData: 0x%08x--\n", top->startSector, top->sectorCount, *(unsigned int *)(top->pData));
	}

	printf("======================================\n\n");
}

static void write_data_to_zone(Zone *zone, PageInfo *pi,unsigned int count)
{
	PageList *pl = NULL;
	PageList *tpl = NULL;
	unsigned int i = 0;
	unsigned char *buf = malloc(zone->vnand->BytePerPage);
	Zone *wzone = NULL;
	BuffListManager *blm = ((Context *)(zone->context))->blm;

	memset(buf,'a',zone->vnand->BytePerPage);
	pl = (PageList *)BuffListManager_getTopNode((int)blm, sizeof(PageList));
	pi->PageID = Zone_AllocNextPage(zone);

	printf("func %s line %d pi->PageID %d \n",__FUNCTION__,__LINE__,pi->PageID);

	for(i = 0 ; i < count;i++)
	{
		tpl = (PageList *)BuffListManager_getNextNode((int)blm, (void *)pl, sizeof(PageList));	
		tpl->startPageID = Zone_AllocNextPage(zone);
		tpl->Bytes = zone->vnand->BytePerPage;
		tpl->OffsetBytes = 0;
		tpl->pData = buf;
		tpl->retVal = 0;
		
	}

	Zone_MultiWritePage(zone,0,pl,pi);
	free(buf);
	BuffListManager_freeAllList((int)blm, (void **)&pl, sizeof(PageList));
}

static void test_for_getrecyclezone(Recycle *rep,int context,int force)
{
	/* test for force */
	printf("test for force rep->context %08x \n",rep->context);
	rep->force = 1;
	Recycle_getRecycleZone(rep);
	printf("rep->rZone %d \n",rep->rZone->ZoneID);
	printf("rep->taskStep %d \n",rep->taskStep);
/* 
	printf("test for normal \n");
	Recycle_getRecycleZone(rep,0,0);
	printf("rep->rZone %d \n",rep->rZone->ZoneID);
	printf("rep->taskStep %d \n",rep->taskStep);
*/	

}

static void test_for_findfirstpageinfo(Recycle *rep,PageInfo *pi)
{
	/* test for findfirstoageinfo */
	printf("test for findfirstpageinfo \n");
	Recycle_FindFirstPageInfo(rep);
	pi = rep->curpageinfo;

	printf("pi->pageID %d \n",pi->PageID);
	printf("pi->l1index %d \n",pi->L1Index);
	printf("pi->l2index %d \n",pi->L2Index);
	printf("pi->l3index %d \n",pi->L3Index);
	printf("pi->L1len %d \n",pi->L1InfoLen);
	printf("pi->l2len %d \n",pi->L2InfoLen);
	printf("pi->L3len %d \n",pi->L3InfoLen);
	printf("pi->L4Len %d \n",pi->L4InfoLen);
	
}

static void test_for_findvaildsector(Recycle *rep)
{
	/* test for findvaildsector */	
	printf("test for findvaildsector \n");
	Recycle_FindValidSector(rep);
	unsigned int i = 0;

	printf("start sector %d \n",rep->startsectorID);
	for(i = 0 ; i < 256 ;i++)
	{
		printf(" %d sector %d \n",i,rep->record_writeadd[i]);	
	}

}

static void test_for_mergersectorid(Recycle *rep)
{
	/* test for merger sectorID */
	printf("func %s line %d \n",__FUNCTION__,__LINE__);
	Recycle_MergerSectorID(rep);
	struct singlelist *sg;
	PageList *pl = NULL;
	unsigned int i = 0;

	printf("func %s line %d \n",__FUNCTION__,__LINE__);
	singlelist_for_each(sg,&rep->pagelist->head)
	{
	printf("func %s line %d \n",__FUNCTION__,__LINE__);
		i++;
		pl = singlelist_entry(sg,PageList,head);
	printf("func %s line %d %p\n",__FUNCTION__,__LINE__,pl);
		printf("%d pl->startpageId %d \n",i ,pl->startPageID);
		printf("%d pl->Bytes %d \n",i,pl->Bytes);
	}

}

static test_for_recycle_freenode(Recycle *rep)
{
	Recycle_FreeZone(rep);	
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
	Context * conptr = &context;
	unsigned char *ptr = NULL;
	Recycle recycle;
	Recycle *rep = &recycle;
	PageInfo px ;
	PageInfo *pi = &px;
	SectorList *sl_write, *sl_read, *sl_node;
	Zone *zoneptr = NULL;
	int i = 0;
	int j = 0;
	int ret = -1;
	int handle;
	BuffListManager *blm;
	PPartition *pt;
	PManager pm;
	ForceRecycleInfo frinfo;
	
#ifdef TEST_FORCERECYCLE
	init_nand_flash(&conptr->vnand);
	
	pm.bufferlist = (BuffListManager *)BuffListManager_BuffList_Init();
	L2PConvert_Init(&pm);

	handle = L2PConvert_ZMOpen(&conptr->vnand, &ppt[0]);
	blm = ((Context *)handle)->blm;

	sl_write = (SectorList *)BuffListManager_getTopNode((int)blm, sizeof(SectorList));
	sl_read = (SectorList *)BuffListManager_getTopNode((int)blm, sizeof(SectorList));
	
	for (i = 0; i < 128; i++) {
		sl_node = (SectorList *)BuffListManager_getNextNode((int)blm, (void *)sl_write, sizeof(SectorList));
		sl_node->startSector = 0;
		sl_node->sectorCount = 2008;
		sl_node->pData = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * sl_node->sectorCount * SECTOR_SIZE);
		memset(sl_node->pData, 0x31 + i % 9, sizeof(unsigned char) * sl_node->sectorCount * SECTOR_SIZE);
		
		BuffListManager_freeList((int)blm, (void **)&sl_write, (void *)sl_write, sizeof(SectorList));
		
		ret = L2PConvert_WriteSector(handle, sl_write);
		if (ret == -1) {
			printf("L2PConvert_WriteSector failed!\n");
			return -1;
		}

	}
	printf("L2PConvert_WriteSector finished!\n");

	for (i = 0; i < 1; i++) {
		sl_node = (SectorList *)BuffListManager_getNextNode((int)blm, (void *)sl_read, sizeof(SectorList));
		sl_node->startSector = 0;
		sl_node->sectorCount = 2008;
		sl_node->pData = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * sl_node->sectorCount * SECTOR_SIZE);
		memset(sl_node->pData, 0x0, sizeof(unsigned char) * sl_node->sectorCount * SECTOR_SIZE);
	}
	BuffListManager_freeList((int)blm, (void **)&sl_read, (void *)sl_read, sizeof(SectorList));	

	ret = L2PConvert_ReadSector(handle, sl_read);
	if (ret == -1) {
		printf("L2PConvert_ReadSector failed!\n");
		return -1;
	}
	printf("L2PConvert_ReadSector finished!\n");
	dumpSectorList(sl_read);
#else//ifdef TEST_FORCERECYCLE
	printf("func %s line %d \n",__FUNCTION__,__LINE__);
#if 0	
	for(i = 0 ; i < 64 * 128 * 4; i++)
	{
		L1Info_set(i,i);
		printf(" %d ,get L1info ID %d \n",i,L1Info_get(i));
	}
#endif 
	
	printf("func %s line %d \n",__FUNCTION__,__LINE__);
    	printf("recyID %d \n",ZoneManager_RecyclezoneID((int )conptr,3000));

	printf("test alloc Recycle zone \n");
	for(i = 0 ; i < 16 ; i ++)
	{
		zoneptr = ZoneManager_AllocRecyclezone((int)conptr,i);
		ZoneManager_FreeRecyclezone((int)conptr,zoneptr);
	}
	printf("test alloc zone \n");
/*  	
	for(i = 0 ; i < 16 ; i ++)
	{
		zoneptr = ZoneManager_AllocZone();
		ZoneManager_FreeZone(zoneptr);
	}
*/

	pi->L1Index = 0;
	pi->L1Info = malloc(2048);
	pi->L1InfoLen = 2048;
	pi->L2Index  = 0;
	pi->L2Info = NULL;
	pi->L2InfoLen = 0;
	pi->L3Index = 0;
	pi->L3InfoLen = 0;
	pi->L3Info = 0;
	pi->L4InfoLen = 1024;
	pi->L4Info = malloc(1024);

	Recycle_Init((int)conptr);

	//rep->rZone->vnand = conptr->vnand;
	rep = conptr->rep;
	test_for_getrecyclezone(conptr->rep,(int)conptr,0);
	
	Zone_Init(rep->rZone,NULL,NULL);
	printf("func %s line %d \n",__FUNCTION__,__LINE__);
	write_data_to_zone(rep->rZone, pi,20);
	printf("func %s line %d \n",__FUNCTION__,__LINE__);
	test_for_findfirstpageinfo(rep,pi);
	printf("func %s line %d \n",__FUNCTION__,__LINE__);
	test_for_findvaildsector(rep);
	printf("func %s line %d \n",__FUNCTION__,__LINE__);
	test_for_mergersectorid(rep);
	printf("func %s line %d \n",__FUNCTION__,__LINE__);

	printf("func %s line %d \n",__FUNCTION__,__LINE__);
	test_for_recycle_freenode(rep);	
	printf("func %s line %d \n",__FUNCTION__,__LINE__);

	Recycle_DeInit((int)conptr);
	ZoneManager_DeInit((int)conptr);
#endif
	return 0;

}


static int Handle(int argc, char *argv[]){

	int ret = InitNandTest(argc,argv);
	DeinitNandTest();
	return ret;
}



static char *mycommand="rec";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
