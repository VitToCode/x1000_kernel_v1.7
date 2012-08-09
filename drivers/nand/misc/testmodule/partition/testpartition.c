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
#include "vNand.h"
#include "partitioninterface.h"
#include "nandmanagerinterface.h"
#include "ppartition.h"
#include <string.h>
#include <stdio.h>

PPartition ppt[] = {{"xboot",0,64,2048,128*8,20,512,0,64*128*8,1,NULL},{"kernel",0,64,2048,128*8,20,512,0,64*128*8,0,NULL},{"ubifs",0,64,2048,128*8,20,512,0,64*128*8,1,NULL}};
PPartArray partition={3,ppt};
VNandInfo test_info = {0,128,2048,128*8,NULL,20,512};
LPartition *app_lp;

static int Open ( VNandInfo* vn, PPartition* pt ){
	printf("test open ok\n");
	return 1111;
}

static int Close ( int handle ){
	printf("test close ok\n");
	return 0;
}

static int Read ( int context, SectorList* sl ){
	printf("test read ok\n");
	return 0;
}

static int Write ( int context, SectorList* sl ){
	printf("test write ok\n");
	return 0;
}

static int Ioctrl ( int context, int cmd, int argv ){
	printf("test ioctrl ok\n");
	return 0;
}

PartitionInterface interface = {
	.PartitionInterface_iOpen = Open,
	.PartitionInterface_iClose =Close,
	.PartitionInterface_Read = Read,
	.PartitionInterface_Write = Write,
	.PartitionInterface_Ioctrl = Ioctrl,
};

static int Open1 ( VNandInfo* vn, PPartition* pt ){
	printf("test1 open ok....................\n");
	return 2222;
}

static int Close1 ( int handle ){
	printf("test1 close ok..................\n");
	return 0;
}

static int Read1 ( int context, SectorList* sl ){
	printf("test1 read ok...................\n");
	return 0;
}

static int Write1 ( int context, SectorList* sl ){
	printf("test1 write ok......................\n");
	return 0;
}

static int Ioctrl1 ( int context, int cmd, int argv ){
	printf("test1 ioctrl ok...................\n");
	return 0;

}

PartitionInterface interface1 = {
	.PartitionInterface_iOpen = Open1,
	.PartitionInterface_iClose =Close1,
	.PartitionInterface_Read = Read1,
	.PartitionInterface_Write = Write1,
	.PartitionInterface_Ioctrl = Ioctrl1,
};

static int p_vnandinit(int handle){
	PManager *pm = (PManager*)handle;
	VNandManager *vnm ; 
	pm->vnand = (VNandManager*)malloc(sizeof(VNandManager));
	pm->bufferlist = (BuffListManager*)BuffListManager_BuffList_Init();
 
	if (pm->bufferlist == NULL)
	{
		printf("func: %s line: %d \n",__func__,__LINE__);
	}

	vnm = pm->vnand;
	vnm->info = test_info;
	vnm->pt = &partition;
	pm->Mlist = NULL;

	printf("p_vnandinit parameter:\n \
            pagesize: %d   hwsector: %d \n \
            ptcount: %d\n \
            name:%s  pagecount: %d  mode: %d\n",vnm->info.BytePerPage,vnm->info.hwSector,vnm->pt->ptcount,vnm->pt->ppt->name, \
            vnm->pt->ppt->PageCount,vnm->pt->ppt->mode);
	printf("p_vnandinit end !!\n");

	return (int)pm;	
}

static int register_test(int handle){
	PManager *pm = (PManager*)handle;
	struct singlelist *pos;
	ManagerList *mlist;

	printf("FUNCTION: %s test begin. \n",__func__);

	NandManger_Register_Manager(handle,1,&interface);
	NandManger_Register_Manager(handle,0,&interface1);
	
	NandManger_Register_Manager(handle,1,&interface);

	singlelist_for_each(pos,&pm->Mlist->head){
		mlist = singlelist_entry(pos,ManagerList,head);
		printf("mode: %d\n  nmi: %p\n",mlist->mode,mlist->nmi);		
	}
	
	return 0;
}

static int  getpartition_test(int handle){
	struct singlelist *pos;
	LPartition *lp;
	LPartition *printlp;
	printf("FUNCTION: %s test begin. \n",__func__);

	 NandManger_getPartition(handle, &lp);
	app_lp = lp;
	singlelist_for_each(pos,&lp->head){
		printlp = singlelist_entry(pos,LPartition,head);
		printf("startsector: %d\n sectorcount: %d \n name %s \n mode %d \n",printlp->startSector,printlp->sectorCount,printlp->name,printlp->mode);
	}

	return 0;
}

static int nandopen_test(int handle){
	struct singlelist *pos;
	LPartition *lp;
	int add;

	printf("FUNCTION: %s test begin. \n",__func__);

	singlelist_for_each(pos,&app_lp->head){
		lp = singlelist_entry(pos,LPartition,head);
		printf("lp name %s \n",lp->name);
		add = NandManger_open(handle,lp->name, 1);
		}

	return add;
}

static int Handle(int argc, char *argv[]){
	int add_test;
	int handle;
	int handle_test;

	printf("Partition test begain !\n");
	handle = NandManger_Init();
	handle_test = p_vnandinit(handle);

	register_test(handle_test);
	getpartition_test(handle_test);
	add_test = nandopen_test(handle_test);
	if (add_test == 0){
		printf("The name is not eixt! Please open again with the correct name !\n");
		return 0;
	}
	NandManger_write(add_test, NULL);
	NandManger_read(add_test, NULL);
	NandManger_ioctrl(add_test, 0, 0);
	NandManger_close(add_test);
	
	return 0;
}

static char *mycommand="pa";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
