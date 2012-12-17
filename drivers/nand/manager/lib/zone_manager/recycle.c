#include "os/clib.h"
#include "recycle.h"
#include "hash.h"
#include "pagelist.h"
#include "pageinfo.h"
#include "zone.h"
#include "cachemanager.h"
#include "nanddebug.h"
#include "zonemanager.h"
#include "os/NandSemaphore.h"
#include "bufflistmanager.h"
#include "context.h"
#include "vNand.h"
#include "l2vNand.h"
#include "taskmanager.h"
#include "nanddebug.h"
#include "nandsigzoneinfo.h"
#include "utils/nmbitops.h"
#include "pageinfodebug.h"
//#include "badblockinfo.h"

#define FIRST_PAGEINFO(vnand) ((vnand)->v2pp->_2kPerPage * 2)
#define L4UNITSIZE(context) ( L4INFOLEN / 4 * SECTOR_SIZE)
#define RECYCLECACHESIZE		VNANDCACHESIZE

static int getRecycleZone ( Recycle *rep);
static int FindFirstPageInfo ( Recycle *rep);
static int FindValidSector ( Recycle *rep);
//static int MergerSectorID ( Recycle *rep);
static int RecycleReadWrite ( Recycle *rep);
static int FindNextPageInfo ( Recycle *rep);
static int FreeZone ( Recycle *rep );
int Recycle_OnForceRecycle ( int frinfo );
static int MergerSectorID_Align(Recycle *rep);
/**
 *	Recycle_OnFragmentHandle - Process of normal recycle
 *
 *	@context: global variable
 */
#define WAIT_L2PCONVERT 0
int Recycle_OnFragmentHandle ( int context )
{
	int ret = 0;
	Context *conptr = (Context *)context;
	Recycle *rep = conptr->rep;

	switch(rep->taskStep) {
		case RECYSTART:
		case GETZONE:
			NandMutex_Lock(&rep->mutex);
			ret = getRecycleZone(rep);
			NandMutex_Unlock(&rep->mutex);
			if (ret == -1)
				return ret;
			break;
		case READFIRSTINFO:
			NandMutex_Lock(&rep->mutex);
			if (WAIT_L2PCONVERT && nd_getcurrentsec_ns() < (conptr->t_startrecycle + INTERNAL_TIME)) {
				NandMutex_Unlock(&rep->mutex);
				break;
			}
			ret = FindFirstPageInfo(rep);
			NandMutex_Unlock(&rep->mutex);
			if (ret == -1)
				goto ERROR;
			break;
		case FINDVAILD:
			NandMutex_Lock(&rep->mutex);
			if (WAIT_L2PCONVERT && nd_getcurrentsec_ns() < (conptr->t_startrecycle + INTERNAL_TIME)) {
				NandMutex_Unlock(&rep->mutex);
				break;
			}
			ret = FindValidSector(rep);
			if (ret == -1)
				goto ERROR;
			break;
		case MERGER:
			MergerSectorID_Align(rep);
			//MergerSectorID(rep);
			break;
		case RECYCLE:
			ret = RecycleReadWrite(rep);
			NandMutex_Unlock(&rep->mutex);
			if (ret == -1)
				goto ERROR;
			break;
		case READNEXTINFO:
			NandMutex_Lock(&rep->mutex);
			if (WAIT_L2PCONVERT && nd_getcurrentsec_ns() < (conptr->t_startrecycle + INTERNAL_TIME)) {
				NandMutex_Unlock(&rep->mutex);
				break;
			}
			ret = FindNextPageInfo(rep);
			NandMutex_Unlock(&rep->mutex);
			if (ret == -1)
				goto ERROR;
			break;
		case FINISH:
			NandMutex_Lock(&rep->mutex);
			if (WAIT_L2PCONVERT && nd_getcurrentsec_ns() < (conptr->t_startrecycle + INTERNAL_TIME)) {
				NandMutex_Unlock(&rep->mutex);
				break;
			}
			rep->write_pagecount = rep->rZone->sumpage - rep->write_pagecount;
			ret = FreeZone(rep);
			NandMutex_Unlock(&rep->mutex);
			if (ret == -1)
				goto ERROR;
			break;
		case RECYIDLE:
			break;
		default:
			break;
	}
ERROR:
	return ret;
}

/**
 *	alloc_pageinfo  -  alloc L1Info, L2Info, L3Info and L4Info of pageinfo
 *
 *	@rep: to konw whether L2InfoLen and L3InfoLen are 0 or not
 */
static int alloc_pageinfo(int context, PageInfo *pi)
{
	int i;
	Context *conptr = (Context *)context;

	for (i = 0; i < 2; i++) {
		if (conptr->L2InfoLen) {
			pi[i].L2Info = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * conptr->L2InfoLen);
			if (!(pi[i].L2Info)) {
				ndprint(RECYCLE_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
				if (i == 0)
					goto ERROR1;
				else
					goto ERROR4;
			}
		}

		if (conptr->L3InfoLen) {
			pi[i].L3Info = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * conptr->L3InfoLen);
			if (!(pi[i].L3Info)) {
				ndprint(RECYCLE_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
				if (i == 0)
					goto ERROR2;
				else
					goto ERROR5;
			}
		}

		pi[i].L4Info = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * L4INFOLEN);
		if (!(pi[i].L4Info)) {
			ndprint(RECYCLE_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
			if (i == 0)
				goto ERROR3;
			else
				goto ERROR6;
		}

		pi[i].L1InfoLen = conptr->l1info->len;
		pi[i].L2InfoLen = conptr->L2InfoLen;
		pi[i].L3InfoLen = conptr->L3InfoLen;
		pi[i].L4InfoLen = L4INFOLEN;
	}

	return 0;

ERROR6:
	if (conptr->L3InfoLen)
		Nand_VirtualFree(pi[1].L3Info);
ERROR5:
	if (conptr->L2InfoLen)
		Nand_VirtualFree(pi[1].L2Info);
ERROR4:
	Nand_VirtualFree(pi[0].L4Info);
ERROR3:
	if (conptr->L3InfoLen)
		Nand_VirtualFree(pi[0].L3Info);
ERROR2:
	if (conptr->L2InfoLen)
		Nand_VirtualFree(pi[0].L2Info);
ERROR1:
	return -1;
}

/**
 *	free_pageinfo  -  free L1Info, L2Info, L3Info and L4Info of pageinfo
 *
 *	@rep: operate object
 */
static void free_pageinfo(int context, PageInfo *pi)
{
	int i;
	Context *conptr = (Context *)context;

	for (i = 0; i < 2; i++) {
		if (conptr->L2InfoLen)
			Nand_VirtualFree(pi[i].L2Info);

		if (conptr->L3InfoLen)
			Nand_VirtualFree(pi[i].L3Info);

		Nand_VirtualFree(pi[i].L4Info);
	}
}

/**
 *	alloc_normalrecycle_memory - alloc normal recycle memory
 *
 *	@rep: operate object
 */
