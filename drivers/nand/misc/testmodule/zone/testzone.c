/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2007 by dsqiu (dsqiu@ingenic.cn)
 */

#include "testfunc.h"
#include "vnandinfo.h"
#include "nandinterface.h"
#include "pagelist.h"
#include "context.h"
#include "zone.h"
#include "vNand.h"
#include "pageinfo.h"
#include "string.h"


static void test_zone_init(int context,Zone *zon,SigZoneInfo *prev,SigZoneInfo *next)
{
	Zone_Init( zon,prev,next);

	printf("zone badblock %d \n",zon->badblock);
	printf("zone ZoneID %d \n",zon->ZoneID);
	printf("zone pageCursor %d \n",zon->pageCursor);
	printf("zone allocPageCursor %d \n",zon->allocPageCursor);
	printf("zone valid page %d \n",zon->validpage);
	printf("prevzone address %p \n",zon->prevzone);
	printf("nextzone address %p \n",zon->nextzone);
	printf("sigzoneinfo address %p \n",zon->sigzoneinfo);
}

static void test_skip_badblock_alloc(Zone *zone)
{
	int i = 0;
	int cursor = 0;

	for(i = 0 ; i < 600 ; i++)
	{
		 cursor = Zone_AllocNextPage(zone);
	
	     printf("zone alloc cursor %d \n",cursor);
	}
	 
}

