#include "nanddebug.h"
#include "nandmanagerinterface.h"
#include "singlelist.h"
#include "vnandinfo.h"
#include "lpartarray.h"
#include "managerlist.h"
#include "partcontext.h"
#include "partitioninterface.h"
#include "l2pconvert.h"
#include "simpleblockmanager.h"
#include "splblockmanager.h"
#include "vNand.h"
#include "l2vNand.h"
#include "os/clib.h"
#include "zone.h"

#define N_UNITSIZE     8
#define N_UNITDIM(x)  (((x) + N_UNITSIZE - 1 ) / N_UNITSIZE)
#define PAGEINFO_PAGE 1
#define SECTOR_SIZE    512

/* Translate the physical partition structto logicla partition struct */
static int p2lPartition( PPartition *ppa, LPartition *lpa)
{
	int pageperzone;
	int maxdatapage;
	int zonenum;
	int zonevalidpage;
	int extrapage;
	int zoneinfo_pages;
	int bestsecnum;
	int worsesecnum;
	int reservesecnum;
        int reservezonenum;
	int totalblocks;
        int initsectors;

	if (ppa == NULL || lpa == NULL){
		ndprint(PARTITION_ERROR,"ERROR:FUNCTION: %s(%d), Nand alloc continue memory error!\n",
				__FUNCTION__, __LINE__);
		return -1;
	}
        totalblocks = ppa->totalblocks - ppa->actualbadblockcount - ppa->badblockcount;

	if (ppa->mode == ZONE_MANAGER) {
		pageperzone = ppa->pageperblock * ppa->v2pp->_2kPerPage * BLOCK_PER_ZONE;
		maxdatapage = L4INFOLEN / sizeof(unsigned int) / (ppa->byteperpage
                                                / ppa->v2pp->_2kPerPage / SECTOR_SIZE);
                /* the best is 90% , 2k:pageinfo L4INFOLEN:data */
                zonenum = (totalblocks * 90 / 100) / BLOCK_PER_ZONE;
                if(ppa->v2pp->_2kPerPage > 1) {
			zoneinfo_pages = ppa->v2pp->_2kPerPage * 2;
                        /* the worse is 10% ,2k:pageinfo 2k:data */
		        worsesecnum = (totalblocks - zonenum * BLOCK_PER_ZONE) *
                                                ppa->pageperblock * ppa->byteperpage /
                                                        SECTOR_SIZE / ppa->v2pp->_2kPerPage;
                } else {
			zoneinfo_pages = 3;
                        /* the worse is 10% ,2k:pageinfo 2k:data */
		        worsesecnum = (totalblocks - zonenum * BLOCK_PER_ZONE) *
                                                ppa->pageperblock * ppa->byteperpage /
                                                        SECTOR_SIZE / 2;
                }
		zonevalidpage = (pageperzone - zoneinfo_pages) /
                                        (maxdatapage + PAGEINFO_PAGE) * maxdatapage;
		extrapage = (pageperzone - zoneinfo_pages) % (maxdatapage + PAGEINFO_PAGE);

		if (extrapage > ppa->v2pp->_2kPerPage)
                        bestsecnum = (zonevalidpage + extrapage) * (ppa->byteperpage /
                                        ppa->v2pp->_2kPerPage / SECTOR_SIZE) * zonenum;
                else
			bestsecnum = zonevalidpage * (ppa->byteperpage /
                                        ppa->v2pp->_2kPerPage / SECTOR_SIZE) * zonenum;
                /* the 4% is reserve zone for recycle */
                reservezonenum = (totalblocks * 4 / 100) / BLOCK_PER_ZONE;
                if (reservezonenum > 9)
                        reservezonenum = 9;
                if (reservezonenum < 4)
                        reservezonenum = 4;

		reservesecnum = reservezonenum * BLOCK_PER_ZONE *
                                        ppa->pageperblock * ppa->byteperpage / SECTOR_SIZE;

		lpa->sectorCount = bestsecnum + worsesecnum;
                /*
                 * when the calc secoters by 90% best > the totalsecotrs's 80%
                 * we will set the 'lpa->sectorCount = the totalsecotrs's 80%'
                 * otherwise we will set the 'lpa->sectorCount = bestsecnum + worsesecnum'
                 *
                 */
                initsectors = totalblocks * 85 / 100 * ppa->pageperblock * ppa->byteperpage / SECTOR_SIZE;
                if (lpa->sectorCount > initsectors)
		        lpa->sectorCount = initsectors;
                lpa->sectorCount = lpa->sectorCount - reservesecnum;
	} else{
		lpa->sectorCount = (totalblocks * ppa->pageperblock * ppa->byteperpage) / SECTOR_SIZE;
        }
        ndprint(1,"%s: reservezone=%d badblocks=%d 90%%+10%%=%d lpa->sectorCount=%d\n"
                        , ppa->name, reservezonenum, ppa->badblockcount
                        , bestsecnum + worsesecnum, lpa->sectorCount);
	lpa->startSector = 0;
	lpa->name = ppa->name;
	lpa->hwsector = ppa->hwsector;
	lpa->segmentsize = L4INFOLEN / 4 * (ppa->hwsector);

	lpa->mode = ppa->mode;
	lpa->pc = 0;

	return 0;
}