static int alloc_normalrecycle_memory(Recycle *rep)
{
	rep->record_writeadd = (unsigned int *)Nand_VirtualAlloc(L4INFOLEN);
	if(rep->record_writeadd == NULL) {
		ndprint(RECYCLE_ERROR,"Force recycle alloc error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}

	return alloc_pageinfo(rep->context,rep->pi);
}

/**
 *	free_normalrecycle_memory - alloc normal recycle memory
 *
 *	@rep: operate object
 */
static void free_normalrecycle_memory(Recycle *rep)
{
	Nand_VirtualFree(rep->record_writeadd);
	free_pageinfo(rep->context,rep->pi);
}

/**
 *	alloc_forcerecyle_memory - alloc force recycle memory
 *
 *	@rep: operate object
 */
static int alloc_forcerecycle_memory( Recycle *rep )
{
	rep->force_record_writeadd = (unsigned int *)Nand_VirtualAlloc(L4INFOLEN);
	if(rep->force_record_writeadd == NULL) {
		ndprint(RECYCLE_ERROR,"Force recycle alloc error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}

	return alloc_pageinfo(rep->context,rep->force_pi);
}

/**
 *	free_forcerecycle_memory - free force recycle memory
 *
 *	@rep: operate object
 */
static void free_forcerecycle_memory( Recycle *rep )
{
	Nand_VirtualFree(rep->force_record_writeadd);
	free_pageinfo(rep->context,rep->force_pi);
}

/**
 *	Recycle_OnNormalRecycle - normal recycle
 *
 *	@context: global variable
 */
int Recycle_OnNormalRecycle ( int context )
{
	int ret = 0;
	Context *conptr = (Context *)context;
	Recycle *rep = conptr->rep;

	NandMutex_Lock(&rep->mutex);
	ndprint(RECYCLE_INFO, "\n%s: start normal recycle--------->\n"
			,((PPartition *)(conptr->vnand.prData))->name);
	if (rep->taskStep == RECYIDLE)
		rep->taskStep = RECYSTART;
	NandMutex_Unlock(&rep->mutex);
	rep->write_pagecount = 0;
	while(1) {
		if (ret == -1 || rep->taskStep == RECYIDLE)
			break;

		ret = Recycle_OnFragmentHandle(context);
	}


	NandMutex_Lock(&rep->mutex);
	if(ret != 0) {
		if(rep->rZone)
			Drop_JunkZone(((Context *)rep->context)->junkzone,rep->rZone->ZoneID);
	}
	rep->rZone = NULL;
	rep->taskStep = RECYIDLE;
	ndprint(RECYCLE_INFO, "%s: normal recycle finished recycle pagecount = %d--------->\n\n"
			,((PPartition *)(conptr->vnand.prData))->name,rep->write_pagecount);
	NandMutex_Unlock(&rep->mutex);

	return ret;
}

/**
 *	get_pageinfo_from_cache - alloc pageinfo
 *
 *	@rep: operate object
 *	@sector: number of sector
 */
static PageInfo *get_pageinfo_from_cache(Recycle *rep,unsigned int sector)
{
	PageInfo *pi = NULL;

	CacheManager_lockCache((int)((Context *)rep->context)->cachemanager, sector, &pi);

	return pi;
}

/**
 *	give_pageinfo_to_cache - free pageinfo
 *
 *	@rep: operate object
 *	@pi: which need to free
 */
static void give_pageinfo_to_cache(Recycle *rep,PageInfo *pi)
{

	CacheManager_unlockCache((int)((Context *)rep->context)->cachemanager, pi);
}

/**
 *	get_force_zoneID - get a force recycle zoneID
 *
 *	@context: global variable
 */
static unsigned short get_force_zoneID(int context)
{
	unsigned int count = 0;
	unsigned int lifetime = 0;

	count = ZoneManager_Getusedcount(context);
	if(count == 0) {
		ndprint(RECYCLE_ERROR,"PANIC ERROR func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}

	lifetime = ZoneManager_Getminlifetime(context);

	return ZoneManager_ForceRecyclezoneID(context,lifetime + 1);
}

/**
 *	get_normal_zoneID - get a normal recycle zoneID
 *
 *	@context: global variable
 */
static unsigned short get_normal_zoneID(int context)
{
	unsigned int count = 0;
	unsigned int minlifetime = 0;
	unsigned int maxlifetime = 0;
	unsigned int lifetime = 0;
	unsigned int difflifetime;
	unsigned short zoneid = -1;
	count = ZoneManager_Getusedcount(context);
	if(count == 0) {
		ndprint(RECYCLE_ERROR,"PANIC ERROR func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}

	minlifetime = ZoneManager_Getminlifetime(context);
	maxlifetime = ZoneManager_Getmaxlifetime(context);
	difflifetime = maxlifetime - minlifetime;
	lifetime = minlifetime + difflifetime / 3;
	if(difflifetime > BALANCECOUNT / 2)
		zoneid = ZoneManager_RecyclezoneID(context,lifetime);
	if(difflifetime > BALANCECOUNT && zoneid == 0xffff)
		zoneid = ZoneManager_RecyclezoneID(context,0);
	return zoneid;
}

/**
 *	getRecycleZone - get a normal recycle zone
 *
 *	@rep: operate object
 */
static int getRecycleZone ( Recycle *rep)
{
	unsigned short ZoneID = 0;
	int ret = 0;
	int context = rep->context;
	Zone *zone;
	Zone *current_zone;
	//min sector count 128 sectors
	ZoneID = Get_JunkZoneRecycleZone(((Context *)context)->junkzone);//Get_MaxJunkZone(((Context *)context)->junkzone,128);
	rep->junk_zoneid = ZoneID;
	if(ZoneID == 0xffff){
		ZoneID = get_normal_zoneID(context);
	}

	if(ZoneID == 0xffff) {
		ret = -1;
		ndprint(RECYCLE_INFO,"ZoneID == 0xffff ! func %s line %d\n",
				__FUNCTION__,__LINE__);
		goto err;
	}

	current_zone = ZoneManager_GetCurrentWriteZone(rep->context);
	if(current_zone && (ZoneID == current_zone->ZoneID)){
		ndprint(RECYCLE_INFO,"Start handle recyclezone=currentzone!!! func %s line %d ZoneID = %d\n",
				__FUNCTION__,__LINE__,ZoneID);
		zone = ZoneManager_Get_Used_Zone(((Context*)(rep->context))->zonep, ZoneID);
		if(zone == NULL) {
			ndprint(RECYCLE_ERROR,"alloc recycle zone error func %s line %d ZoneID = %d\n",
					__FUNCTION__,__LINE__,ZoneID);
			ret = -1;
			goto err;
		}
		ZoneManager_SetPrevZone(rep->context,zone);
		ZoneManager_FreeZone (rep->context,zone);
	}

	rep->rZone = ZoneManager_AllocRecyclezone(context,ZoneID);
	if(rep->rZone == NULL) {
		ndprint(RECYCLE_ERROR,"alloc recycle zone error func %s line %d ZoneID = %d\n",
				__FUNCTION__,__LINE__,ZoneID);
		ret = -1;
		goto err;
	}
	rep->taskStep = READFIRSTINFO;
err:
	return ret;
}

/**
 *	is_same_L4 - whether L4 is same or not
 *
 *	@prev_pi: previous pageinfo
 *	@next_pi: next pageinfo
 */
static int is_same_L4(PageInfo *prev_pi, PageInfo *next_pi)
{
	if (prev_pi->L3InfoLen) {
		if (prev_pi->L2InfoLen)
			return prev_pi->L1Index != 0xffff
				&& prev_pi->L1Index == next_pi->L1Index
				&& prev_pi->L2Index == next_pi->L2Index
				&& prev_pi->L3Index == next_pi->L3Index;
		else
			 return prev_pi->L1Index != 0xffff
				&& prev_pi->L1Index == next_pi->L1Index
				&& prev_pi->L3Index == next_pi->L3Index;
	}
	else
		return prev_pi->L1Index != 0xffff
			&& prev_pi->L1Index == next_pi->L1Index;
}

/**
 *	FindFirstPageInfo - Find first pageinfo of recycle zone
 *
 *	@rep: operate object
 */
static int FindFirstPageInfo ( Recycle *rep)
{
	int ret = 0;
	PageInfo *prev_pi = rep->pi;
	PageInfo *pi = rep->pi + 1;

	ret = Zone_FindFirstPageInfo(rep->rZone,prev_pi);
	if (ISERROR(ret)) {
		ndprint(RECYCLE_ERROR,"Find First Pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
		goto err0;
	}

	rep->curpageinfo = prev_pi;
	do {
		if (rep->rZone->NextPageInfo == 0) {
			rep->end_findnextpageinfo = 1;
			goto exit;
		}
		else if (rep->rZone->NextPageInfo == 0xffff) {
			ndprint(RECYCLE_ERROR,"pageinfo data error func %s line %d \n",
						__FUNCTION__,__LINE__);
			goto err0;
		}

		ret = Zone_FindNextPageInfo(rep->rZone,pi);
		if (ISNOWRITE(ret)) {
			rep->end_findnextpageinfo = 1;
			goto exit;
		}
		else if(ret < 0) {
			ndprint(RECYCLE_ERROR,"find next pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
			goto err0;
		}

		rep->prevpageinfo = rep->curpageinfo;
		rep->curpageinfo = pi;

		if (pi == rep->pi)
			pi = rep->pi + 1;
		else if (pi == rep->pi + 1)
			pi = rep->pi;
	}while(is_same_L4(rep->prevpageinfo, rep->curpageinfo));

	rep->nextpageinfo = rep->curpageinfo;
	rep->curpageinfo = rep->prevpageinfo;

exit:
	rep->taskStep = FINDVAILD;
	return 0;

err0:
	return -1;
}

/**
 *	 FindValidSector - Find valid sector of recycle zone
 *
 *	 @rep: operate object
 */
static int FindValidSector ( Recycle *rep)
{
	unsigned short l3index = 0;
	unsigned short l2index = 0;
	unsigned short l1index = 0;
	unsigned int l1unitlen = 0;
	unsigned int l2unitlen = 0;
	unsigned int l3unitlen = 0;
	unsigned int start_sectorID = 0;
	Context *conptr = (Context *)(rep->context);
	CacheManager *cachemanager = conptr->cachemanager;
	PageInfo *pi = NULL;
	l1unitlen = cachemanager->L1UnitLen;
	l2unitlen = cachemanager->L2UnitLen;
	l3unitlen = cachemanager->L3UnitLen;

	pi = rep->curpageinfo;
	l1index = pi->L1Index;
	l2index = pi->L2Index;
	l3index = pi->L3Index;

	if (l1index == 0xffff) {
		ndprint(RECYCLE_ERROR,"PANIC ERROR l1index = -1, func %s line %d \n",__FUNCTION__,__LINE__);
		goto FindValidSector_err;
	}
	if(pi->L3InfoLen == 0)
		start_sectorID = l1index * l1unitlen;
	else {
		if (l3index == 0xffff) {
			ndprint(RECYCLE_ERROR,"PANIC ERROR l3index = -1, func %s line %d \n",__FUNCTION__,__LINE__);
			goto FindValidSector_err;
		}

		if(pi->L2InfoLen == 0)
			start_sectorID = l1index * l1unitlen + l3index * l3unitlen;
		else {
			if (l2index == 0xffff) {
				ndprint(RECYCLE_ERROR,"PANIC ERROR l2index = -1, func %s line %d \n",__FUNCTION__,__LINE__);
				goto FindValidSector_err;
			}
			start_sectorID = l3index * l3unitlen +
				l2index * l2unitlen + l1index * l1unitlen;
		}
	}

	rep->startsectorID = start_sectorID;
	rep->writepageinfo = get_pageinfo_from_cache(rep,rep->startsectorID);
	rep->taskStep = MERGER;
	return 0;
FindValidSector_err:
	return -1;
}

/**
 *	data_in_rzone - whether L4 data in recycle zone or not
 *
 *	@rep: operate object
 *	@pageid: number of page
*/
static int data_in_rzone (Recycle *rep, unsigned int pageid)
{
	Zone *zone;
	VNandInfo *vnand = NULL;
	unsigned short zoneid;
	int ppb;
	ZoneManager *zonep = ((Context *)(rep->context))->zonep;

	if (rep->force)
		zone = rep->force_rZone;
	else
		zone = rep->rZone;

	vnand = zone->vnand;
	zoneid = zone->ZoneID;
	ppb = vnand->PagePerBlock;

	if (zoneid == zonep->pt_zonenum - 1) {
		//return pageid >= BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,zoneid) * ppb;
		return pageid >= (zoneid * BLOCKPERZONE(zonep->vnand)) * ppb;
	}

	/*return pageid >= BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,zoneid) * ppb
	  && pageid < BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,zoneid + 1) * ppb;*/

	return pageid >= (zoneid * BLOCKPERZONE(zonep->vnand)) * ppb
		&& pageid < ((zoneid + 1) * BLOCKPERZONE(zonep->vnand)) * ppb;
}

static int data_in_wzone (Recycle *rep, unsigned int pageid)
{
	Zone *zone = ZoneManager_GetCurrentWriteZone(rep->context);
	VNandInfo *vnand = NULL;
	unsigned short zoneid;
	int ppb;
	ZoneManager *zonep = ((Context *)(rep->context))->zonep;
	if(!zone)
		return 0;
	vnand = zone->vnand;
	zoneid = zone->ZoneID;
	ppb = vnand->PagePerBlock;

	if (zoneid == zonep->pt_zonenum - 1) {
		//return pageid >= BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,zoneid) * ppb;
		return pageid >= (zoneid * BLOCKPERZONE(zonep->vnand)) * ppb;
	}

	/*return pageid >= BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,zoneid) * ppb
	  && pageid < BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,zoneid + 1) * ppb;*/

	return pageid >= (zoneid * BLOCKPERZONE(zonep->vnand)) * ppb
		&& pageid < ((zoneid + 1) * BLOCKPERZONE(zonep->vnand)) * ppb;
}

/**
 *	data_in_prev_zone - whether L4 data in previous of recycle zone or not
 *
 *	@rep: operate object
 *	@pageid: number of page
*/
static int data_in_prev_zone (Recycle *rep, unsigned int pageid)
{
	Zone *zone = NULL;
	VNandInfo *vnand = NULL;
	unsigned short zoneid;
	int ppb;
	ZoneManager *zonep = ((Context *)(rep->context))->zonep;

	if (rep->force)
		zone = rep->force_rZone;
	else
		zone = rep->rZone;

	if (!zone->prevzone)
		return 0;

	vnand = zone->vnand;
	zoneid = zone->prevzone- zone->top;
	ppb = vnand->PagePerBlock;

	if (zoneid == zonep->pt_zonenum - 1) {
		//return pageid >= BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,zoneid) * ppb;
		return pageid >= (zoneid * BLOCKPERZONE(zonep->vnand)) * ppb;
	}

	/*return pageid >= BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,zoneid) * ppb
	  && pageid < BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,zoneid + 1) * ppb;*/

	return pageid >= (zoneid * BLOCKPERZONE(zonep->vnand)) * ppb
		&& pageid < ((zoneid + 1) * BLOCKPERZONE(zonep->vnand)) * ppb;
}

/**
 *	data_in_next_zone - whether L4 data in next of recycle zone or not
 *
 *	@rep: operate object
 *	@pageid: number of page
*/
static int data_in_next_zone (Recycle *rep, unsigned int pageid)
{
	Zone *zone = NULL;
	VNandInfo *vnand = NULL;
	unsigned short zoneid;
	int ppb;
	ZoneManager *zonep = ((Context *)(rep->context))->zonep;

	if (rep->force)
		zone = rep->force_rZone;
	else
		zone = rep->rZone;

	if (!zone->nextzone || zone->ZoneID == ZoneManager_GetCurrentWriteZone(rep->context)->ZoneID);
		return 0;

	vnand = zone->vnand;
	zoneid = zone->nextzone- zone->top;
	ppb = vnand->PagePerBlock;

	if (zoneid == zonep->pt_zonenum - 1) {
		//return pageid >= BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,zoneid) * ppb;
		return pageid >= (zoneid * BLOCKPERZONE(zonep->vnand)) * ppb;
	}

	/*return pageid >= BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,zoneid) * ppb
	  && pageid < BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,zoneid + 1) * ppb;*/

	return pageid >= (zoneid * BLOCKPERZONE(zonep->vnand)) * ppb
		&& pageid < ((zoneid + 1) * BLOCKPERZONE(zonep->vnand)) * ppb;
}

/**
 *	data_in_3_zone - whether L4 data in previous, current or next of recycle zone or not
 *
 *	@rep: operate object
 *	@pageid: number of page
*/
static int data_in_3_zone (Recycle *rep, unsigned int pageid)
{
	return data_in_rzone(rep, pageid)
		|| data_in_prev_zone(rep, pageid)
		|| data_in_next_zone(rep, pageid);
}
#if 0
static int align_sectors(Recycle *rep,int sectorcount) {
	Context *conptr = (Context *)(rep->context);
	VNandInfo *vnand = &conptr->vnand;

	int alignsectorcount = 0;
	//first - 2k pageinfo sector
	alignsectorcount = (vnand->BytePerPage * vnand->v2pp->_2kPerPage - vnand->BytePerPage) / SECTOR_SIZE;
	if(sectorcount > alignsectorcount) {
		sectorcount = sectorcount - alignsectorcount;
		alignsectorcount = (vnand->BytePerPage * vnand->v2pp->_2kPerPage) / SECTOR_SIZE;
	}
	sectorcount = sectorcount % alignsectorcount;
	return sectorcount;
}
static void FilluptoAlign ( Recycle *rep,PageList *tpl,int *record_writeaddr,int sectorcount) {

	unsigned int *latest_l4info;
	unsigned int tmp0 ,tmp1;
	unsigned int spp;
	int i,k;
	unsigned int l4count;
	int oldzoneid;
	int blm = ((Context *)(rep->context))->blm;
	int jzone = ((Context *)(rep->context))->junkzone;
	if(!rep->force) {
		spp = rep->rZone->vnand->BytePerPage / SECTOR_SIZE;
		l4count = rep->curpageinfo->L4InfoLen >> 2;
		latest_l4info = (unsigned int *)(rep->writepageinfo->L4Info);
	} else {
		spp = rep->force_rZone->vnand->BytePerPage / SECTOR_SIZE;
		l4count = rep->force_curpageinfo->L4InfoLen >> 2;
		latest_l4info = (unsigned int *)rep->force_writepageinfo->L4Info;
	}

	sectorcount = align_sectors(rep,sectorcount);
	i = 0;
	while(sectorcount && i < l4count) {
		if ((int)latest_l4info[i] == -1) {
			i++;
			continue;
		}
		if(record_writeaddr[i] == 0){
			i++;
			continue;
		}
		k = 0;
		tmp0 = latest_l4info[i];
		tmp1 = tmp0;
		while(i < l4count) {
			if(record_writeaddr[i] == 0)
				break;
			if(tmp0++ == latest_l4info[i++])
				k++;
			else
				break;
			if(tmp0 % spp == 0) break;
		}
		if(k > sectorcount) {
			k = sectorcount;
		}
		if(k > 0) {
			tpl = (PageList *)BuffListManager_getNextNode(blm, (void *)tpl, sizeof(PageList));
			tpl->startPageID = tmp1 / spp;
			tpl->OffsetBytes = (tmp1 % spp) * SECTOR_SIZE;
			tpl->Bytes = k * SECTOR_SIZE;
			tpl->retVal = 0;
			tpl->pData = NULL;
			sectorcount -= k;
			oldzoneid = ZoneManager_convertPageToZone(rep->context,tpl->startPageID);
			Insert_JunkZone(jzone,k,oldzoneid);
		}
	}
}
#endif
static int MergerSectorID_Align(Recycle *rep) {
	unsigned int *latest_l4info;
	int spp;
	unsigned int l4count;
	unsigned int *record_writeaddr;
	int n,k,i;
	Context *conptr = (Context *)rep->context;
	int freezonecount = ZoneManager_Getfreecount(rep->context);
	VNandInfo *vnand =  &conptr->vnand;
	int wsectors = 0;
	unsigned int *l4info;
	unsigned int tmp0;
	unsigned int tmp1;
	PageList *pl = NULL;
	PageList *tpl = NULL;
	int sectorcount = 0;
	int alignsectorcount = 0;
	int blm = conptr->blm;
	int oldzoneid;

	if(!rep->force) {
		if(!rep->curpageinfo) return 0;
		spp = rep->rZone->vnand->BytePerPage / SECTOR_SIZE;
		l4count = rep->curpageinfo->L4InfoLen >> 2;
		latest_l4info = (unsigned int *)(rep->writepageinfo->L4Info);
		record_writeaddr = rep->record_writeadd;
		l4info = (unsigned int *)(rep->curpageinfo->L4Info);

	} else {
		if(rep->force_curpageinfo == NULL)
			return 0;
		spp = rep->force_rZone->vnand->BytePerPage / SECTOR_SIZE;
		l4count = rep->force_curpageinfo->L4InfoLen >> 2;
		latest_l4info = (unsigned int *)rep->force_writepageinfo->L4Info;
		record_writeaddr = rep->force_record_writeadd;
		l4info = (unsigned int *)(rep->force_curpageinfo->L4Info);

	}
	memset(record_writeaddr, 0xff, l4count * 4);
	n = 0;
	while(n < l4count) {
		if(l4info[n] == -1){
			n++;
			continue;
		}
		tmp0 = l4info[n] / spp;
		tmp1 = latest_l4info[n] / spp;
		if(!rep->force && rep->recyclemode == RECYCLE_NORMAL_MODE) {
			if((tmp0 == tmp1) && data_in_3_zone(rep, tmp0)) {
				record_writeaddr[n] = latest_l4info[n];
				sectorcount++;
			}
		}else{
			if((tmp0 == tmp1) && data_in_rzone(rep, tmp0)) {
				record_writeaddr[n] = latest_l4info[n];
				sectorcount++;
			}
		}
		n++;

	}

	//first - 2k pageinfo sector
	alignsectorcount = (vnand->BytePerPage * vnand->v2pp->_2kPerPage - vnand->BytePerPage) / SECTOR_SIZE;
	if(sectorcount > alignsectorcount) {
		sectorcount = sectorcount - alignsectorcount;
		alignsectorcount = (vnand->BytePerPage * vnand->v2pp->_2kPerPage) / SECTOR_SIZE;
	}
	sectorcount = sectorcount % alignsectorcount;
	alignsectorcount = (vnand->BytePerPage * vnand->v2pp->_2kPerPage) / SECTOR_SIZE;
	n = 0;
	while(n < l4count && sectorcount) {
		if(latest_l4info[n] == -1) {
			n++;
			continue;
		}
		if(record_writeaddr[n] != -1) {
			n++;
			continue;
		}
		if(!data_in_wzone(rep,latest_l4info[n] / spp)){
			wsectors++;
		}
		n++;
	}
	if(wsectors > 0) {
		if(freezonecount > 9) {
			wsectors = wsectors - sectorcount;
			if(wsectors > 0) {
				wsectors = wsectors / alignsectorcount * alignsectorcount + wsectors;
			}else
				wsectors = sectorcount;
		}else{
			wsectors = sectorcount;
		}
		n = 0;
		while(n < l4count && wsectors) {
			if(latest_l4info[n] == -1) {
				n++;
				continue;
			}
			if(record_writeaddr[n] != -1) {
				n++;
				continue;
			}
			if(!data_in_wzone(rep,latest_l4info[n] / spp)){
				record_writeaddr[n] = latest_l4info[n];
				wsectors--;
			}
			n++;
		}
	}
	n = 0;
	while(n < l4count) {
		if(record_writeaddr[n] == -1) {
				n++;
				continue;
		}
		tmp0 = record_writeaddr[n];
		tmp1 = tmp0;
		k = 1;
		n++;
		for(i = 1;i < spp;i++) {
			if(record_writeaddr[n] == -1) break;
			if(++tmp0 != record_writeaddr[n]) break;
			if(tmp0 % spp == 0) break;
			k++;
			n++;
		}

		if(tpl == NULL){
			tpl = (PageList *)BuffListManager_getTopNode(blm, sizeof(PageList));
			pl = tpl;
		}else
			tpl = (PageList *)BuffListManager_getNextNode(blm, (void *)tpl, sizeof(PageList));

		tpl->startPageID = tmp1 / spp;
		tpl->OffsetBytes = tmp1 % spp * SECTOR_SIZE;
		tpl->Bytes = k * SECTOR_SIZE;
		tpl->retVal = 0;
		tpl->pData = NULL;
		if(!data_in_rzone(rep, tpl->startPageID)) {
			oldzoneid = ZoneManager_convertPageToZone(rep->context,tpl->startPageID);
			for(i = 0;i < k;i++)
				Insert_JunkZone(conptr->junkzone,k + i,oldzoneid);
		}
	}
	if(!rep->force) {
		rep->pagelist = pl;
		rep->taskStep = RECYCLE;
	}
	else
		rep->force_pagelist = pl;
	return 0;
}

#if 0
/**
 *	MergerSectorID - Merger sectorID of recycle zone
 *
 *	 @rep: operate object
*/
static int MergerSectorID ( Recycle *rep)
{
	PageList *pl = NULL;
	PageList *tpl = NULL;
	unsigned int tmp0 = 0;
	unsigned int tmp1 = 0;
	unsigned int tmp2 = 0;
	unsigned int tmp3 = 0;
	unsigned int l4count = 0;
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int k = 0;
	unsigned int *l4info = NULL;
	unsigned int *latest_l4info = NULL;
	unsigned int spp = 0;
	int blm = ((Context *)(rep->context))->blm;
	int sectorcount = 0;
	unsigned int *record_writeaddr;
	Context *conptr = (Context *)rep->context;
	int oldzoneid;

	l4count = rep->curpageinfo->L4InfoLen >> 2;
	spp = rep->rZone->vnand->BytePerPage / SECTOR_SIZE;
	l4info = (unsigned int *)(rep->curpageinfo->L4Info);
	latest_l4info = (unsigned int *)(rep->writepageinfo->L4Info);
	pl = NULL;
	tpl = NULL;

	record_writeaddr = rep->record_writeadd;

	memset(record_writeaddr, 0xff, conptr->zonep->l4infolen);

	for(i = 0; i < l4count; i += k) {
		k = 1;
		if ((int)l4info[i] == -1)
			continue;

		tmp0 = l4info[i] / spp;
		tmp1 = latest_l4info[i] / spp;

		if ((tmp0 == tmp1 || latest_l4info[i] == -1) && data_in_3_zone(rep, tmp0)) {
			record_writeaddr[i] = 0;
			for(j = i + 1; j < spp + i && j < l4count; j++) {
				if ((int)l4info[j] == -1)
					break;

				tmp2 = l4info[j] / spp;
				tmp3 = latest_l4info[j] / spp;

				if(tmp2 == tmp0 && (tmp2 == tmp3 || latest_l4info[j] == -1)){
					k++;
					record_writeaddr[j] = 0;
				}
				else
					break;
			}
			if(tpl == NULL){
				tpl = (PageList *)BuffListManager_getTopNode(blm, sizeof(PageList));
				pl = tpl;
			}else
				tpl = (PageList *)BuffListManager_getNextNode(blm, (void *)tpl, sizeof(PageList));
			tpl->startPageID = tmp0;
			tpl->OffsetBytes = l4info[i] % spp * SECTOR_SIZE;
			tpl->Bytes = k * SECTOR_SIZE;
			tpl->retVal = 0;
			tpl->pData = NULL;
			sectorcount += k;
			if(!data_in_rzone(rep, tmp0)) {
				oldzoneid = ZoneManager_convertPageToZone(rep->context,tmp0);
				Insert_JunkZone(conptr->junkzone,k,oldzoneid);
			}
		}
	}

	if(sectorcount > 0 && tpl) {
		FilluptoAlign(rep,tpl,record_writeaddr,sectorcount);
	}

	rep->pagelist = pl;
	rep->taskStep = RECYCLE;

	return 0;
}
#endif
/**
 *	get_current_write_zone - get current write zone
 *
 *	@context: global variable
*/
static Zone *get_current_write_zone(int context)
{
	return ZoneManager_GetCurrentWriteZone(context);
}

/**
 *	alloc_update_l1l2l3l4 - get pageinfo and update cache
 *
 *	@rep: operate object
 *	@wzone: write zone
 *	@pi: pageinfo
 *	@count: sector count
*/
static void alloc_update_l1l2l3l4(Recycle *rep,Zone *wzone,PageInfo *pi, unsigned int sector_count)
{
	unsigned int l1index = 0;
	unsigned int l2index = 0;
	unsigned int l3index = 0;
	unsigned int l1unitlen = 0;
	unsigned int l2unitlen = 0;
	unsigned int l3unitlen = 0;
	unsigned int *l4buf = NULL;
	unsigned int *l3buf = NULL;
	unsigned int *l2buf = NULL;
	unsigned int *l1buf = NULL;
	unsigned int pageid = -1;
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int l4index = 0;
	unsigned int l4count = 0;
	unsigned int startsectorid;
	unsigned int *record_writeaddr;
	unsigned int write_cursor = 0;
	Zone *rzone;
	unsigned int total_sectorcount = 0;
	unsigned int s_count = 0;
	struct singlelist *pos;
	PageList *pl;
	PageList *pl_node = NULL;
	PageInfo *current_pageinfo;
	Context *conptr = (Context *)(rep->context);
	CacheManager *cachemanager = conptr->cachemanager;
	VNandInfo *vnand = &conptr->vnand;
	unsigned int spp = vnand->BytePerPage / SECTOR_SIZE;
	l4count = conptr->zonep->l4infolen >> 2;

	l1buf = (unsigned int *)pi->L1Info;
	l2buf = (unsigned int *)pi->L2Info;
	l3buf = (unsigned int *)pi->L3Info;
	l4buf = (unsigned int *)pi->L4Info;

	l1unitlen = cachemanager->L1UnitLen;
	l2unitlen = cachemanager->L2UnitLen;
	l3unitlen = cachemanager->L3UnitLen;

	pi->L1Index = 0xffff;
	pi->L2Index = 0xffff;
	pi->L3Index = 0xffff;

	if (rep->force) {
		startsectorid = rep->force_startsectorID;
		record_writeaddr = rep->force_record_writeadd;
		rzone = rep->force_rZone;
		rep->force_alloc_num = (sector_count + spp - 1) / spp;
		pl = rep->force_pagelist;
		current_pageinfo = rep->force_curpageinfo;
	}
	else {
		startsectorid = rep->startsectorID;
		record_writeaddr = rep->record_writeadd;
		rzone = rep->rZone;
		rep->alloc_num = (sector_count + spp - 1) / spp;;
		pl = rep->pagelist;
		current_pageinfo = rep->curpageinfo;
	}

	l1index = startsectorid / l1unitlen;
	pi->zoneID = rzone->ZoneID;
	pi->PageID = Zone_AllocNextPage(wzone);
	if (wzone->vnand->v2pp->_2kPerPage > 1) {
		while (pi->PageID % wzone->vnand->v2pp->_2kPerPage)
			pi->PageID = Zone_AllocNextPage(wzone);
	}
#ifdef RECYCLE_DEBUG_PAGEINFO
	L2p_Debug_SetstartPageid(rep->debug,pi->PageID);
#endif
	l1buf[l1index] = pi->PageID;
	pi->L1Index = l1index;
	memset(record_writeaddr, 0xff, conptr->zonep->l4infolen);

	singlelist_for_each(pos,&pl->head) {
		if (total_sectorcount == sector_count)
			break;

		pl_node = singlelist_entry(pos,PageList,head);

		for (j = l4index; j < l4count; j++) {
			if (pl_node->startPageID * spp + pl_node->OffsetBytes / SECTOR_SIZE == l4buf[j])
				break;
		}
		if(j > l4count){
			ndprint(RECYCLE_ERROR,"pl_node->startPageID = %d\n",pl_node->startPageID);
			for (j = 0; j < l4count; j++) {
				ndprint(RECYCLE_ERROR,"infol4[%d] = %d\n",j,l4buf[j]);
			}
			while(1);
		}
		l4index = j;
		if (pos == &pl->head)
			write_cursor = l4index;
		total_sectorcount += pl_node->Bytes / SECTOR_SIZE;

		for (i = pl_node->OffsetBytes / SECTOR_SIZE; i < (pl_node->Bytes + pl_node->OffsetBytes) / SECTOR_SIZE; i++) {
			if (s_count % spp == 0) {
				pageid = Zone_AllocNextPage(wzone);
				/*	if (s_count == 0) {
					while (pageid % wzone->vnand->v2pp->_2kPerPage)
						pageid = Zone_AllocNextPage(wzone);
						}*/
				s_count = 0;
			}
			record_writeaddr[l4index] = pageid * spp + s_count;
			l4buf[l4index] = record_writeaddr[l4index];
			l4index++;
			s_count++;
		}
	}

	if (rep->force)
		rep->force_write_cursor = write_cursor;
	else
		rep->write_cursor = write_cursor;

	if(rzone->L3InfoLen != 0) {
		if(rzone->L2InfoLen != 0) {
			l2index = startsectorid % l1unitlen / l2unitlen;
			l2buf[l2index] = pi->PageID;
			pi->L2Index = l2index;
			l3index = startsectorid % l2unitlen / l3unitlen;
			l3buf[l3index] = pi->PageID;
			pi->L3Index = l3index;
		}
		else {
			l3index = startsectorid % l1unitlen / l3unitlen;
			l3buf[l3index] = pi->PageID;
			pi->L3Index = l3index;
		}
	}
}

/**
 *	Create_read_pagelist - Create read pagelist
 *
 *	@rep: operate object
 *	@pagenum: page count
*/
static int Create_read_pagelist(Recycle *rep, int pagenum)
{
	PageList *pl = NULL;
	PageList *px = NULL;
	PageList *pagelist = NULL;
	struct singlelist *sg = NULL;
	Zone *rzone = NULL;
	int datalen = 0;
	unsigned int flag = 0;
	int blm = (int)((Context *)(rep->context))->blm;

	if (rep->force) {
		rzone = rep->force_rZone;
		pl = rep->force_pagelist;
	}
	else {
		rzone = rep->rZone;
		pl = rep->pagelist;
	}

	datalen = pagenum * rzone->vnand->BytePerPage;

	singlelist_for_each(sg,&pl->head) {
		px = singlelist_entry(sg,PageList,head);
		datalen -= px->Bytes;
		if(datalen <= 0)
			break;
	}

	if(datalen < 0) {
		px->Bytes = px->Bytes - (0 - datalen);

		pagelist = (PageList *)BuffListManager_getTopNode(blm, sizeof(PageList));
		if(pagelist == NULL){
			ndprint(RECYCLE_ERROR,"%s %d ALLOC memory failed!\n",__func__,__LINE__);
			return -1;
		}
		pagelist->startPageID = px->startPageID;
		pagelist->Bytes = 0 - datalen;
		pagelist->OffsetBytes = px->OffsetBytes + px->Bytes;
		pagelist->pData = NULL;
		pagelist->retVal = 0;
		pagelist->head.next = NULL;

		flag = 1;
		ndprint(RECYCLE_ERROR,"<warning> %s %d px->bytes=%d offset=%d pageid=%d\n",__func__,__LINE__,px->Bytes,pagelist->OffsetBytes,pagelist->startPageID);
	}

	if((px->head).next != NULL) {
		if (rep->force)
			rep->force_pagelist = singlelist_entry((px->head).next,PageList,head);
		else
			rep->pagelist = singlelist_entry((px->head).next,PageList,head);
	}
	else {
		if (rep->force)
			rep->force_pagelist = NULL;
		else
			rep->pagelist = NULL;
	}
	(px->head).next = NULL;

	if (rep->force)
		rep->force_read_pagelist = pl;
	else
		rep->read_pagelist = pl;

	if(flag == 1) {
		if (rep->force) {
			if(rep->force_pagelist != NULL){
				BuffListManager_mergerList(blm,(void*)(&pagelist->head),(void*)(&rep->force_pagelist->head));
			}
			else
				pagelist->head.next = NULL;
			rep->force_pagelist = pagelist;
		}
		else {
			if(rep->pagelist != NULL){
				BuffListManager_mergerList(blm,(void*)(&pagelist->head),(void*)(&rep->pagelist->head));
			}else
				pagelist->head.next = NULL;
			rep->pagelist = pagelist;
		}
	}

	return 0;
}

/**
 *	Create_write_pagelist - Create write pagelist
 *
 *	@rep: operate object
 *	@len: page count
*/
static int Create_write_pagelist(Recycle *rep,unsigned int len)
{
	PageList *pl = NULL;
	PageList *px = NULL;
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int l4count = 0;
	unsigned int write_cursor;
	unsigned alloc_num;
	unsigned int *addr;
	int count = 0;
	Context *conptr = (Context *)(rep->context);
	VNandInfo *vnand = &conptr->vnand;
	unsigned int spp = vnand->BytePerPage / SECTOR_SIZE;
	int blm = conptr->blm;

	if (rep->force) {
		addr = rep->force_record_writeadd;
		write_cursor = rep->force_write_cursor;
		alloc_num = rep->force_alloc_num;
	}
	else {
		addr = rep->record_writeadd;
		write_cursor = rep->write_cursor;
		alloc_num = rep->alloc_num;
	}

	l4count = conptr->zonep->l4infolen >> 2;
	pl = (PageList *)BuffListManager_getTopNode(blm, sizeof(PageList));

	for(i = 0 ; i < len ; i++) {
		alloc_num--;
		px = (PageList *)BuffListManager_getNextNode(blm, (void *)pl, sizeof(PageList));
		if(px == NULL) {
			ndprint(RECYCLE_ERROR,"PANIC ERROR func %s line %d \n",__FUNCTION__,__LINE__);
			return -1;
		}

		px->startPageID = addr[write_cursor] / spp;
		px->OffsetBytes = 0;
		px->pData = NULL;
		px->retVal = 1;
		if (i == len - 1) {
			for (j = write_cursor; j < l4count; j++) {
				if (addr[j] != -1)
					count++;
			}
			if (count > spp)
				count = spp;
			px->Bytes = count * SECTOR_SIZE;
			count = 0;
		}
		else
			px->Bytes = vnand->BytePerPage;

		for (j = write_cursor; j < l4count; j++) {
			if (addr[j] != -1)
				count++;
			if (count == spp + 1) {
				count = 0;
				break;
			}
		}
		write_cursor = j;
	}
	BuffListManager_freeList(blm, (void **)&pl, (void *)pl, sizeof(PageList));

	if (rep->force) {
		rep->force_write_pagelist = pl;
		rep->force_write_cursor = write_cursor;
		rep->force_alloc_num = alloc_num;
	}
	else {
		rep->write_pagelist = pl;
		rep->write_cursor = write_cursor;
		rep->alloc_num = alloc_num;
	}

	return 0;
}

/**
 *	all_recycle_buflen - Write all buf data once
 *
 *	@rep: operate object
 *	@wzone: write zone
 *	@count: write count
*/
static int all_recycle_buflen(Recycle *rep,Zone *wzone,unsigned int count)
{
	unsigned int i = 0;
	int ret = 0;
	unsigned int bufpage;
	PageInfo *writepageinfo = NULL;
	PageList *read_pagelist = NULL;
	PageList *write_pagelist = NULL;
	int pagelist;
	int blm = ((Context *)(rep->context))->blm;

	if (rep->force) {
		writepageinfo = rep->force_writepageinfo;
		bufpage = rep->force_buflen / rep->force_rZone->vnand->BytePerPage;
		rep->force_write_pagecount++;
	}
	else {
		writepageinfo = rep->writepageinfo;
		bufpage = rep->buflen / rep->rZone->vnand->BytePerPage;
		rep->write_pagecount++;
	}

	for(i = 0 ; i < count; i++) {
		ret = Create_read_pagelist(rep,bufpage);
		if(ret != 0) {
			ndprint(RECYCLE_ERROR,"creat read pagelist error func %s line %d \n",
							__FUNCTION__,__LINE__);
			return -1;
		}

		ret = Create_write_pagelist(rep,bufpage);
		if(ret != 0) {
			ndprint(RECYCLE_ERROR,"creat write pagelist error func %s line %d \n",
							__FUNCTION__,__LINE__);
			return -1;
		}

		if (rep->force) {
			read_pagelist = rep->force_read_pagelist;
			write_pagelist = rep->force_write_pagelist;
		}
		else {
			read_pagelist = rep->read_pagelist;
			write_pagelist = rep->write_pagelist;
		}

		if(i == 0) {
			pagelist = Zone_MultiWritePage(wzone, bufpage, write_pagelist, writepageinfo);
			write_pagelist =(PageList*)pagelist;
		}
		ret = vNand_CopyData(wzone->vnand, read_pagelist, write_pagelist);
		if(ret != 0) {
			ndprint(RECYCLE_ERROR,"vNand_CopyData error func %s line %d ret=%d startblockid=%d badblock=%08x\n",
					__FUNCTION__,__LINE__,ret,wzone->startblockID,wzone->sigzoneinfo->badblock);
			return -1;
		}
		BuffListManager_freeAllList(blm,(void **)&read_pagelist,sizeof(PageList));
		BuffListManager_freeAllList(blm,(void **)&write_pagelist,sizeof(PageList));
	}

	return 0;
}

/**
 *	part_recycle_buflen - Write part of buf data once
 *
 *	@rep: operate object
 *	@wzone: write zone
 *	@numpage: count of page
 *	@flag: if flag = 1, write pageinfo, else write data
*/
static int part_recycle_buflen(Recycle *rep,Zone *wzone,unsigned int numpage,unsigned int flag)
{
	int ret = 0;
	PageInfo *writepageinfo = NULL;
	PageList *read_pagelist = NULL;
	PageList *write_pagelist = NULL;
	int pagelist;
	int blm = ((Context *)(rep->context))->blm;

	if(numpage == 0)
		return 0;

	ret = Create_read_pagelist(rep,numpage);
	if(ret != 0) {
		ndprint(RECYCLE_ERROR,"creat read pagelist error func %s line %d \n",
						__FUNCTION__,__LINE__);
		return -1;
	}

	ret = Create_write_pagelist(rep,numpage);
	if(ret != 0) {
		ndprint(RECYCLE_ERROR,"creat write pagelist error func %s line %d \n",
							__FUNCTION__,__LINE__);
		return -1;
	}

	if (rep->force) {
		writepageinfo = rep->force_writepageinfo;
		read_pagelist = rep->force_read_pagelist;
		write_pagelist = rep->force_write_pagelist;
	}
	else {
		writepageinfo = rep->writepageinfo;
		read_pagelist = rep->read_pagelist;
		write_pagelist = rep->write_pagelist;
	}
	if(flag == 1) {

			pagelist = Zone_MultiWritePage(wzone, numpage, write_pagelist, writepageinfo);
			write_pagelist = (PageList*)pagelist;
			if(rep->force)
				rep->force_write_pagecount++;
			else
				rep->write_pagecount++;
		}

	ret = vNand_CopyData(wzone->vnand, read_pagelist, write_pagelist);
	if(ret != 0) {
		ndprint(RECYCLE_ERROR,"vNand_CopyData error func %s line %d ret=%d\n",
				__FUNCTION__,__LINE__,ret);
		return -1;
	}
	BuffListManager_freeAllList(blm,(void **)&read_pagelist,sizeof(PageList));
	BuffListManager_freeAllList(blm,(void **)&write_pagelist,sizeof(PageList));

	return 0;
}

/**
 *	alloc_new_zone_write - update prev zone sigzoneinfo and return new zone
 *
 *	@context: global variable
 *	@zone: the prev zone
 */
static Zone *alloc_new_zone_write (int context,Zone *zone)
{
	Zone *new_zone = NULL;
	SigZoneInfo *prev = NULL;
	SigZoneInfo *next = NULL;
	int ret,count = 0;
	if (zone) {
		ZoneManager_SetPrevZone(context,zone);
		ZoneManager_FreeZone(context,zone);
	}
retry:
	if (ZoneManager_GetAheadCount(context) == 0) {
		new_zone = ZoneManager_AllocZone(context);
		if (!new_zone) {
			ndprint(RECYCLE_ERROR,"ERROR: Can't alloc a new zone! %s %d \n",
				__FUNCTION__, __LINE__);
			return NULL;
		}
		ZoneManager_SetAheadZone(context,new_zone);
	}

	ZoneManager_GetAheadZone(context, &new_zone);
	ZoneManager_SetCurrentWriteZone(context,new_zone);
	prev = ZoneManager_GetPrevZone(context);
	next = ZoneManager_GetNextZone(context);
	ret = Zone_Init(new_zone,prev,next);
	if(ret != 0) {
		count++;
		if(count > 4) {
			ndprint(L2PCONVERT_ERROR,"too many zone\n");
		}
		ZoneManager_DropZone(context,new_zone);
		Drop_JunkZone(((Context *)context)->junkzone,new_zone->ZoneID);
		goto retry;
	}
	return new_zone;
}

/**
 *	fill_ahead_zone - alloc 4 zone beforehand
 *
 *	@context: global variable
 */
static int fill_ahead_zone ( int context )
{
	int i;
	unsigned int count = 0;
	Zone *zone = NULL;

	count = ZoneManager_GetAheadCount(context);
	for (i = 0; i < 4 - count; i++){
		zone = ZoneManager_AllocZone(context);
		if (!zone)
			return 1;
		ZoneManager_SetAheadZone(context,zone);
	}

	return 0;
}

/**
 *	get_sectorcount - get count of sector in write operation
 *
 *	@pagelist: calculate object
*/
static int get_sectorcount(Recycle *rep, PageList *pagelist)
{
	struct singlelist *pos;
	PageList *pl_node;
	int sectorcount = 0;

	singlelist_for_each(pos, &pagelist->head) {
		pl_node = singlelist_entry(pos,PageList,head);
		sectorcount += pl_node->Bytes / SECTOR_SIZE;
	}

	return sectorcount;
}

/**
 *	copy_data  - copy data from recycle zone to current write zone
 *
 *	@rep: operate object
 *	@wzone: write zone
 *	@recyclepage: count of page to recycle
*/
static int copy_data(Recycle *rep, Zone *wzone, unsigned int recyclepage)
{
	int ret = 0;
	unsigned int bufpage = 0;
	unsigned int temppage = 0;
	unsigned int temppage1 = 0;

	if (rep->force)
		bufpage = rep->force_buflen / rep->force_rZone->vnand->BytePerPage;
	else
		bufpage = rep->buflen / rep->rZone->vnand->BytePerPage;

	temppage = recyclepage / bufpage;
	temppage1 = recyclepage % bufpage;

	if (temppage) {
		ret = all_recycle_buflen(rep,wzone,temppage);
		if(ret != 0) {
			ndprint(RECYCLE_ERROR,"all recycle buflen error func %s line %d ret=%d\n",
					__FUNCTION__,__LINE__,ret);
			return -1;
		}
	}

	if (temppage1) {
		ret = part_recycle_buflen(rep,wzone,temppage1,!temppage);
		if(ret != 0) {
			ndprint(RECYCLE_ERROR,"part recycle buflen error func %s line %d ret=%d\n",
					__FUNCTION__,__LINE__,ret);
			return -1;

		}
	}

	return ret;
}

/**
 *	RecycleReadWrite  - data transfer in recycle operation
 *
 *	@rep: operate object
*/
#define SPARE_PAGES    1
#define PAGEINFO_PAGES 1

static int RecycleReadWrite(Recycle *rep)
{
	Zone *wzone = NULL;
	unsigned int recyclepage = L4UNITSIZE(rep->rZone->vnand) / rep->rZone->vnand->BytePerPage;
	unsigned int zonepage = 0;
	int ret = 0;
	unsigned int write_sectorcount = 0;
	int spp = rep->rZone->vnand->BytePerPage / SECTOR_SIZE;
	unsigned int recyclesector = recyclepage * spp;
	int wpagecount;
	wzone = get_current_write_zone(rep->context);
	if (!wzone)
		wzone = alloc_new_zone_write(rep->context, wzone);
	zonepage = Zone_GetFreePageCount(wzone);
	if (zonepage < wzone->vnand->v2pp->_2kPerPage) {
		wzone = alloc_new_zone_write(rep->context, wzone);
		if(wzone == NULL) {
			ndprint(RECYCLE_ERROR,"alloc new zone func %s line %d \n",
						__FUNCTION__,__LINE__);
			goto err;
		}
		zonepage = Zone_GetFreePageCount(wzone);
	}

	write_sectorcount = get_sectorcount(rep,rep->pagelist);
	if (write_sectorcount == 0)
		goto exit;
	else if (write_sectorcount < recyclesector) {
		recyclesector = write_sectorcount;
	}
	recyclepage = (recyclesector + spp - 1) / spp;
	rep->write_pagecount += recyclepage;
#ifdef RECYCLE_DEBUG_PAGEINFO
	if(rep->debug == NULL)rep->debug = Init_L2p_Debug(rep->context);
	L2p_Debug_SaveCacheData(rep->debug,rep->writepageinfo);
#endif
	if((zonepage / wzone->vnand->v2pp->_2kPerPage * wzone->vnand->v2pp->_2kPerPage)
			>= (recyclepage + PAGEINFO_PAGES)) {
		alloc_update_l1l2l3l4(rep,wzone,rep->writepageinfo,recyclesector);
		ret = copy_data(rep, wzone, recyclepage);
		if(ret != 0) {
			ndprint(RECYCLE_ERROR,"all recycle buflen error func %s line %d \n",
						__FUNCTION__,__LINE__);
			goto err;
		}
#ifdef RECYCLE_DEBUG_PAGEINFO
		L2p_Debug_CheckData(rep->debug,rep->writepageinfo,recyclepage + 1);
#endif

	}
	else {

		if(zonepage >  wzone->vnand->v2pp->_2kPerPage) {
			wpagecount = zonepage /  wzone->vnand->v2pp->_2kPerPage * wzone->vnand->v2pp->_2kPerPage;
			alloc_update_l1l2l3l4(rep,wzone,rep->writepageinfo, (wpagecount - PAGEINFO_PAGES)*spp);
			ret = copy_data(rep, wzone, wpagecount - PAGEINFO_PAGES);
			if(ret != 0) {
				ndprint(RECYCLE_ERROR,"all recycle buflen error func %s line %d \n",
					__FUNCTION__,__LINE__);
				goto err;
			}
			recyclesector = recyclesector - (wpagecount - PAGEINFO_PAGES)* spp;
			recyclepage = (recyclesector + spp - 1) / spp;
#ifdef RECYCLE_DEBUG_PAGEINFO
			L2p_Debug_CheckData(rep->debug,rep->writepageinfo,wpagecount);
#endif
		}
		wzone = alloc_new_zone_write(rep->context, wzone);
		if(wzone == NULL) {
			ndprint(RECYCLE_ERROR,"ERROR:alloc new zone func %s line %d \n",
						__FUNCTION__,__LINE__);
			goto err;
		}
		if(recyclesector) {
			alloc_update_l1l2l3l4(rep,wzone,rep->writepageinfo,recyclesector);
			ret = copy_data(rep, wzone, recyclepage);
			if(ret != 0) {
				ndprint(RECYCLE_ERROR,"all recycle buflen error func %s line %d \n",
					__FUNCTION__,__LINE__);
				goto err;
			}
		}
#ifdef RECYCLE_DEBUG_PAGEINFO
		L2p_Debug_CheckData(rep->debug,rep->writepageinfo,recyclepage + 1);
#endif
	}

	if (fill_ahead_zone(rep->context))
		rep->taskStep = FINISH;
exit:
	rep->prevpageinfo = rep->curpageinfo;
	give_pageinfo_to_cache(rep,rep->writepageinfo);

	if (rep->end_findnextpageinfo) {
		rep->taskStep = FINISH;
		rep->end_findnextpageinfo = 0;
	}
	else
		rep->taskStep = READNEXTINFO;
	return 0;

err:
	give_pageinfo_to_cache(rep,rep->writepageinfo);
	return -1;
}

/**
 *	FindNextPageInfo - Find next pageinfo of recycle zone
 *
 *	@rep: operate object
 */
static int FindNextPageInfo ( Recycle *rep)
{
	int ret = 0;
	PageInfo *pi = rep->curpageinfo;

	rep->curpageinfo = rep->nextpageinfo;
	do{
		if(rep->rZone->NextPageInfo == 0) {
			rep->end_findnextpageinfo = 1;
			goto exit;
		}
		else if (rep->rZone->NextPageInfo == 0xffff) {
			ndprint(RECYCLE_ERROR,"pageinfo data error func %s line %d \n",
						__FUNCTION__,__LINE__);
			goto err;
		}

		ret = Zone_FindNextPageInfo(rep->rZone,pi);
		if (ISNOWRITE(ret)) {
			rep->end_findnextpageinfo = 1;
			goto exit;
		}
		else if(ret < 0) {
			ndprint(RECYCLE_ERROR,"find next pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
			goto err;
		}

		rep->prevpageinfo = rep->curpageinfo;
		rep->curpageinfo = pi;

		if (pi == rep->pi)
			pi = rep->pi + 1;
		else if (pi == rep->pi + 1)
			pi = rep->pi;
	}while(is_same_L4(rep->prevpageinfo, rep->curpageinfo));

	rep->nextpageinfo = rep->curpageinfo;
	rep->curpageinfo = rep->prevpageinfo;

exit:
	rep->taskStep = FINDVAILD;
	return 0;

err:
	return -1;
}
/**
 *	FreeZone - Free recycle zone
 *
 *	@rep: operate object
 */
static int FreeZone ( Recycle *rep)
{
	int ret = Zone_MarkEraseBlock(rep->rZone,-1,0);
	if(ret) {
		ndprint(RECYCLE_ERROR, "ERROR: markeraseblock error %s, line:%d\n", __func__, __LINE__);
		rep->rZone->sigzoneinfo->pre_zoneid = -1;
		rep->rZone->sigzoneinfo->next_zoneid = -1;
		ZoneManager_DropZone(rep->context,rep->rZone);
	}else{
		if (rep->rZone)
			Delete_JunkZone(((Context *)(rep->context))->junkzone, rep->rZone->ZoneID);
		ZoneManager_FreeRecyclezone(rep->context,rep->rZone);
	}
	rep->taskStep = RECYIDLE;
	return ret;
}

/**
 *	Recycle_Init - Initialize operation
 *
 *	@context: global variable
 */
int Recycle_Init(int context)
{
	int ret = 0;
	Context *conptr = (Context *)context;
	Recycle *rep = NULL;
	conptr->rep = (Recycle *)Nand_VirtualAlloc(sizeof(Recycle));
	rep = conptr->rep;
	if(conptr->rep == NULL) {
		ndprint(RECYCLE_ERROR,"Recycle init alloc recycle error func %s line %d \n",__FUNCTION__,__LINE__);
		return -1;
	}

	memset(conptr->rep,0x0,sizeof(Recycle));
	rep->taskStep = RECYIDLE;
	rep->rZone = NULL;
	rep->force_rZone = NULL;
	rep->prevpageinfo = NULL;
	rep->curpageinfo = NULL;
	rep->nextpageinfo = NULL;
	rep->writepageinfo = NULL;
	rep->startsectorID = 0xffffffff;
	rep->write_cursor = 0;
	rep->pagelist = NULL;
	rep->read_pagelist = NULL;
	rep->write_pagelist = NULL;
	rep->buflen = RECYCLECACHESIZE;
	rep->end_findnextpageinfo = 0;
	rep->recyclemode = RECYCLE_NORMAL_MODE;
	InitNandMutex(&rep->mutex);
	rep->context = context;
	rep->force = 0;
	rep->junk_zoneid = -1;
	rep->write_pagecount = 0;
	ret = alloc_normalrecycle_memory(rep);
	if(ret != 0) {
		ndprint(RECYCLE_ERROR,"alloc_normalrecycle_memory error func %s line %d \n",__FUNCTION__,__LINE__);
		return ret;
	}

	ret = alloc_forcerecycle_memory(rep);
	if(ret != 0) {
		ndprint(RECYCLE_ERROR,"alloc_forcerecycle_memory error func %s line %d \n",__FUNCTION__,__LINE__);
		return ret;
	}

	return ret;
}

/**
 *	Recycle_DeInit - Deinit operation
 *
 *	@context: global variable
 */
void Recycle_DeInit(int context)
{
	Context *conptr = (Context *)context;
	Recycle *rep = conptr->rep;
#ifdef RECYCLE_DEBUG_PAGEINFO
	if(rep->debug != NULL)
		Deinit_L2p_Debug(rep->debug);
#endif
	free_forcerecycle_memory(rep);
	free_normalrecycle_memory(rep);
	DeinitNandMutex(&rep->mutex);
	Nand_VirtualFree(rep);
}

/**
 *	Recycle_Suspend - suspend recycle process
 *
 *	@context: global variable
 */
int Recycle_Suspend ( int context )
{
	Recycle *rep = ((Context *)context)->rep;

	NandMutex_Lock(&rep->mutex);

	ndprint(RECYCLE_INFO, "recycle suspend...........\n");

	return 0;
}

/**
 *	Recycle_Resume -resume recycle process
 *
 *	@context: global variable
 */
int Recycle_Resume ( int context )
{
	Recycle *rep = ((Context *)context)->rep;

	NandMutex_Unlock(&rep->mutex);

	ndprint(RECYCLE_INFO, "recycle resume...........\n");

	return 0;
}

/* for test function  */
/* ******************************************************************** */
int Recycle_getRecycleZone ( Recycle *rep)
{
	return getRecycleZone(rep);
}

int Recycle_FindFirstPageInfo ( Recycle *rep)
{
	return FindFirstPageInfo(rep);
}

int Recycle_FindValidSector ( Recycle *rep)
{
	return FindValidSector(rep);
}
int Recycle_MergerSectorID ( Recycle *rep)
{
//	return MergerSectorID(rep);
	return MergerSectorID_Align(rep);
}
int Recycle_RecycleReadWrite ( Recycle *rep)
{
	return RecycleReadWrite(rep);
}
int Recycle_FindNextPageInfo ( Recycle *rep)
{
	return FindNextPageInfo(rep);
}
int Recycle_FreeZone ( Recycle *rep )
{
	return FreeZone(rep);
}


/* ************************ force rcycle **************************** */
static int ForceRecycle_For_Once ( Recycle *rep, unsigned short zoneid );
static int OnForce_Init ( Recycle *rep );
static int OnForce_GetRecycleZone ( Recycle *rep, unsigned short zoneid );
static int OnForce_FindFirstValidPageInfo ( Recycle *rep );
static int OnForce_FindValidSector ( Recycle *rep);
//static int  OnForce_MergerSectorID ( Recycle *rep);
static int OnForce_RecycleReadWrite(Recycle *rep );
static int OnForce_FindNextValidPageInfo ( Recycle *rep );
static int OnForce_FreeZone ( Recycle *rep );
static void OnForce_Deinit ( Recycle *rep );

/**
 *	Recycle_OnForceRecycle - force recycle
 *
 *	@frinfo: force recycle info
 */
int Recycle_OnForceRecycle ( int frinfo )
{
	int ret = 0;
	int recycle_pagecount = 0;
	ForceRecycleInfo *FRInfo = (ForceRecycleInfo *)frinfo;
	Context *conptr = (Context *)(FRInfo->context);
	Recycle *rep = conptr->rep;
	unsigned short zoneid = FRInfo->suggest_zoneid;
	int need_pagecount = FRInfo->pagecount;

	ndprint(RECYCLE_INFO, "\n%s: start force recycle--------->\n"
			,((PPartition *)(conptr->vnand.prData))->name);
	rep->endpageid = FRInfo->endpageid;
	while (1) {
		ret = OnForce_Init(rep);
		if (ret == -1)
			return -1;

		ret = ForceRecycle_For_Once(rep, zoneid);
		if (ret < 0) {
			ndprint(RECYCLE_ERROR, "ForceRecycle_For_Once ERROR func %s line %d \n",
						__FUNCTION__,__LINE__);
			OnForce_Deinit(rep);
			return -1;
		}

		recycle_pagecount += rep->force_rZone->sumpage - rep->force_write_pagecount;
		ZoneManager_FreeRecyclezone(rep->context,rep->force_rZone);
		rep->force_rZone = NULL;
		if (need_pagecount == -1 || recycle_pagecount >= need_pagecount)
			break;

		zoneid = 0xffff;
	}

	OnForce_Deinit(rep);

	ndprint(RECYCLE_INFO, "%s: force recycle finished RecPagecount = %d---->\n\n"
			,((PPartition *)(conptr->vnand.prData))->name,recycle_pagecount);

	return ret;
}

/**
 *	ForceRecycle_For_Once - force recycle one time
 *
 *	@frinfo: force recycle info
 */
static int ForceRecycle_For_Once ( Recycle *rep, unsigned short zoneid )
{
	int i;
	int ret = 0;

	ret = OnForce_GetRecycleZone(rep, zoneid);
	if (ret == -1) {
		/*
		if (rep->force_junk_zoneid != -1)
			Release_MaxJunkZone(((Context *)(rep->context))->junkzone, rep->force_junk_zoneid);
		*/
		if (rep->force_rZone)
			Delete_JunkZone(((Context *)(rep->context))->junkzone, rep->force_rZone->ZoneID);
		return -1;
	}

	for (i = 0; ; i++) {
		if (rep->force_end_findnextpageinfo)
			break;

		if (i == 0)
			ret = OnForce_FindFirstValidPageInfo(rep);
		else
			ret = OnForce_FindNextValidPageInfo(rep);
		if (ret == -1){
			ndprint(RECYCLE_INFO, "%s %d ret=%d --------->\n",__func__,__LINE__,ret);
			goto exit;
		}

		ret = OnForce_FindValidSector(rep);
		if (ret == -1)
			goto exit;
		MergerSectorID_Align(rep);
		//OnForce_MergerSectorID (rep);

		ret = OnForce_RecycleReadWrite(rep);
		if (ret == -1)
			goto exit;
	}
	ret = OnForce_FreeZone(rep);
exit:
	return ret;
}

/**
 *	OnForce_Init - Force recycle initialize operation
 *
 *	@rep: operate object
 */
static int OnForce_Init ( Recycle *rep )
{
	rep->force_rZone = NULL;
	rep->force_prevpageinfo = NULL;
	rep->force_curpageinfo = NULL;
	rep->force_nextpageinfo = NULL;
	rep->force_writepageinfo = NULL;
	rep->force_startsectorID = 0xffffffff;
	rep->force_write_cursor = 0;
	rep->force_pagelist = NULL;
	rep->force_buflen = RECYCLECACHESIZE;
	rep->force_end_findnextpageinfo = 0;
	rep->force_alloc_num = 0;
	rep->force_write_pagecount = 0;
	rep->force_junk_zoneid = -1;
	rep->force = 1;

	return 0;
}

/**
 *	OnForce_GetRecycleZone - Get force recycle zone
 *
 *	@rep: operate object
 *	@suggest_zoneid: suggest zoneid
 */
static int OnForce_GetRecycleZone ( Recycle *rep, unsigned short suggest_zoneid)
{
	unsigned short ZoneID = suggest_zoneid;
	Zone *zone = ZoneManager_GetCurrentWriteZone(rep->context);
	if (zone && ZoneID == zone->ZoneID) {
		zone = alloc_new_zone_write(rep->context,zone);
		if(zone == NULL) {
			ndprint(RECYCLE_ERROR,"alloc new zone func %s line %d \n",
						__FUNCTION__,__LINE__);
			return -1;
		}
		rep->force_rZone = ZoneManager_AllocRecyclezone(rep->context, ZoneID);
		if(rep->force_rZone == NULL) {
			ndprint(RECYCLE_ERROR,"alloc force recycle zone error ZoneID = %d func %s line %d \n",
						ZoneID, __FUNCTION__,__LINE__);
			return -1;
		}
		ndprint(RECYCLE_INFO, "recycle current write zone...zoneid=%d\n",ZoneID);
	}
	else {
		if (ZoneID == 0xffff) {
GET_FORCE_RECYCLE_JUNKZONE:
			//seek min sectors count is 128 sectors
			ZoneID = Get_MaxJunkZone(((Context *)(rep->context))->junkzone,128);
			if ((ZoneID != 0xffff) && rep->rZone && (ZoneID == rep->rZone->ZoneID)) {
				//Release_MaxJunkZone(((Context *)(rep->context))->junkzone, ZoneID);
				Delete_JunkZone(((Context *)(rep->context))->junkzone, rep->rZone->ZoneID);
				goto GET_FORCE_RECYCLE_JUNKZONE;
			}
			rep->force_junk_zoneid = ZoneID;
		}
		if (ZoneID == 0xffff) {
			ZoneID = get_force_zoneID(rep->context);
			if(ZoneID == 0xffff) {
				ndprint(RECYCLE_ERROR,"PANIC ERROR func %s line %d \n",
							__FUNCTION__,__LINE__);
				return -1;
			}
		}

		rep->force_rZone = ZoneManager_AllocRecyclezone(rep->context,ZoneID);
		if(rep->force_rZone == NULL) {
			ndprint(RECYCLE_ERROR,"alloc force recycle zone error ZoneID = %d func %s line %d \n",
						ZoneID, __FUNCTION__,__LINE__);
			return -1;
		}
	}

	return 0;
}

/**
 *	OnForce_FindFirstValidPageInfo - Find first valid pageinfo of force recycle zone
 *
 *	@rep: operate object
 */
static int OnForce_FindFirstValidPageInfo ( Recycle *rep)
{
	int ret = 0;
	PageInfo *prev_pi = rep->force_pi;
	PageInfo *pi = rep->force_pi + 1;
#ifdef REREAD_PAGEINFO
	VNandInfo *vnand = rep->force_rZone->vnand;
	int nextpageinfo;
	int endpageid = rep->endpageid % (vnand->PagePerBlock * BLOCKPERZONE(vnand));

	if(endpageid == FIRST_PAGEINFO(vnand)){
		ndprint(RECYCLE_INFO,"Error handle for reread pageinfo ecc error: %s %d endpageid: %d\n",
				__func__,__LINE__,endpageid);
		rep->force_curpageinfo = NULL;
		rep->force_end_findnextpageinfo = 1;
		return 0;
	}
#endif
	ret = Zone_FindFirstPageInfo(rep->force_rZone,prev_pi);
	if (ISERROR(ret)) {
		ndprint(RECYCLE_ERROR,"Find First Pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}

	rep->force_curpageinfo = prev_pi;
	do {
		if (rep->force_rZone->NextPageInfo == 0) {
			rep->force_end_findnextpageinfo = 1;
			return 0;
		}
		else if (rep->force_rZone->NextPageInfo == 0xffff) {
			ndprint(RECYCLE_ERROR,"pageinfo data error func %s line %d \n",
						__FUNCTION__,__LINE__);
			return -1;
		}
#ifdef REREAD_PAGEINFO
		nextpageinfo = rep->force_rZone->NextPageInfo;
		if(nextpageinfo == endpageid && rep->endpageid != 0){
			ndprint(RECYCLE_INFO,"Error handle for reread pageinfo ecc error: %s %d endpageid: %d nextpageinfo:%d\n",
					__func__,__LINE__,endpageid,nextpageinfo);
			rep->force_end_findnextpageinfo = 1;
			return 0;
		}
#endif
		ret = Zone_FindNextPageInfo(rep->force_rZone,pi);
		if (ISNOWRITE(ret)) {
			rep->force_end_findnextpageinfo = 1;
			return 0;
		}
		else if (ret < 0) {
			ndprint(RECYCLE_ERROR,"find next pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
			return -1;
		}

		rep->force_prevpageinfo = rep->force_curpageinfo;
		rep->force_curpageinfo = pi;

		if (pi == rep->force_pi)
			pi = rep->force_pi + 1;
		else if (pi == rep->force_pi + 1)
			pi = rep->force_pi;
	}while(is_same_L4(rep->force_prevpageinfo, rep->force_curpageinfo));

	rep->force_nextpageinfo = rep->force_curpageinfo;
	rep->force_curpageinfo = rep->force_prevpageinfo;

	return 0;
}

/**
 *	OnForce_FindNextValidPageInfo - Find next valid pageinfo of force recycle zone
 *
 *	@rep: operate object
 */
static int OnForce_FindNextValidPageInfo ( Recycle *rep)
{
	int ret = 0;
	PageInfo *pi = rep->force_curpageinfo;
#ifdef REREAD_PAGEINFO
	VNandInfo *vnand = &((Context *)(rep->context))->vnand;
	int nextpageinfo;
	int endpageid = rep->endpageid % (vnand->PagePerBlock * BLOCKPERZONE(vnand));
#endif
	rep->force_curpageinfo = rep->force_nextpageinfo;
	do{
		if(rep->force_rZone->NextPageInfo == 0) {
			rep->force_end_findnextpageinfo = 1;
			return 0;
		}
		else if (rep->force_rZone->NextPageInfo == 0xffff) {
			ndprint(RECYCLE_ERROR,"pageinfo data error func %s line %d \n",
						__FUNCTION__,__LINE__);
			return -1;
		}
#ifdef REREAD_PAGEINFO
		nextpageinfo = rep->force_rZone->NextPageInfo;
		if(nextpageinfo == endpageid && rep->endpageid != 0){
			ndprint(RECYCLE_INFO,"Error handle for reread pageinfo ecc error: %s %d endpageid: %d nextpageinfo:%d\n",
					__func__,__LINE__,endpageid,nextpageinfo);
			rep->force_end_findnextpageinfo = 1;
			return 0;
		}
#endif
		ret = Zone_FindNextPageInfo(rep->force_rZone,pi);
		if (ISNOWRITE(ret)) {
			rep->force_end_findnextpageinfo = 1;
			return 0;
		}
		else if(ret < 0) {
			ndprint(RECYCLE_ERROR,"find next pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
			return -1;
		}

		rep->force_prevpageinfo = rep->force_curpageinfo;
		rep->force_curpageinfo = pi;

		if (pi == rep->force_pi)
			pi = rep->force_pi + 1;
		else if (pi == rep->force_pi + 1)
			pi = rep->force_pi;
	}while(is_same_L4(rep->force_prevpageinfo, rep->force_curpageinfo));

	rep->force_nextpageinfo = rep->force_curpageinfo;
	rep->force_curpageinfo = rep->force_prevpageinfo;

	return 0;
}

/**
 *	OnForce_FindValidSector - Find valid sector of force recycle zone
 *
 *	@rep: operate object
 */
static int OnForce_FindValidSector ( Recycle *rep)
{
	unsigned short l3index = 0;
	unsigned short l2index = 0;
	unsigned short l1index = 0;
	unsigned int l1unitlen = 0;
	unsigned int l2unitlen = 0;
	unsigned int l3unitlen = 0;
	unsigned int start_sectorID = 0;
	Context *conptr = (Context *)(rep->context);
	CacheManager *cachemanager = conptr->cachemanager;
	PageInfo *pi = NULL;

	l1unitlen = cachemanager->L1UnitLen;
	l2unitlen = cachemanager->L2UnitLen;
	l3unitlen = cachemanager->L3UnitLen;

	pi = rep->force_curpageinfo;
	if(pi == NULL)
		return 0;
	l1index = pi->L1Index;
	l2index = pi->L2Index;
	l3index = pi->L3Index;

	if (l1index == 0xffff) {
		ndprint(RECYCLE_ERROR,"PANIC ERROR l1index = -1, func %s line %d \n",__FUNCTION__,__LINE__);
		goto err;
	}

	if(pi->L3InfoLen == 0)
		start_sectorID = l1index * l1unitlen;
	else {
		if (l3index == 0xffff) {
			ndprint(RECYCLE_ERROR,"PANIC ERROR l3index = -1, func %s line %d \n",__FUNCTION__,__LINE__);
			goto err;
		}

		if(pi->L2InfoLen == 0)
			start_sectorID = l1index * l1unitlen + l3index * l3unitlen;
		else {
			if (l2index == 0xffff) {
				ndprint(RECYCLE_ERROR,"PANIC ERROR l2index = -1, func %s line %d \n",__FUNCTION__,__LINE__);
				goto err;
			}

			start_sectorID = l3index * l3unitlen +
				l2index * l2unitlen + l1index * l1unitlen;
		}
	}

	rep->force_startsectorID = start_sectorID;
	rep->force_writepageinfo = get_pageinfo_from_cache(rep,rep->force_startsectorID);
	return 0;
err:
	return -1;
}
#if 0
/**
 *	OnForce_MergerSectorID - Merger sectorID of force recycle zone
 *
 *	@rep: operate object
 */
static int  OnForce_MergerSectorID ( Recycle *rep)
{
	PageList *pl = NULL;
	PageList *tpl = NULL;
	unsigned int tmp0 = 0;
	unsigned int tmp1 = 0;
	unsigned int tmp2 = 0;
	unsigned int tmp3 = 0;
	unsigned int l4count = 0;
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int k = 0;
	unsigned int *l4info = NULL;
	unsigned int *latest_l4info = NULL;
	unsigned int spp = 0;
	int blm = ((Context *)(rep->context))->blm;
	int sectorcount = 0;
	unsigned int *record_writeaddr;
	Context *conptr = (Context *)rep->context;

	if(rep->force_curpageinfo == NULL)
		return 0;
	l4count = rep->force_curpageinfo->L4InfoLen >> 2;
	spp = rep->force_rZone->vnand->BytePerPage / SECTOR_SIZE;
	l4info = (unsigned int *)(rep->force_curpageinfo->L4Info);
	latest_l4info = (unsigned int *)(rep->force_writepageinfo->L4Info);

	record_writeaddr = rep->record_writeadd;
	memset(record_writeaddr, 0xff, conptr->zonep->l4infolen);

	pl = NULL;
	tpl = NULL;
	for(i = 0; i < l4count; i += k) {
		k = 1;
		if ((int)l4info[i] == -1)
			continue;

		tmp0 = l4info[i] / spp;
		tmp1 = latest_l4info[i] / spp;

		if ((tmp0 == tmp1 || latest_l4info[i] == -1) && data_in_rzone(rep, tmp0)) {
			record_writeaddr[i] = 0;
			for(j = i + 1; j < spp + i && j < l4count; j++) {
				if ((int)l4info[j] == -1)
					break;

				tmp2 = l4info[j] / spp;
				tmp3 = latest_l4info[j] / spp;
				if(tmp2 == tmp0 && (tmp2 == tmp3 || latest_l4info[j] == -1)) {
					record_writeaddr[j] = 0;
					k++;
				}
				else
					break;
			}
			if(tpl == NULL){
				pl = (PageList *)BuffListManager_getTopNode(blm, sizeof(PageList));
				tpl = pl;
			}else tpl = (PageList *)BuffListManager_getNextNode(blm, (void *)tpl, sizeof(PageList));
			tpl->startPageID = tmp0;
			tpl->OffsetBytes = l4info[i] % spp * SECTOR_SIZE;
			tpl->Bytes = k * SECTOR_SIZE;
			tpl->retVal = 0;
			tpl->pData = NULL;
			sectorcount += k;
		}
	}
	rep->force_pagelist = pl;

	if(sectorcount > 0 && tpl) {
		FilluptoAlign(rep,tpl,record_writeaddr,sectorcount);
	}
	return 0;
}
#endif
/**
 *	OnForce_RecycleReadWrite - Data transfer of force recycle operation
 *
 *	@rep: operate object
 */
static int OnForce_RecycleReadWrite(Recycle *rep)
{
	int ret = 0;
	Zone *wzone = NULL;
	unsigned int recyclepage = L4UNITSIZE(rep->force_rZone->vnand) / rep->force_rZone->vnand->BytePerPage;
	unsigned int zonepage = 0;
	unsigned int write_sectorcount = 0;
	int spp = rep->force_rZone->vnand->BytePerPage / SECTOR_SIZE;
	unsigned int recyclesector = recyclepage * spp;
    int wpagecount;

	if(rep->force_curpageinfo == NULL)
		return 0;
	wzone = get_current_write_zone(rep->context);
	if (!wzone)
		wzone = alloc_new_zone_write(rep->context, wzone);
	zonepage = Zone_GetFreePageCount(wzone);
	if (zonepage < wzone->vnand->v2pp->_2kPerPage) {
		wzone = alloc_new_zone_write(rep->context, wzone);
		if(wzone == NULL) {
			ndprint(RECYCLE_ERROR,"alloc new zone func %s line %d \n",
						__FUNCTION__,__LINE__);
			goto err;
		}
		zonepage = Zone_GetFreePageCount(wzone);
	}

	write_sectorcount = get_sectorcount(rep,rep->force_pagelist);
	if (write_sectorcount == 0)
		goto exit;
	else if (write_sectorcount < recyclesector) {
		recyclesector = write_sectorcount;
	}
	recyclepage = (recyclesector + spp - 1) / spp;
	rep->force_write_pagecount += recyclepage;
#ifdef RECYCLE_DEBUG_PAGEINFO
	if(rep->debug == NULL)rep->debug = Init_L2p_Debug(rep->context);
	L2p_Debug_SaveCacheData(rep->debug,rep->force_writepageinfo);
#endif
	if((zonepage / wzone->vnand->v2pp->_2kPerPage * wzone->vnand->v2pp->_2kPerPage)
			>= (recyclepage + PAGEINFO_PAGES)) {
		alloc_update_l1l2l3l4(rep,wzone,rep->force_writepageinfo,recyclesector);
		ret = copy_data(rep, wzone, recyclepage);
		if(ret != 0) {
			ndprint(RECYCLE_ERROR,"all recycle buflen error func %s line %d \n",
						__FUNCTION__,__LINE__);
			goto err;
		}
#ifdef RECYCLE_DEBUG_PAGEINFO
		L2p_Debug_CheckData(rep->debug,rep->force_writepageinfo,recyclepage + 1);
#endif
	}
	else {
		if(zonepage > wzone->vnand->v2pp->_2kPerPage) {
			wpagecount = zonepage /  wzone->vnand->v2pp->_2kPerPage * wzone->vnand->v2pp->_2kPerPage;

			alloc_update_l1l2l3l4(rep,wzone,rep->force_writepageinfo,(wpagecount - PAGEINFO_PAGES) * spp);
			ret = copy_data(rep, wzone, wpagecount - PAGEINFO_PAGES);
			if(ret != 0) {
				ndprint(RECYCLE_ERROR,"all recycle buflen error func %s line %d \n",
							__FUNCTION__,__LINE__);
				goto err;
			}

			recyclesector = recyclesector - (wpagecount - PAGEINFO_PAGES) * spp;
			recyclepage = (recyclesector + spp - 1) / spp;
#ifdef RECYCLE_DEBUG_PAGEINFO
		L2p_Debug_CheckData(rep->debug,rep->force_writepageinfo,wpagecount);
#endif
		}
		wzone = alloc_new_zone_write(rep->context, wzone);
		if(wzone == NULL) {
			ndprint(RECYCLE_ERROR,"alloc new zone func %s line %d \n",
						__FUNCTION__,__LINE__);
			goto err;
		}

		alloc_update_l1l2l3l4(rep,wzone,rep->force_writepageinfo,recyclesector);

		ret = copy_data(rep, wzone, recyclepage);
		if(ret != 0) {
			ndprint(RECYCLE_ERROR,"all recycle buflen error func %s line %d \n",
						__FUNCTION__,__LINE__);
			goto err;
		}
#ifdef RECYCLE_DEBUG_PAGEINFO
		L2p_Debug_CheckData(rep->debug,rep->force_writepageinfo,recyclepage + 1);
#endif
	}
	fill_ahead_zone(rep->context);

exit:
	give_pageinfo_to_cache(rep,rep->force_writepageinfo);
	return 0;
err:
	give_pageinfo_to_cache(rep,rep->force_writepageinfo);
	return -1;
}

/**
 *	OnForce_FreeZone - Frdd force recycle zone
 *
 *	@rep: operate object
 */
static int OnForce_FreeZone ( Recycle *rep)
{
	unsigned int pageid = -1;

	if(rep->endpageid != 0) {
		pageid = rep->endpageid;
	}

	if(Zone_MarkEraseBlock(rep->force_rZone,pageid,0)) {
		rep->force_rZone->sigzoneinfo->pre_zoneid = -1;
		rep->force_rZone->sigzoneinfo->next_zoneid = -1;
		ZoneManager_DropZone(rep->context,rep->force_rZone);
		Drop_JunkZone(((Context *)(rep->context))->junkzone, rep->force_rZone->ZoneID);
		rep->force_rZone = NULL;
		return -1;
	}else{
		if (rep->force_rZone)
			Delete_JunkZone(((Context *)(rep->context))->junkzone, rep->force_rZone->ZoneID);
	}
	return 0;
}

/**
 *	OnForce_Deinit - Deinit operation
 *
 *	@rep: operate object
 */
static void OnForce_Deinit ( Recycle *rep)
{
	rep->force = 0;
}



/* ************************ follow rcycle **************************** */


/**
 *	OnFollow_FindFirstValidPageInfo - Find first valid pageinfo of follow recycle zone
 *
 *	@rep: operate object
 */
static int OnFollow_FindFirstValidPageInfo ( Recycle *rep)
{
	int ret = 0;
	PageInfo *prev_pi = rep->force_pi;
	PageInfo *pi = rep->force_pi + 1;
	Context *conptr = (Context *)(rep->context);
	ZoneManager *zonep = conptr->zonep;
	int pageperzone = conptr->vnand.PagePerBlock * BLOCKPERZONE(conptr->vnand);

	ret = Zone_FindFirstPageInfo(rep->force_rZone,prev_pi);
	if (ISERROR(ret)) {
		ndprint(RECYCLE_ERROR,"Find First Pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}

	rep->force_curpageinfo = prev_pi;
	while (pageperzone--) {
		do {
			if (rep->force_rZone->NextPageInfo == 0) {
				rep->force_end_findnextpageinfo = 1;
				return 0;
			}
			else if (rep->force_rZone->NextPageInfo == 0xffff) {
				ndprint(RECYCLE_ERROR,"pageinfo data error func %s line %d \n",
							__FUNCTION__,__LINE__);
				return -1;
			}

			ret = Zone_FindNextPageInfo(rep->force_rZone,pi);
			if (ISNOWRITE(ret)) {
				rep->force_end_findnextpageinfo = 1;
				return 0;
			}
			else if(ret < 0) {
				ndprint(RECYCLE_ERROR,"find next pageinfo error func %s line %d \n",
						__FUNCTION__,__LINE__);
				return -1;
			}

			rep->force_prevpageinfo = rep->force_curpageinfo;
			rep->force_curpageinfo = pi;

			if (pi == rep->force_pi)
				pi = rep->force_pi + 1;
			else if (pi == rep->force_pi + 1)
				pi = rep->force_pi;
		}while(is_same_L4(rep->force_prevpageinfo, rep->force_curpageinfo));

		if (is_same_L4(rep->force_prevpageinfo, zonep->last_pi)
			|| rep->force_end_findnextpageinfo == 1)
			break;
	}

	rep->force_nextpageinfo = rep->force_curpageinfo;
	rep->force_curpageinfo = rep->force_prevpageinfo;

	return 0;
}

/**
 *	Recycle_OnFollowRecycle - follow recycle
 *
 *	@context: global variable
 */
int Recycle_OnFollowRecycle ( int context )
{
	int i;
	int ret = 0;
	Recycle *rep = ((Context *)context)->rep;
	ZoneManager *zonep = ((Context *)context)->zonep;
	unsigned short zoneid = zonep->last_rzone_id;

	ndprint(RECYCLE_INFO, "start follow recycle--------->\n");

	ret = OnForce_Init(rep);
	if (ret == -1)
		return -1;

	ret = OnForce_GetRecycleZone(rep, zoneid);
	if (ret == -1)
		goto exit;

	for (i = 0; ; i++) {
		if (rep->force_end_findnextpageinfo)
			break;

		if (i == 0)
			ret = OnFollow_FindFirstValidPageInfo(rep);
		else
			ret = OnForce_FindNextValidPageInfo(rep);
		if (ret == -1) {
			ndprint(RECYCLE_ERROR,"find infopage error\n");
			break;
		}
		ret = OnForce_FindValidSector(rep);
		if (ret == -1)
			goto exit;

		MergerSectorID_Align(rep);
		//OnForce_MergerSectorID (rep);

		if(ret == 0){
			ret = OnForce_RecycleReadWrite(rep);
			if (ret == -1)
				goto exit;
		}
	}

	ret = OnForce_FreeZone(rep);
	if (ret == -1)
		goto exit;
	ZoneManager_FreeRecyclezone(rep->context,rep->force_rZone);

exit:
	OnForce_Deinit(rep);
	ndprint(RECYCLE_INFO, "follow recycle finished--------->\n\n");

	return ret;
}



/* ************************ boot rcycle **************************** */


/**
 *	OnBoot_GetRecycleZone - Get boot recycle zone
 *
 *	@rep: operate object
 */
static int OnBoot_GetRecycleZone ( Recycle *rep)
{
	Zone *zone;
	SigZoneInfo *prev = NULL;
	SigZoneInfo *next = NULL;
	ZoneManager *zonep = ((Context *)(rep->context))->zonep;
	int ret,count = 0;
	int ZoneID = zonep->last_zone->ZoneID;

	ZoneManager_SetPrevZone(zonep->context,zonep->last_zone);
	ZoneManager_FreeZone (zonep->context,zonep->last_zone);
	rep->force_rZone = ZoneManager_AllocRecyclezone(rep->context ,ZoneID);
	if(rep->force_rZone == NULL) {
		ndprint(RECYCLE_ERROR,"alloc boot recycle zone error ZoneID = %d func %s line %d \n",
				ZoneID, __FUNCTION__,__LINE__);
		return -1;
	}
retry:
	if (ZoneManager_GetAheadCount(zonep->context) == 0) {
		zone = ZoneManager_AllocZone(zonep->context);
		if (!zone) {
			ndprint(RECYCLE_ERROR,"ERROR: Can't alloc a new zone! %s %d \n",
				__FUNCTION__, __LINE__);
			return -1;
		}
		ZoneManager_SetAheadZone(zonep->context,zone);
	}

	ZoneManager_GetAheadZone(zonep->context, &zone);
	ZoneManager_SetCurrentWriteZone(zonep->context,zone);
	prev = ZoneManager_GetPrevZone(zonep->context);
	next = ZoneManager_GetNextZone(zonep->context);
	ret = Zone_Init(zone,prev,next);
	if(ret != 0) {
		count++;
		if(count > 4){
			ndprint(L2PCONVERT_ERROR,"too many zone\n");
		}
		ZoneManager_DropZone(zonep->context,zone);
		Drop_JunkZone(((Context *)(zonep->context))->junkzone, zone->ZoneID);
		goto retry;
	}
	return 0;
}

/**
 *	Recycle_OnBootRecycle - boot recycle
 *
 *	@context: global variable
 */
int Recycle_OnBootRecycle ( int bootinfo )
{
	int i;
	int ret = 0;
	ForceRecycleInfo *BootInfo = (ForceRecycleInfo *)bootinfo;
	Context *conptr = (Context *)(BootInfo->context);
	Recycle *rep = conptr->rep;
	ndprint(RECYCLE_INFO, "start boot recycle--------->\n");

	rep->endpageid = BootInfo->endpageid;
	ret = OnForce_Init(rep);
	if (ret == -1)
		return -1;

	ret = OnBoot_GetRecycleZone(rep);
	if (ret == -1)
		goto exit;

	for (i = 0; ; i++) {
		if (rep->force_end_findnextpageinfo)
			break;

		if (i == 0)
			ret = OnForce_FindFirstValidPageInfo(rep);
		else
			ret = OnForce_FindNextValidPageInfo(rep);
		if (ret == -1)
			goto exit;

		ret = OnForce_FindValidSector(rep);
		if (ret == -1)
			goto exit;

		MergerSectorID_Align (rep);
		//OnForce_MergerSectorID (rep);

		if(ret == 0){
			ret = OnForce_RecycleReadWrite(rep);
			if (ret == -1)
				goto exit;
		}
	}
	ret = OnForce_FreeZone(rep);
	if (ret == -1)
		goto exit;
	ZoneManager_FreeRecyclezone(rep->context,rep->force_rZone);

exit:
	OnForce_Deinit(rep);
	ndprint(RECYCLE_INFO, "boot recycle finished--------->\n\n");

	return ret;
}

void Recycle_Lock(int context)
{
	Recycle *rep = ((Context *)context)->rep;
	NandMutex_Lock(&rep->mutex);
}

void Recycle_Unlock(int context)
{
	Recycle *rep = ((Context *)context)->rep;
	NandMutex_Unlock(&rep->mutex);
}

int Recycle_OnEraseRecycle ( int context )
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;
        Hash *hash = zonep->useZone;
        HashNode *hashnode = NULL;
        SigZoneInfo *sigzoneinfo = NULL;
        unsigned short ZoneID = -1;
        Zone *zone = NULL;
	int cm = (int)conptr->cachemanager;
        int i = 0, ret = 0;

        Recycle_Lock(context);
        zone = ZoneManager_GetCurrentWriteZone(context);
        if (zone != NULL) {
                ret = Zone_MarkEraseBlock(zone, -1, 0);
                if(ret) {
                        ndprint(ZONEMANAGER_ERROR, "ERROR: eraseblock error %s, line:%d\n"
                                        , __func__, __LINE__);
                        zone->sigzoneinfo->pre_zoneid = -1;
                        zone->sigzoneinfo->next_zoneid = -1;
                        ZoneManager_DropZone(context, zone);
                        Drop_JunkZone(conptr->junkzone, zone->ZoneID);
                } else {
                        Delete_JunkZone(conptr->junkzone, zone->ZoneID);
                        ZoneManager_FreeRecyclezone(context, zone);
                }
        }
        for (i = 0; i < HASHNODE_COUNT; i++) {
                hashnode = (HashNode *)(*(hash->top + i));
                while (1) {
                        sigzoneinfo = HashNode_getTop(hashnode);
                        if (NULL == sigzoneinfo)
                                break;
                        ZoneID = sigzoneinfo - zonep->sigzoneinfo;
                        ndprint(ZONEMANAGER_INFO,"erase zoneid = %d \n", ZoneID);
                        zone = ZoneManager_AllocRecyclezone(context, ZoneID);
                        ret = Zone_MarkEraseBlock(zone, -1, 0);
                        if(ret) {
                                ndprint(ZONEMANAGER_ERROR, "ERROR: eraseblock error %s, line:%d\n"
                                                , __func__, __LINE__);
                                zone->sigzoneinfo->pre_zoneid = -1;
                                zone->sigzoneinfo->next_zoneid = -1;
                                ZoneManager_DropZone(context, zone);
                                Drop_JunkZone(conptr->junkzone, zone->ZoneID);
                        } else {
                                Delete_JunkZone(conptr->junkzone, zone->ZoneID);
								ZoneManager_FreeRecyclezone(context, zone);
                        }
                }
        }

        ((junkzone *)(conptr->junkzone))->current_zoneid = -1;
        ((junkzone *)(conptr->junkzone))->max_timestamp = nd_get_timestamp();
        ((junkzone *)(conptr->junkzone))->min_node = NULL;
        zonep->last_zone = NULL;
        zonep->write_zone = NULL;
        zonep->prev = NULL;
        zonep->next = NULL;
        ndprint(ZONEMANAGER_INFO,"%s finish!!! \n", __func__);

        CacheManager_DropAllCache(cm);
        Recycle_Unlock(context);

        return 0;
}
