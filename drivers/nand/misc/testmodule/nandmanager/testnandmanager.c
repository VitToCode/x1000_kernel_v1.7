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
#include "recycle.h"
#include "taskmanager.h"
#include "NandThread.h"
#include "zonememory.h"
#include "simpleblockmanager.h"
#include "nandmanagerinterface.h"
#include "nanddebug.h"
#include "pmanager.h"
#include "nandsigzoneinfo.h"

extern void ND_Init(void);

#define BLOCKPERZONE(context)   	8

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
		printf("--startSector = %d, sectorCount = %d, pData: %s--\n", top->startSector, top->sectorCount, (char *)(top->pData));
	}

	printf("======================================\n\n");
}

static void init_nand_flash(VNandManager *vm)
{
	int i, j, k;
	int handle;
	VNandInfo *vnand = &vm->info;
	unsigned int totalzonenum = (vnand->TotalBlocks - 1) / BLOCKPERZONE(vnand) + 1;
	unsigned int zonenum[vm->pt->ptcount];
	PageList *pl = NULL;
	vm->info.prData = (int *)(vm->pt->ppt);

	handle = BuffListManager_BuffList_Init();

	pl = (PageList *)BuffListManager_getTopNode(handle, sizeof(PageList));

	for (i = 0; i < vm->pt->ptcount; i++) {
		if (i == 0)	
			zonenum[i] = (vm->pt->ppt[i].totalblocks - 1) / BLOCKPERZONE(vnand) + 1;
		else
			zonenum[i] = zonenum[i - 1] + (vm->pt->ppt[i].totalblocks - 1) / BLOCKPERZONE(vnand) + 1;
	}

	unsigned char *buf = malloc(vnand->BytePerPage);	
	memset(buf,0xff,vnand->BytePerPage);
	NandSigZoneInfo *nandsig = (NandSigZoneInfo *)buf;

	for(i = 0; i < totalzonenum; i++, j++) {
		for (k = 0; k < vm->pt->ptcount; k++) {
			if (i % zonenum[k] == 0)
				j = 0;
		}

		nandsig->ZoneID = j;
		nandsig->lifetime = random() % 300;
		nandsig->badblock = 0;

		pl->startPageID = i * BLOCKPERZONE(vnand) * vnand->PagePerBlock;
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
static void start(int handle){
	printf("%s(%s) %d\n",__FUNCTION__,__FILE__,__LINE__);	
}
static int Handle(int argc, char *argv[]){
	int i, ret;
	SectorList *sl_write, *sl_read;
	int nhandle;
	int ohandle;
	int handle;
	BuffListManager *blm;
	LPartition *lp;
	PManager *pm = NULL;
	
	nhandle = NandManger_Init();
	
	ND_Init();
	
	NandManger_startNotify(nhandle,start,(int)pm);

	pm = (PManager *)nhandle;

	init_nand_flash(pm->vnand);
	ret = NandManger_getPartition(nhandle,&lp);
	if(ret != 0){
		printf("FUNCTION:%s  LINE:%d  NandManger_getPartition failed!\n",__FUNCTION__,__LINE__);
		return -1;		
	}
	
#ifdef TEST_ZONE_MANAGER     //write in Makefile if defined
	ohandle = NandManger_open(nhandle, "x-boot", ZONE_MANAGER);
	if (ohandle == -1){
		printf("FUNCTION:%s  LINE:%d  The partition name is not exit , it can't be open!",__FUNCTION__,__LINE__);
		return -1;
	}
	handle = ((PartContext *)ohandle)->nmhandle;
	blm = ((Context *)handle)->blm;
#else
	ohandle = NandManger_open(nhandle, "ubifs", DIRECT_MANAGER);
	if (ohandle == -1){
		printf("FUNCTION:%s  LINE:%d  The partition name is not exit , it can't be open!",__FUNCTION__,__LINE__);
		return -1;
	}
	handle = ((PartContext *)ohandle)->nmhandle;
	blm = ((SmbContext *)handle)->blm;
#endif

	sl_write = (SectorList *)BuffListManager_getTopNode((int)blm, sizeof(SectorList));
	sl_read = (SectorList *)BuffListManager_getTopNode((int)blm, sizeof(SectorList));

	sl_write->startSector = 0;
	sl_write->sectorCount = 2000;
	sl_write->pData = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * sl_write->sectorCount * SECTOR_SIZE);
	memset(sl_write->pData, 0x31, sizeof(unsigned char) * sl_write->sectorCount * SECTOR_SIZE);

	sl_read->startSector = 0;
	sl_read->sectorCount = 2000;
	sl_read->pData = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * sl_read->sectorCount * SECTOR_SIZE);
	memset(sl_read->pData, 0x0, sizeof(unsigned char) * sl_read->sectorCount * SECTOR_SIZE);

#ifdef TEST_ZONE_MANAGER
	for (i = 0; i < 15; i++) {
		ret = NandManger_write(ohandle, sl_write);
		if (ret == -1) {
			printf("L2PConvert_WriteSector failed!\n");
			return -1;
		}
	}
	printf("L2PConvert_WriteSector finished!\n");
	sleep(5);
#else
	ret = NandManger_write(ohandle, sl_write);
	if (ret == -1) {
		printf("L2PConvert_WriteSector failed!\n");
		return -1;
	}
	printf("L2PConvert_WriteSector finished!\n");
#endif

	ret = NandManger_read(ohandle, sl_read);
	if (ret == -1) {
		printf("L2PConvert_ReadSector failed!\n");
		return -1;
	}
	printf("L2PConvert_ReadSector finished!\n");
	dumpSectorList(sl_read);

	return 0;
}




static char *mycommand="nm";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