static void start(int handle)
{
	PManager *pm = (PManager*)handle;
	struct NotifyList *nl;
	struct singlelist *it;

	if(-1 == vNand_Init(&pm->vnand)){
		ndprint(PARTITION_ERROR, "ERROR:vNand_Init error  func %s line %d\n",__FUNCTION__,__LINE__);
		return;
	}

#ifndef TEST_PARTITION
	if (-1 == SplBlockManager_Init(pm)){
		ndprint(PARTITION_ERROR, "ERROR:SimpleBlockManager_Init failed\n");
		return;
	}
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

/**
 * Please open the partition before you
 * operate(read,write,ioctrl,close) the partition.
 * @name: The partition name you want to open
 * @mode: DIRECT_MANAGER ----0
 *        ZONE_MANAGER ------1
 **/
int NandManger_ptOpen(int handle, const char* name, int mode)
{
	PManager *pm = (PManager*)handle;
	struct singlelist *pos;
	struct singlelist *lpos;
	LPartition *lp;
	ManagerList *ml;
	PartitionInterface *pInterface = NULL;
	PPartition  *ppartition;
	PartContext *pcontext;
	VNandInfo *vInfo;
	int i, count, nmhandle = 0, name_exit = 0;

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
		ndprint(PARTITION_ERROR, "ERROR:FUNCTION:%s  LINE:%d  The partition name is not exit , it can't be open!\n",
				__FUNCTION__,__LINE__);
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
				ndprint(PARTITION_ERROR, "ERROR:FUNCTION:%s  LINE:%d \n The partition open failed! \n",
						__FUNCTION__,__LINE__);
				return 0;
			}
			break;
		}
	}

	/*fill the PartContext structure lp->pc*/
	pcontext = Nand_VirtualAlloc(sizeof(PartContext));
	pcontext->nmhandle = nmhandle;
	pcontext->ptif = pInterface;
	lp->pc = (int)pcontext;
	ndprint(PARTITION_INFO, "Nand manager interface open ok !\n");
	return lp->pc;
}

int NandManger_ptRead(int context, SectorList* bl)
{
	PartContext *pcontext = (PartContext*)context;
	return pcontext->ptif->PartitionInterface_Read(pcontext->nmhandle,bl);
}

int NandManger_ptWrite(int context, SectorList* bl)
{
	PartContext *pcontext = (PartContext*)context;
	return pcontext->ptif->PartitionInterface_Write(pcontext->nmhandle,bl);
}

int NandManger_ptIoctrl(int context, int cmd, int args)
{
	PartContext *pcontext = (PartContext*)context;
	return pcontext->ptif->PartitionInterface_Ioctrl(pcontext->nmhandle,cmd,args);
}

int NandManger_ptErase(int context)
{
	PartContext *pcontext = (PartContext*)context;
	return pcontext->ptif->PartitionInterface_Erase(pcontext->nmhandle);
}

int NandManger_ptClose(int context)
{
	PartContext *pcontext = (PartContext*)context;
	return pcontext->ptif->PartitionInterface_iClose(pcontext->nmhandle);
}

/**
 * Get the logical partition info
 * and fill the LPartition structure
 **/
int NandManger_getPartition(int handle, LPartition** pt)
{
	PManager *pm = (PManager*)handle;
	PPartition *ppa = NULL;
	LPartition *lpa = NULL;
	int i, count, ret = -1;

	ppa = pm->vnand->pt->ppt;
	count = pm->vnand->pt->ptcount;

	for (i=0; i<count; i++){
		if(lpa == NULL){
			lpa = (LPartition *)BuffListManager_getTopNode(pm->blm, sizeof(LPartition));
			pm->lpt.pt = lpa;
			*pt = pm->lpt.pt;
		}else
			lpa = (LPartition *)BuffListManager_getNextNode(pm->blm,(void*)lpa,sizeof(LPartition));

		if(lpa){
			if ( -1 == (ret = p2lPartition((ppa+i), lpa)) ){
				ndprint(PARTITION_ERROR, "ERROR:FUNCTION: %s  LINE: %d  Get partition%d failed !\n",
						__FUNCTION__, __LINE__,i);
				return -1;
			}
		}else{
			ndprint(PARTITION_ERROR,"ERROR:func: %s line: %d  Alloc memory error !\n",__func__,__LINE__);
			return -1;
		}
	}

	ndprint(PARTITION_INFO, "Nand manager interface get partition ok!\n");
	return 0;
}

/**
 * If you want to use nand manager layer,
 * please registe the PartitionInterface
 * and the mode first.
 **/
