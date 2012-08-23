#include "nanddebug.h"
#include "nandmanagerinterface.h"
#include "singlelist.h"
#include "vnandinfo.h"
#include "ppartition.h"
#include "lpartarray.h"
#include "lpartition.h"
#include "managerlist.h"
#include "partcontext.h"
#include "partitioninterface.h"
#include "l2pconvert.h"
#include "simpleblockmanager.h"
#include "vNand.h"
#include "clib.h"

#define N_UNITSIZE     8
#define N_UNITDIM(x)  (((x) + N_UNITSIZE - 1 ) / N_UNITSIZE)
#define BLOCK_PER_ZONE 8
#define ZONEINFO_PAGES 3 
#define SECTOR_SIZE    512

/* Translate the physical partition structto logicla partition struct */
static int  p2lPartition( PPartition *ppa, LPartition *lpa){
	int  pageperzone;
	int  maxdatapage;
	int  zonenum;
	int  zonevalidpage;
	int  extrapage;

	if (ppa == NULL || lpa == NULL){
		ndprint(PARTITION_ERROR,"ERROR:FUNCTION: %s  LINE: %d  Nand alloc continue memory error!\n",__FUNCTION__,__LINE__);
		return -1;		
	}
	pageperzone = ppa->pageperblock * BLOCK_PER_ZONE;
	maxdatapage = 1024 / sizeof(unsigned int) / (ppa->byteperpage / SECTOR_SIZE);
	zonenum = (ppa->totalblocks - ppa->badblockcount) / BLOCK_PER_ZONE;
	zonevalidpage = (pageperzone - ZONEINFO_PAGES) / (maxdatapage + 1) * maxdatapage;
	extrapage = (pageperzone - ZONEINFO_PAGES) % (maxdatapage + 1)-1;
	
	if (extrapage > 1)
		lpa->sectorCount = (zonevalidpage + extrapage) * ppa->byteperpage / SECTOR_SIZE * zonenum;
	else
		lpa->sectorCount = zonevalidpage * ppa->byteperpage / SECTOR_SIZE * zonenum;

	lpa->startSector = 0;
	lpa->name = ppa->name; 
	lpa->hwsector = ppa->hwsector;
	lpa->segmentsize = 1024/4*(ppa->hwsector);

	lpa->mode = ppa->mode;
	lpa->pc = NULL;

	return 0;
}

static void start(int handle){
	PManager *pm = (PManager*)handle;
	struct NotifyList *nl;
	struct singlelist *it;
	if(-1 == vNand_Init(&pm->vnand)){
		ndprint(PARTITION_ERROR, "ERROR:vNand_Init error  func %s line %d\n",__FUNCTION__,__LINE__);
		return;
	}	

#ifndef TEST_PARTITION
	if (-1 == SimpleBlockManager_Init(pm)){
		ndprint(PARTITION_ERROR, "ERROR:SimpleBlockManager_Init failed\n");
		return;	
	}
	if (-1 == L2PConvert_Init(pm)){
		ndprint(PARTITION_ERROR, "ERROR:L2PConvert_Init failed\n");
		return;			
	}
#endif

	singlelist_for_each(it,&pm->nl->head){
		nl = singlelist_entry(it,struct NotifyList,head);
		if(nl->start){
			nl->start(nl->prdata);
		}
	}
}

/* Please open the partition before you operate(read,write,ioctrl,close) the partition.
 * @name: The partition name you want to open
 * @mode: DIRECT_MANAGER ----0
 *        ZONE_MANAGER ------1
 */
