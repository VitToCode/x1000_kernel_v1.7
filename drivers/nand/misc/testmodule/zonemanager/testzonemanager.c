/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2007 by dsqiu (dsqiu@ingenic.cn)
 */


#include <string.h>
#include <nanddebug.h>
#include "testfunc.h"
#include "vnandinfo.h"
#include "nandinterface.h"
#include "pagelist.h"
#include "context.h"
#include "zone.h"
#include "vNand.h"
#include "pageinfo.h"
#include "l1info.h"
#include "nandsigzoneinfo.h"
#include "nandzoneinfo.h"

#define BLOCKPERZONE(context)   	8

//#define TEST_FREE_USED

extern NandInterface em_nand_ops;

static void init_nand_flash(Context *conptr)
{
	int i = 0;
	int handle;
	unsigned int zonenum = conptr->vnand.TotalBlocks / BLOCKPERZONE(conptr->vnand);

	handle = BuffListManager_BuffList_Init();

	PageList *pl = NULL;
	PageList *pl1 = NULL;
	pl = (PageList *)BuffListManager_getTopNode(handle, sizeof(PageList));
#ifndef TEST_FREE_USED	
	(pl->head).next = NULL;
	pl1 = (PageList *)BuffListManager_getNextNode(handle, (void *)pl, sizeof(PageList));
	(pl1->head).next = NULL;
#endif 
	unsigned char *buf0 = malloc(conptr->vnand.BytePerPage);
	unsigned char *buf1 = malloc(conptr->vnand.BytePerPage);
	
	memset(buf0,0xff,conptr->vnand.BytePerPage);
	memset(buf1,0xff,conptr->vnand.BytePerPage);
	NandSigZoneInfo *nandsigzoneinfo = (NandSigZoneInfo *)buf0;
	NandZoneInfo *nandzoneinfo = (NandZoneInfo *)buf1;
	SigZoneInfo *sig1 = (SigZoneInfo *)(buf1 + sizeof(unsigned short));
	SigZoneInfo *sig2 = (SigZoneInfo *)(buf1 + 2*sizeof(unsigned short) + sizeof(SigZoneInfo));
	SigZoneInfo *sig3 = (SigZoneInfo *)(buf1 + 3 * sizeof(unsigned short) + 2 * sizeof(SigZoneInfo));
	unsigned int *maxptr = (unsigned int *)(buf1 + 3 * sizeof(unsigned short) + 3 * sizeof(SigZoneInfo));

	for(i = 0; i < zonenum ; i++)
	{
		nandsigzoneinfo->ZoneID = i;
		nandsigzoneinfo->lifetime = random()%3000;
		nandsigzoneinfo->badblock = 0;

		pl->startPageID = i * BLOCKPERZONE(conptr->vnand) * conptr->vnand.PagePerBlock;
		pl->OffsetBytes = 0;
		pl->Bytes = conptr->vnand.BytePerPage;
		pl->pData = buf0;
		pl->retVal = 0;
		
#ifndef TEST_FREE_USED
		//fill local zone info
		nandzoneinfo->localZone.ZoneID = i;
		nandzoneinfo->localZone.lifetime = nandsigzoneinfo->lifetime;
		nandzoneinfo->localZone.badblock = nandsigzoneinfo->badblock;
		nandzoneinfo->localZone.validpage = conptr->vnand.PagePerBlock * BLOCKPERZONE(conptr->vnand) - 3;
		printf("local zone %d lifetime %d \n",nandzoneinfo->localZone.ZoneID,nandzoneinfo->localZone.lifetime);

		//fill previous zone info
		nandzoneinfo->preZone.ZoneID = i - 1;
		nandzoneinfo->preZone.lifetime = random() %3000;
		nandzoneinfo->preZone.badblock = 0;
		nandzoneinfo->preZone.validpage = conptr->vnand.PagePerBlock * BLOCKPERZONE(conptr->vnand) - 3;
		printf("prev zone %d lifetime %d \n",nandzoneinfo->preZone.ZoneID,nandzoneinfo->preZone.lifetime);
		
		//fill next zone info
		nandzoneinfo->nextZone.ZoneID = i + 1;
		nandzoneinfo->nextZone.lifetime = random() %3000;
		nandzoneinfo->nextZone.badblock = 0;
		nandzoneinfo->nextZone.validpage = conptr->vnand.PagePerBlock * BLOCKPERZONE(conptr->vnand) - 3;
		printf("next zone %d lifetime %d \n",nandzoneinfo->nextZone.ZoneID,nandzoneinfo->nextZone.lifetime);
		
		nandzoneinfo->serialnumber = random() % 0xfffffff;
		printf("maxserial num %d \n",nandzoneinfo->serialnumber);	
		printf("\n");

		pl1->startPageID = i * BLOCKPERZONE(conptr->vnand) * conptr->vnand.PagePerBlock + 1;
		pl1->OffsetBytes = 0;
		pl1->Bytes = conptr->vnand.BytePerPage;
		pl1->pData = buf1;
		pl1->retVal = 0;
#endif 
		vNand_MultiPageWrite(&conptr->vnand,pl);
	}

	free(buf0);
	free(buf1);
	BuffListManager_freeAllList(handle, (void **)&pl, sizeof(PageList));
	BuffListManager_BuffList_DeInit(handle);
}
extern Context context;
int start_test_nand(int argc, char *argv[]){
	Context *conptr = &context;
	unsigned char *ptr = NULL;
	Zone *zoneptr = NULL;
	int i = 0;
	int j = 0;
	int ret = -1;

	init_nand_flash(conptr);

	conptr->blm = (BuffListManager *)BuffListManager_BuffList_Init();

	ZoneManager_Init((int)conptr);

	for(i = 0 ; i < 64 * 128 * 4; i++)
	{
		L1Info_set((int)conptr,i,i);
		//printf(" %d ,get L1info ID %d \n",i,L1Info_get((int)conptr,i));
	}

    	printf("recyID %d \n",ZoneManager_RecyclezoneID((int)conptr,3000));

	printf("test alloc Recycle zone \n");
	for(i = 0 ; i < 16 ; i ++)
	{
		zoneptr = ZoneManager_AllocRecyclezone((int)conptr,i);
		ZoneManager_FreeRecyclezone((int)conptr,zoneptr);
	}
	printf("test alloc zone \n");
	for(i = 0 ; i < 16 ; i ++)
	{
		zoneptr = ZoneManager_AllocZone((int)conptr);
		ZoneManager_FreeZone((int)conptr,zoneptr);
	}

	ZoneManager_DeInit((int)conptr);
	return 0;

}

static int Handle(int argc, char *argv[]){

	int ret = InitNandTest(argc,argv);
	DeinitNandTest();
	return ret;
}


static char *mycommand="zem";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