extern Context context;
int start_test_nand(int argc, char *argv[]){

	Context *conptr = &context;
	unsigned char *ptr = NULL;
	
	printf("func %s line %d \n",__FUNCTION__,__LINE__);
	int i = 0;
	int j = 0;
	int ret = -1;
	int pageperzone = conptr->vnand.PagePerBlock * 8;
	ZoneManager *zonep = Nand_ContinueAlloc(sizeof(ZoneManager));
	if(zonep == NULL)
	{
		printf("alloc zonemanager fail func %s line %d \n",
				__FUNCTION__,__LINE__);
		return -1;
	}
	
	conptr->blm = (BuffListManager *)BuffListManager_BuffList_Init();

	conptr->l1info = malloc(conptr->vnand.BytePerPage);

	conptr->top = (SigZoneInfo *)malloc(512 * sizeof(SigZoneInfo));

	Zone zone;
	printf("func %s line %d \n",__FUNCTION__,__LINE__);

	zone.sigzoneinfo = conptr->top;
	zone.sigzoneinfo->badblock = 0;
	zone.vnand = &conptr->vnand;
	zone.context = (int)conptr;
	zone.badblock = 0x01;
	zone.mem0 = malloc(2048);
	//zone.context = conptr;
	zone.top = conptr->top;
//	zone.top = (SigZoneInfo *)malloc(512 * sizeof(SigZoneInfo));

/********************test zone init********************************************/
	// test_zone_init(&context,&zone,conptr->top +10,conptr->top+30);
	printf("func %s line %d \n",__FUNCTION__,__LINE__);
	Zone_Init(&zone,conptr->top + 10,conptr->top +30);
/******************************************************************************/

/************************test skip badblock **********************************/
	//test_skip_badblock_alloc(&zone);
	
/*********************************************************************************/

	conptr->zonep = zonep;
	zonep->context = (int)&context;
	zonep->vnand = &conptr->vnand;

	zonep->zonevalidinfo.zoneid = -1;
	zonep->zonevalidinfo.current_count = -1;
	zonep->zonevalidinfo.wpages = (Wpages *)Nand_ContinueAlloc(sizeof(Wpages) * pageperzone);
	if (zonep->zonevalidinfo.wpages == NULL)
	{
		printf("alloc zonemanager fail func %s line %d \n",
				__FUNCTION__,__LINE__);
		return -1;

	}

	//ZoneManager_Init((int) conptr);
/*********************test zone write *****************************************************/
	printf("func %s line %d \n",__FUNCTION__,__LINE__);

	PageInfo pageinfo;
	pageinfo.L1Index = 10;
	pageinfo.L2InfoLen = 200;
	pageinfo.L2Index = 20;
	pageinfo.L3InfoLen = 300;
	pageinfo.L3Index = 30;
	pageinfo.L2Info = malloc(pageinfo.L2InfoLen);
	//pageinfo.L2Info = malloc(3000);
	pageinfo.L3Info = malloc(pageinfo.L3InfoLen);
	//pageinfo.L3Info = malloc(3000);
	pageinfo.L1Info = (unsigned char*)conptr->l1info;
	pageinfo.L4InfoLen = 1024;
	pageinfo.L4Info = malloc(pageinfo.L4InfoLen);

	memset(conptr->l1info , 'a',2048);
	memset(pageinfo.L3Info,'b',pageinfo.L3InfoLen);
	memset(pageinfo.L2Info,'c',pageinfo.L2InfoLen);
	memset(pageinfo.L4Info,'x',pageinfo.L4InfoLen);

	zone.L2InfoLen = pageinfo.L2InfoLen;
	zone.L3InfoLen = pageinfo.L3InfoLen;
	zone.L4InfoLen = pageinfo.L4InfoLen;
	
	ptr = malloc(2048);

	memset(ptr,'d',2048);

	int zoneid = Zone_AllocNextPage(&zone);
	PageList *ppageptr = (PageList *)BuffListManager_getTopNode((int)(conptr->blm), sizeof(PageList));
		ppageptr->pData = ptr;
		ppageptr->startPageID = Zone_AllocNextPage(&zone);
		ppageptr->retVal = 0;
		ppageptr->Bytes = 2048;
		ppageptr->OffsetBytes = 0;

	PageList *px = NULL;
	for(i = 0 ; i < 9; i++)
	{
		px = (PageList *)BuffListManager_getNextNode((int)(conptr->blm), (void *)ppageptr, sizeof(PageList));
		px->pData = ptr;
		px->startPageID = Zone_AllocNextPage(&zone);
		px->retVal = 0;
		px->Bytes = 2048;
		px->OffsetBytes = 0;
	}
	
	pageinfo.PageID = zoneid;

	zone.currentLsector = px->startPageID * zone.vnand->BytePerPage / SECTOR_SIZE; 
	ret = Zone_MultiWritePage(&zone,10,ppageptr,&pageinfo);
	//ret = Zone_MultiWritePage(&zone,10,ppageptr,&pageinfo);

	BuffListManager_freeAllList((int)(conptr->blm), (void **)&ppageptr, sizeof(PageList));

	memset(pageinfo.L3Info,'F',pageinfo.L3InfoLen);
	memset(pageinfo.L2Info,'E',pageinfo.L2InfoLen);
	memset(pageinfo.L4Info,'G',pageinfo.L4InfoLen);
	
	pageinfo.PageID = Zone_AllocNextPage(&zone);
		ppageptr = (PageList *)BuffListManager_getTopNode((int)(conptr->blm), sizeof(PageList));
		ppageptr->pData = ptr;
		ppageptr->startPageID = Zone_AllocNextPage(&zone);
		ppageptr->retVal = 0;
		ppageptr->Bytes = 2048;
		ppageptr->OffsetBytes = 0;

	printf("func %s line %d pageID %d \n",__FUNCTION__,__LINE__,pageinfo.PageID);
	for(i = 0 ; i < 10; i++)
	{
		px = (PageList *)BuffListManager_getNextNode((int)(conptr->blm), (void *)ppageptr, sizeof(PageList));
		printf("px %p  \n",px);
		px->pData = ptr;
		px->startPageID = Zone_AllocNextPage(&zone);
		px->retVal = 0;
		px->Bytes = 2048;
		px->OffsetBytes = 0;
	}
	
	zone.currentLsector = px->startPageID * zone.vnand->BytePerPage / SECTOR_SIZE; 
	ret = Zone_MultiWritePage(&zone,10,ppageptr,&pageinfo);
	//ret = Zone_MultiWritePage(&zone,10,ppageptr,&pageinfo);
 
	BuffListManager_freeAllList((int)(conptr->blm), (void **)&ppageptr, sizeof(PageList));

	memset(pageinfo.L3Info,'Y',pageinfo.L3InfoLen);
	memset(pageinfo.L2Info,'J',pageinfo.L2InfoLen);
	memset(pageinfo.L4Info,'Z',pageinfo.L4InfoLen);
	
	pageinfo.PageID = Zone_AllocNextPage(&zone);
		ppageptr = (PageList *)BuffListManager_getTopNode((int)(conptr->blm), sizeof(PageList));
		ppageptr->pData = ptr;
		ppageptr->startPageID = Zone_AllocNextPage(&zone);
		ppageptr->retVal = 0;
		ppageptr->Bytes = 2048;
		ppageptr->OffsetBytes = 0;
	printf("func %s line %d pageID %d \n",__FUNCTION__,__LINE__,pageinfo.PageID);
	for(i = 0 ; i < 10; i++)
	{
		px = (PageList *)BuffListManager_getNextNode((int)(conptr->blm), (void *)ppageptr, sizeof(PageList));
		px->pData = ptr;
		px->startPageID = Zone_AllocNextPage(&zone);
		printf("px startPageid %d \n",px->startPageID);
		px->retVal = 0;
		px->Bytes = 2048;
		px->OffsetBytes = 0;
	}
	
	zone.currentLsector = px->startPageID * zone.vnand->BytePerPage / SECTOR_SIZE; 
	ret = Zone_MultiWritePage(&zone,10,ppageptr,&pageinfo);
	//ret = Zone_MultiWritePage(&zone,10,ppageptr,&pageinfo);

	
	printf("func %s line %d \n",__FUNCTION__,__LINE__);
	memset(pageinfo.L3Info,0x0,pageinfo.L3InfoLen);
	memset(pageinfo.L2Info,0x0,pageinfo.L2InfoLen);
	memset(pageinfo.L4Info,0x0,pageinfo.L4InfoLen);

	printf("func %s line %d \n",__FUNCTION__,__LINE__);
	ret = Zone_FindFirstPageInfo(&zone,&pageinfo);

	printf("func %s line %d \n",__FUNCTION__,__LINE__);
	printf("first pageinfo.L3Info %s \n",pageinfo.L3Info);
	printf("first pageinfo.L2Info %s \n",pageinfo.L2Info);
	printf("first pageinfo.L4Info %s \n",pageinfo.L4Info);

	memset(pageinfo.L3Info,0x0,pageinfo.L3InfoLen);
	memset(pageinfo.L2Info,0x0,pageinfo.L2InfoLen);
	memset(pageinfo.L4Info,0x0,pageinfo.L4InfoLen);

	ret = Zone_FindNextPageInfo(&zone,&pageinfo);
	printf("second pageinfo.L3Info %s \n",pageinfo.L3Info);
	printf("second pageinfo.L2Info %s \n",pageinfo.L2Info);
	printf("second pageinfo.L4Info %s \n",pageinfo.L4Info);
	memset(pageinfo.L3Info,0x0,pageinfo.L3InfoLen);
	memset(pageinfo.L2Info,0x0,pageinfo.L2InfoLen);
	memset(pageinfo.L4Info,0x0,pageinfo.L4InfoLen);
	ret = Zone_FindNextPageInfo(&zone,&pageinfo);
	printf("three pageinfo.L3Info %s \n",pageinfo.L3Info);
	printf("three pageinfo.L2Info %s \n",pageinfo.L2Info);
	printf("three pageinfo.L4Info %s \n",pageinfo.L4Info);

	memset(pageinfo.L3Info,0x0,pageinfo.L3InfoLen);
	memset(pageinfo.L2Info,0x0,pageinfo.L2InfoLen);
	memset(pageinfo.L4Info,0x0,pageinfo.L4InfoLen);

	//ret = Zone_FindNextPageInfo(&zone,&pageinfo);
	printf("  ret %d  zone->validpage %d \n",ret,zone.validpage);

/*****************************************************************************/
	free(ptr);
	free(conptr->l1info);
	free(pageinfo.L2Info);
	free(pageinfo.L3Info);
	free(pageinfo.L4Info);
	free(conptr->top);
	
	Zone_DeInit(&zone);
	BuffListManager_BuffList_DeInit((int)(conptr->blm));
	
	return 0;

}

static int Handle(int argc, char *argv[]){

	int ret = InitNandTest(argc,argv);
	DeinitNandTest();
	return ret;
}


static char *mycommand="zone";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