int  NandManger_open ( int handle,const char* name, int mode ){
	PManager *pm = (PManager*)handle;
	struct singlelist *pos;
	struct singlelist *lpos;
	LPartition *lp;
	ManagerList *ml;
	PartitionInterface *pInterface = NULL;
	PPartition  *ppartition;
	VNandInfo *vInfo;
	int name_exit = 0;
	int nmhandle = 0;
	int count;
	int i;

	if (pm->lpt.pt == NULL || pm->Mlist == NULL){
		ndprint(PARTITION_ERROR, "ERROR:Please be sure to call NandManger_getPartition() first !! \n");
		return 0;
	}
	/*match the name*/
	singlelist_for_each(lpos, &pm->lpt.pt->head){
		lp = singlelist_entry(lpos, LPartition, head);
		ndprint(PARTITION_INFO, "lp->name:%s  name:%s\n",lp->name,name);
		if (!strncmp(lp->name, name, strlen(name))){
			name_exit = 1;
			break;
		}		
	}
	if (name_exit == 0){
		ndprint(PARTITION_ERROR, "ERROR:FUNCTION:%s  LINE:%d  The partition name is not exit , it can't be open!\n",__FUNCTION__,__LINE__);
		return 0;
	}

	/*match the mode*/
	singlelist_for_each(pos, &pm->Mlist->head){
		ml = singlelist_entry(pos, ManagerList, head);
		if (ml->mode == mode){
			pInterface = ml->nmi;
			break;
		}
	}
	if (!pInterface) {
		ndprint(PARTITION_ERROR, "ERROR:FUNCTION:%s  LINE:%d  Can't find operations!! \n",__FUNCTION__,__LINE__);
		return 0;
	}

	ppartition = pm->vnand->pt->ppt;
	count = pm->vnand->pt->ptcount;
	vInfo = &pm->vnand->info;
	/*match the ppartition*/
	for(i=0; i<count; i++){
		if (!strncmp((ppartition+i)->name, name, strlen(name))){
			nmhandle = pInterface->PartitionInterface_iOpen(vInfo, (ppartition+i));
			if (nmhandle == 0){
				ndprint(PARTITION_ERROR, "ERROR:FUNCTION:%s  LINE:%d \n The partition open failed! \n",__FUNCTION__,__LINE__);	
				return 0;
			}
			break;
		}
	}

	/*fill the PartContext structure lp->pc*/
	lp->pc = Nand_VirtualAlloc(sizeof(PartContext));
	lp->pc->nmhandle = nmhandle;
	lp->pc->ptif = pInterface;
	ndprint(PARTITION_INFO, "Nand manager interface open ok !\n");
	return (int)(lp->pc);
}

int NandManger_read ( int context, SectorList* bl ){
	PartContext *pcontext;
	pcontext = (PartContext*)context;

	return pcontext->ptif->PartitionInterface_Read(pcontext->nmhandle,bl);
}

int NandManger_write ( int context, SectorList* bl ){
	PartContext *pcontext;
	pcontext = (PartContext*)context;

	return pcontext->ptif->PartitionInterface_Write(pcontext->nmhandle,bl);
}

int NandManger_ioctrl ( int context, int cmd, int args ){
	PartContext *pcontext;
	pcontext = (PartContext*)context;

	return pcontext->ptif->PartitionInterface_Ioctrl(pcontext->nmhandle,cmd,args);
}

int NandManger_close ( int context ){
	PartContext *pcontext;
	pcontext = (PartContext*)context;

	return pcontext->ptif->PartitionInterface_iClose(pcontext->nmhandle);
}

/*Get the logical partition info and fill the LPartition structure*/
int NandManger_getPartition ( int handle, LPartition** pt ){
	PManager *pm = (PManager*)handle;
	BuffListManager *blm = pm->bufferlist;
	PPartition *ppa = NULL;
	LPartition *lpa = NULL;
	int ret = -1;
	int count;
	int i;


	ppa = pm->vnand->pt->ppt;
	count = pm->vnand->pt->ptcount;

	for (i=0; i<count; i++){
		if(lpa == NULL){
			lpa = (LPartition *)BuffListManager_getTopNode((int)blm, sizeof(LPartition));
			pm->lpt.pt = lpa;
			*pt = pm->lpt.pt;
		}else
			lpa = (LPartition *)BuffListManager_getNextNode((int)blm,(void*)lpa,sizeof(LPartition));

		if(lpa){
			if ( -1 == (ret = p2lPartition((ppa+i), lpa)) ){
				ndprint(PARTITION_ERROR, "ERROR:FUNCTION: %s  LINE: %d  Get partition%d failed !\n",__FUNCTION__,__LINE__,i);
				return -1;
			}	
		}else{
			ndprint(PARTITION_ERROR,"ERROR:func: %s line: %d  Alloc memory error !\n",__func__,__LINE__);
			return -1;
		}
		
	}

	ndprint(PARTITION_INFO, "Nand manager interface get partition  ok !\n");
	return 0;
}