int NandManger_Register_Manager(int handle, int mode, PartitionInterface* pi)
{
	PManager *pm = (PManager*)handle;
	ManagerList *mlist = pm->Mlist;

	if (pi == NULL){
		ndprint(PARTITION_ERROR, "ERROR:PartitionInterface is NULL, %s(%d)\n", __FUNCTION__, __LINE__);
		return -1;
	}

	if (mlist == NULL){
		mlist = (ManagerList*)BuffListManager_getTopNode(pm->blm, sizeof(ManagerList));
		pm->Mlist = mlist;
	}
	else
		mlist = (ManagerList*)BuffListManager_getNextNode(pm->blm,(void*)mlist,sizeof(ManagerList));

	if (mlist){
		mlist -> mode = mode;
		mlist -> nmi = pi;
	}
	else{
		ndprint(PARTITION_ERROR, "ERROR:Nand alloc continue memory error, %s(%d)\n", __FUNCTION__, __LINE__);
		return -1;
	}

	ndprint(PARTITION_INFO, "Nand manager interface register ok!\n");
	return 0;
}

/**
 *Alloc the memory and do init
 **/
int NandManger_Init(void){
	PManager *pm;

	pm = Nand_VirtualAlloc(sizeof(PManager));
	if (pm == NULL){
		ndprint(PARTITION_ERROR, "ERROR:Nand alloc continue memory error, %s(%d)\n",
				__FUNCTION__, __LINE__);
		return 0;
	}

	if (!(pm->blm = BuffListManager_BuffList_Init())){
		ndprint(PARTITION_ERROR, "ERROR:BuffListManager_BuffList_Init failed\n");
		return 0;
	}

	pm->vnand = NULL;
	pm->Mlist = NULL;
	pm->nl = NULL;

	Register_StartNand(start, (int)pm);

	ndprint(PARTITION_INFO, "Nand manager interface init  ok !\n");
	return (int)pm;
}

/**
 *Free the alloced memory and do deinit
 **/
void NandManger_DeInit(int handle){
	PManager *pm = (PManager*)handle;

	vNand_Deinit(&pm->vnand);
#ifndef TEST_PARTITION
	SimpleBlockManager_Deinit(0);
	L2PConvert_Deinit(0);
#endif
	Nand_VirtualFree((PartContext *)pm->lpt.pt->pc);
	BuffListManager_freeAllList(pm->blm, (void**)(&(pm->lpt.pt)), sizeof(LPartition));
	BuffListManager_freeAllList(pm->blm,(void**)(&(pm->nl)), sizeof(struct NotifyList));
	BuffListManager_freeAllList(pm->blm, (void**)(&(pm->Mlist)), sizeof(ManagerList));
	BuffListManager_BuffList_DeInit (pm->blm);
	Nand_VirtualFree(pm);
}

void NandManger_startNotify(int handle, void (*start)(int), int prdata)
{
	PManager *pm = (PManager*)handle;
	struct NotifyList* nlist = pm->nl;

	if (nlist == NULL){
		nlist = (struct NotifyList*)BuffListManager_getTopNode(pm->blm,
															   sizeof(struct NotifyList));
		pm->nl = nlist;
	} else
		nlist = (struct NotifyList*)BuffListManager_getNextNode(pm->blm,
																(void*)nlist,
																sizeof(struct NotifyList));

	if (nlist) {
		nlist->start = start;
		nlist->prdata = prdata;
	} else {
		ndprint(PARTITION_ERROR, "ERROR:func: %s line: %d.ALLOC MEMORY FAILED!\n", __func__, __LINE__);
		while(1);
	}
}

PPartArray* NandManger_getDirectPartition(int handle)
{
	PManager *pm = (PManager*)handle;

	return (pm && pm->vnand) ? pm->vnand->pt : NULL;
}

int NandManger_DirectRead(int handle, PPartition *pt, int pageid, int off_t, int bytes, void *data)
{
	return vNand_DirectPageRead(pt, pageid, off_t, bytes, data);
}

int NandManger_DirectWrite(int handle, PPartition *pt, int pageid, int off_t, int bytes, void *data)
{
	return vNand_DirectPageWrite(pt, pageid, off_t, bytes, data);
}

int NandManger_DirectErase(int handle, PPartition *pt, BlockList* bl)
{
	return vNand_DirectMultiBlockErase(pt, bl);
}

int NandManger_DirectIsBadBlock(int handle, PPartition *pt, int blockid)
{
	return vNand_DirectIsBadBlock(pt, blockid);
}

int NandManger_DirectMarkBadBlock(int handle, PPartition *pt, int blockid)
{
	return vNand_DirectMarkBadBlock(pt, blockid);
}

int NandManger_UpdateErrorPartition(int handle, PPartition *pt)
{
	PManager *pm = (PManager*)handle;
	return vNand_UpdateErrorPartition(pm->vnand, pt);
}
