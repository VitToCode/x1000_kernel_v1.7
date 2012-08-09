#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "testfunc.h"
#include "simpleblockmanager.h"
#include "vnandinfo.h"
#include "nandinterface.h"
#include "pagelist.h"
#include "context.h"
#include "zone.h"
#include "vNand.h"
#include "pageinfo.h"
#include "l1info.h"

extern int InitNandTest(int argc, char *argv[]);
extern void DeinitNandTest();
extern int SimpleBlockManager_Open(VNandInfo *vn, PPartition *pt);
extern int SimpleBlockManager_Close(int handle);
extern int SimpleBlockManager_Read(int context, SectorList *sl);
extern int SimpleBlockManager_Write(int context, SectorList *sl);

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
		printf("--startSector = %d, sectorCount = %d, pData: %s--\n", top->startSector, top->sectorCount, (char *)(top->pData));
	}

	printf("======================================\n\n");
}

extern Context context;
int start_test_nand(int argc, char *argv[]){
	int i, handle, smbcontext;
	SectorList *sl_read, *sl_write, *sl_node;
	Context * conptr = &context;
	VNandInfo* vnand = &conptr->vnand;
	int pageperblock = vnand->PagePerBlock;
	int sectorperpage = vnand->BytePerPage / SECTOR_SIZE;
	int sectorperblock = sectorperpage * pageperblock;
	
	handle = BuffListManager_BuffList_Init();
	if  (handle == -1) {
		printf("BuffListManager_BuffList_Init failed fun %s line %d\n", __FUNCTION__, __LINE__);
		return -1;
	}

	sl_write = (SectorList *)BuffListManager_getTopNode(handle,sizeof(SectorList));
	sl_read = (SectorList *)BuffListManager_getTopNode(handle,sizeof(SectorList));

	for (i = 0; i < 10; i++) {
		if (i == 0) {
			sl_node = sl_write;
		}
		else {
			sl_node = (SectorList *)BuffListManager_getNextNode(handle, 
					(void *)sl_write,sizeof(SectorList));
		}
		sl_node->startSector = (i * 20 + 1) + (sectorperblock * i);
		sl_node->sectorCount =  i   + 1 + (sectorperblock * i);

		sl_node->pData = (unsigned char *)Nand_VirtualAlloc(
				sizeof(unsigned char) * sl_node->sectorCount * SECTOR_SIZE);
		memset(sl_node->pData, 0x31 + i, sizeof(unsigned char) * sl_node->sectorCount * SECTOR_SIZE);

	}

	for (i = 0; i < 10; i++) {
		if (i == 0) {
			sl_node = sl_read;
		} else {
			sl_node = (SectorList *)BuffListManager_getNextNode(handle, 
					(void *)sl_read,sizeof(SectorList));
		}
		sl_node->startSector = (i * 20 + 1) + (sectorperblock * i);
		sl_node->sectorCount =  i   + 1 + (sectorperblock * i);

		sl_node->pData = (unsigned char *)Nand_VirtualAlloc(
				sizeof(unsigned char) * sl_node->sectorCount * SECTOR_SIZE);
		memset(sl_node->pData, 0x0, sizeof(unsigned char) * sl_node->sectorCount * SECTOR_SIZE);
	}
	
	//BuffListManager_freeList(handle, (void **)&sl_write, (void *)sl_write,sizeof(SectorList));
	//BuffListManager_freeList(handle, (void **)&sl_read, (void *)sl_read,sizeof(SectorList));
	PPartition *p = (PPartition*)conptr->vnand.prData;
	p->mode = DIRECT_MANAGER;
	smbcontext = SimpleBlockManager_Open(&conptr->vnand,(PPartition*)conptr->vnand.prData);
	SmbContext *smbconptr = (SmbContext *)smbcontext;
	smbconptr->blm = (BuffListManager *)handle;

	vNand_MarkBadBlock(&conptr->vnand, 0);
	vNand_MarkBadBlock(&conptr->vnand, 3);
	vNand_MarkBadBlock(&conptr->vnand, 5);
	printf("SimpleBlockManager_Write start.\n");
	SimpleBlockManager_Write(smbcontext, sl_write);
	printf("SimpleBlockManager_Write finished:\n");
	dumpSectorList(sl_write);

	SimpleBlockManager_Read(smbcontext, sl_read);
	printf("SimpleBlockManager_Read finished:\n");
	dumpSectorList(sl_read);

	SimpleBlockManager_Close(smbcontext);


	BuffListManager_freeAllList(handle, (void **)&sl_write, sizeof(SectorList));
	BuffListManager_freeAllList(handle, (void **)&sl_read, sizeof(SectorList));

	BuffListManager_BuffList_DeInit(handle);

	return 0;
}

static int Handle(int argc, char *argv[]){

	int ret = InitNandTest(argc,argv);
	DeinitNandTest();
	return ret;
}

static char *mycommand="sbm";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