/*If you want to use nand manager layer, please registe the PartitionInterface
  and the mode first.*/
int NandManger_Register_Manager ( int handle, int mode, PartitionInterface* pi ){
	PManager *pm = (PManager*)handle;
	ManagerList *mlist = pm->Mlist;
	
	if (pi == NULL){
		ndprint(PARTITION_ERROR, "ERROR:PartitionInterface is NULL. func %s line %d \n",__FUNCTION__,__LINE__);		
		return -1;
	}

	if (mlist == NULL){
		mlist = (ManagerList*)BuffListManager_getTopNode((int)pm->bufferlist, sizeof(ManagerList)); 
		pm->Mlist = mlist;
	}
	else
		mlist = (ManagerList*)BuffListManager_getNextNode((int)pm->bufferlist,(void*)mlist,sizeof(ManagerList)); 

	if (mlist){
		mlist -> mode = mode;
		mlist -> nmi = pi;
	}
	else{
		ndprint(PARTITION_ERROR, "ERROR:Nand alloc continue memory error func %s line %d \n",__FUNCTION__,__LINE__);
		return -1;
	}

	ndprint(PARTITION_INFO, "Nand manager interface register  ok !\n");	
	return 0;
}

/*Alloc the memory and do init*/
int NandManger_Init (void){
	PManager *pm;
	int blm;

	pm = Nand_VirtualAlloc(sizeof(PManager));
	if (pm == NULL){
		ndprint(PARTITION_ERROR, "ERROR:Nand alloc continue memory error func %s line %d \n",__FUNCTION__,__LINE__);
		return 0;
	}
	if (0 == (blm=BuffListManager_BuffList_Init())){    
		ndprint(PARTITION_ERROR, "ERROR:BuffListManager_BuffList_Init failed\n");
		return 0;
	}

	pm->bufferlist = (BuffListManager*)blm;
	pm->vnand = NULL;
	pm->Mlist = NULL;
	pm->nl = NULL;
 
	Register_StartNand(start, (int)pm);
	ndprint(PARTITION_INFO, "Nand manager interface init  ok !\n");	
	return (int)pm;
}

/*Free the alloced memory and do deinit*/
void NandManger_Deinit (int handle){
	PManager *pm = (PManager*)handle;

	vNand_Deinit(&pm->vnand);	
#ifndef TEST_PARTITION
	SimpleBlockManager_Deinit(0);
	L2PConvert_Deinit(0);
#endif
	Nand_VirtualFree(pm->lpt.pt->pc);
	BuffListManager_freeAllList((int)(pm->bufferlist), (void**)(&(pm->lpt.pt)), sizeof(LPartition));
	BuffListManager_freeAllList((int)(pm->bufferlist),(void**)(&(pm->nl)), sizeof(struct NotifyList));
	BuffListManager_freeAllList((int)(pm->bufferlist), (void**)(&(pm->Mlist)), sizeof(ManagerList));
	BuffListManager_BuffList_DeInit ((int)(pm->bufferlist)); 
	Nand_VirtualFree(pm);
}

void NandManger_startNotify(int handle,void (*start)(int),int prdata){
	PManager *pm = (PManager*)handle;
	struct NotifyList* nlist = pm->nl;
	
	if (nlist == NULL){
		nlist = (struct NotifyList*)BuffListManager_getTopNode((int)pm->bufferlist, sizeof(struct NotifyList)); 
		pm->nl = nlist;
	}
	else
		nlist = (struct NotifyList*)BuffListManager_getNextNode((int)pm->bufferlist,(void*)nlist,sizeof(struct NotifyList));
 
	if(nlist){
		nlist->start = start;
		nlist->prdata = prdata;
	}
	else{
		ndprint(PARTITION_ERROR,"ERROR:func: %s line: %d.ALLOC MEMORY FAILED!\n",__func__,__LINE__);
		while(1);
	}
}

