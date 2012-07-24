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
#include <string.h>

#define N_UNITSIZE     8
#define N_UNITDIM(x)  (((x) + N_UNITSIZE - 1 ) / N_UNITSIZE)


/* Translate the physical partition structto logicla partition struct */
static int  p2lPartition(int handle, PPartition *ppa, LPartition *lpa){
	PManager  *pm = (PManager*)handle;
	VNandInfo *vninfo = &pm->vnand->info;

	if (ppa == NULL || lpa == NULL){
		ndprint(1,"FUNCTION: %s  LINE: %d  Nand alloc continue memory error!\n",__FUNCTION__,__LINE__);
		return -1;		
	}

	lpa->startSector = 0;
	lpa->sectorCount = vninfo->BytePerPage /(int)vninfo->hwSector * ppa->PageCount;
	lpa->name = ppa->name; 
	lpa->hwsector = ppa->hwsector;
	lpa->segmentsize = 1024/4*(ppa->hwsector);

	lpa->mode = ppa->mode;
	lpa->head.next = NULL;
	lpa->pc = NULL;

	return 0;
}

static void start(int handle){
	PManager *pm = (PManager*)handle;
	int blm;
	struct NotifyList *nl;
	struct singlelist *it;
	if(-1 == vNand_Init(&pm->vnand)){
		ndprint(1, "vNand_Init error  func %s line %d\n",__FUNCTION__,__LINE__);
		return;
	}
	if (-1 == (blm=BuffListManager_BuffList_Init())){    
		ndprint(1, "BuffListManager_BuffList_Init failed\n");
		return;
	}
	pm->bufferlist = (BuffListManager*)blm;

#ifndef TEST_PARTITION
	if (0 != SimpleBlockManager_Init(pm)){
		ndprint(1, "SimpleBlockManager_Init failed\n");
		return;	
	}
	if (-1 == L2PConvert_Init(pm)){
		ndprint(1, "L2PConvert_Init failed\n");
		return;			
	}
#endif

	singlelist_for_each(it,pm->startlist_top.next){
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
	int p_zid = pm->p_zid;
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

	if (pm->lpt.pt == NULL || pm->mltop.next == NULL){
		ndprint(1, "Please be sure to call NandManger_getPartition() first !! \n");
		return 0;
	}
	/*match the name*/
	singlelist_for_each(lpos, pm->lpt.pt->head.next){
		lp = singlelist_entry(lpos, LPartition, head);
		ndprint(1, "lp->name:%s  name:%s\n",lp->name,name);
		if (!strncmp(lp->name, name, strlen(name))){
			name_exit = 1;
			break;
		}		
	}
	if (name_exit == 0){
		ndprint(1, "FUNCTION:%s  LINE:%d  The partition name is not exit , it can't be open!\n",__FUNCTION__,__LINE__);
		return 0;
	}

	/*match the mode*/
	singlelist_for_each(pos, pm->mltop.next){
		ml = singlelist_entry(pos, ManagerList, head);
		if (ml->mode == mode){
			pInterface = ml->nmi;
			break;
		}
	}

	ppartition = pm->vnand->pt->ppt;
	count = pm->vnand->pt->ptcount;
	vInfo = &pm->vnand->info;
	/*match the ppartition*/
	for(i=0; i<count; i++){
		if (!strncmp((ppartition+i)->name, name, strlen(name))){
			nmhandle = pInterface->PartitionInterface_iOpen(vInfo, (ppartition+i));
			if (nmhandle == 0){
				ndprint(1, "FUNCTION:%s  LINE:%d \n The partition open failed! \n",__FUNCTION__,__LINE__);	
				return 0;
			}
			break;
		}
	}

	/*fill the PartContext structure lp->pc*/
	lp->pc = (PartContext*)ZoneMemory_NewUnits(p_zid,N_UNITDIM(sizeof(PartContext)));
	lp->pc->nmhandle = nmhandle;
	lp->pc->ptif = pInterface;
	ndprint(1, "Nand manager interface open ok !\n");
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
	int p_zid = pm->p_zid;
	PPartition *ppa = NULL;
	LPartition *lpa = NULL;
	LPartition *conptr;
	int ret = -1;
	int count;
	int i;


	pm->lpt.pt = (LPartition*)ZoneMemory_NewUnits(p_zid,N_UNITDIM(sizeof(LPartition)));
	conptr = pm->lpt.pt;
	ppa = pm->vnand->pt->ppt;
	count = pm->vnand->pt->ptcount;

	for (i=0; i<count; i++){
		lpa = (LPartition*)ZoneMemory_NewUnits(p_zid,N_UNITDIM(sizeof(LPartition)));
		if (lpa == NULL){
			ndprint(1, "FUNCTION: %s  LINE: %d  Memory alloc failed !\n",__FUNCTION__,__LINE__);
			return -1;			
		}

		if ( -1 == (ret = p2lPartition(handle, (ppa+i), lpa)) ){
			ndprint(1, "FUNCTION: %s  LINE: %d  Get partition%d failed !\n",__FUNCTION__,__LINE__,i);
			return -2;
		}

		singlelist_add_tail(&conptr->head,&lpa->head);		
	}

	*pt = singlelist_entry(conptr->head.next, LPartition, head);
	ndprint(1, "Nand manager interface get partition  ok !\n");
	return (int)pm;
}

/*If you want to use nand manager layer, please registe the PartitionInterface
  and the mode first.*/
int NandManger_Register_Manager ( int handle, int mode, PartitionInterface* pi ){
	PManager *pm = (PManager*)handle;
	int p_zid = pm->p_zid;
	ManagerList  *Mlist;

	if (pi == NULL)
		return -1;

	Mlist = (ManagerList*)ZoneMemory_NewUnits(p_zid,N_UNITDIM(sizeof(ManagerList)));
	if (Mlist == NULL){
		ndprint(1, "Nand alloc continue memory error func %s line %d \n",__FUNCTION__,__LINE__);
		return -1;
	}

	Mlist -> mode = mode;
	Mlist -> nmi = pi;
	Mlist -> head.next = NULL;
	singlelist_add_tail(&pm->mltop,&Mlist->head);
	ndprint(1, "Nand manager interface register  ok !\n");	
	return 0;
}

/*Alloc the memory and do init*/
int NandManger_Init (void){
	int p_zid;
	PManager *pm;
	
	p_zid = ZoneMemory_Init(N_UNITSIZE);
	pm = (PManager*)ZoneMemory_NewUnits(p_zid,N_UNITDIM(sizeof(PManager)));
	if (pm == NULL){
		ndprint(1, "Nand alloc continue memory error func %s line %d \n",__FUNCTION__,__LINE__);
		return -1;
	}

	pm->mltop.next = NULL;
	pm->p_zid = p_zid;
	pm->startlist_top.next = NULL;
	pm->vnand = NULL;
 
	Register_StartNand(start, (int)pm);
	ndprint(1, "Nand manager interface init  ok !\n");	
	return (int)pm;
}

/*Free the alloced memory and do deinit*/
void NandManger_Deinit (int handle){
	PManager *pm = (PManager*)handle;
	int p_zid = pm->p_zid;

	BuffListManager_BuffList_DeInit ((int)pm->bufferlist); 
	vNand_Deinit(&pm->vnand);
	
#ifndef TEST_PARTITION
	SimpleBlockManager_Deinit(0);
	L2PConvert_Deinit(0);
#endif
	ZoneMemory_DeInit(p_zid);
}
void NandManger_startNotify(int handle,void (*start)(int),int prdata){
	PManager *pm = (PManager*)handle;
	int p_zid = pm->p_zid;
	struct NotifyList *nl;
	nl = (struct NotifyList*)ZoneMemory_NewUnits(p_zid,N_UNITDIM(sizeof(struct NotifyList)));
	nl->head.next = NULL;
	nl->start = start;
	nl->prdata = prdata;
	singlelist_add_tail(&pm->startlist_top,&nl->head);
}

