#include "clib.h"
#include "recycle.h"
#include "hash.h"
#include "pagelist.h"
#include "pageinfo.h"
#include "zone.h"
#include "cachemanager.h"
#include "nanddebug.h"
#include "zonemanager.h"
#include "NandSemaphore.h"
#include "bufflistmanager.h"
#include "context.h"
#include "vNand.h"
#include "l2vNand.h"
#include "taskmanager.h"
#include "nanddebug.h"
#include "nandsigzoneinfo.h"
#include "nmbitops.h"
//#include "badblockinfo.h"

#define L4UNITSIZE(context) 128 * 1024
#define RECYCLECACHESIZE		VNANDCACHESIZE
#define BALANCECOUNT        50

static int getRecycleZone ( Recycle *rep);
static int FindFirstPageInfo ( Recycle *rep);
static int FindValidSector ( Recycle *rep);
static int MergerSectorID ( Recycle *rep);
static int RecycleReadWrite ( Recycle *rep);
static int FindNextPageInfo ( Recycle *rep);
static int FreeZone ( Recycle *rep );
int Recycle_OnForceRecycle ( int frinfo );

/**
 *	Recycle_OnFragmentHandle - Process of normal recycle
 *
 *	@context: global variable
 */
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
			if (ret == -1) {
				if (rep->junk_zoneid != -1) {
					Release_MaxJunkZone(((Context *)(rep->context))->junkzone, rep->junk_zoneid);
					rep->junk_zoneid = -1;
				}
				return ret;
			}
			break;
		case READFIRSTINFO:
			NandMutex_Lock(&rep->mutex);
			if (nd_getcurrentsec_ns() < (conptr->t_startrecycle + INTERNAL_TIME)) {
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
			if (nd_getcurrentsec_ns() < (conptr->t_startrecycle + INTERNAL_TIME)) {
				NandMutex_Unlock(&rep->mutex);
				break;
			}
			ret = FindValidSector(rep);
			if (ret == -1)
				goto ERROR;
			break;
		case MERGER:
			MergerSectorID(rep);
			break;
		case RECYCLE:
			ret = RecycleReadWrite(rep);
			NandMutex_Unlock(&rep->mutex);
			if (ret == -1)
				goto ERROR;
			break;
		case READNEXTINFO:
			NandMutex_Lock(&rep->mutex);
			if (nd_getcurrentsec_ns() < (conptr->t_startrecycle + INTERNAL_TIME)) {
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
			if (nd_getcurrentsec_ns() < (conptr->t_startrecycle + INTERNAL_TIME)) {
				NandMutex_Unlock(&rep->mutex);
				break;
			}
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

	return ret;

ERROR:
	if (rep->junk_zoneid != -1) {
		Release_MaxJunkZone(((Context *)(rep->context))->junkzone, rep->junk_zoneid);
		rep->junk_zoneid = -1;
	}
	else
		Delete_JunkZone(((Context *)(rep->context))->junkzone, rep->rZone->ZoneID);
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
	ndprint(RECYCLE_INFO, "start normal recycle--------->\n");
	if (rep->taskStep == RECYIDLE)
		rep->taskStep = RECYSTART;
	NandMutex_Unlock(&rep->mutex);

	while(1) {
		if (ret == -1 || rep->taskStep == RECYIDLE)
			break;

		ret = Recycle_OnFragmentHandle(context);
	}

	NandMutex_Lock(&rep->mutex);
	rep->taskStep = RECYIDLE;
	ndprint(RECYCLE_INFO, "normal recycle finished--------->\n\n");
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

	count = ZoneManager_Getusedcount(context);
	if(count == 0) {
		ndprint(RECYCLE_ERROR,"PANIC ERROR func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}

	minlifetime = ZoneManager_Getminlifetime(context);
	maxlifetime = ZoneManager_Getmaxlifetime(context);
	lifetime = minlifetime + ( maxlifetime - minlifetime ) / 3;
/*
	if (lifetime == minlifetime)
		return ZoneManager_ForceRecyclezoneID(context,lifetime + 1);
*/
	if(lifetime < minlifetime + BALANCECOUNT)
		return -1;
	return ZoneManager_RecyclezoneID(context,lifetime);
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

	ZoneID = Get_MaxJunkZone(((Context *)context)->junkzone);
	rep->junk_zoneid = ZoneID;
	if(ZoneID == 0xffff)
		ZoneID = get_normal_zoneID(context);

	if(ZoneID == 0xffff) {
		ret = -1;
		goto err;
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
		else if((ret & 0xffff) < 0) {
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
	BuffListManager *blm = ((Context *)(rep->context))->blm;

	l4count = rep->curpageinfo->L4InfoLen >> 2;
	spp = rep->rZone->vnand->BytePerPage / SECTOR_SIZE;
	l4info = (unsigned int *)(rep->curpageinfo->L4Info);
	latest_l4info = (unsigned int *)(rep->writepageinfo->L4Info);
	pl = NULL;
	tpl = NULL;

	for(i = 0; i < l4count; i += k) {
		k = 1;
		if ((int)l4info[i] == -1)
			continue;

		tmp0 = l4info[i] / spp;
		tmp1 = latest_l4info[i] / spp;

		if (tmp0 == tmp1 && data_in_3_zone(rep, tmp0)) {
			for(j = i + 1; j < spp + i && j < l4count; j++) {
				if ((int)l4info[j] == -1)
					break;

				tmp2 = l4info[j] / spp;
				tmp3 = latest_l4info[j] / spp;
				if(tmp2 == tmp0 && tmp2 == tmp3)
					k++;
				else
					break;
			}
			if(tpl == NULL){
				tpl = (PageList *)BuffListManager_getTopNode((int)blm, sizeof(PageList));
				pl = tpl;
			}else
				tpl = (PageList *)BuffListManager_getNextNode((int)blm, (void *)tpl, sizeof(PageList));
			tpl->startPageID = tmp0;
			tpl->OffsetBytes = l4info[i] % spp * SECTOR_SIZE;
			tpl->Bytes = k * SECTOR_SIZE;
			tpl->retVal = 0;
			tpl->pData = NULL;
		}
	}

	rep->pagelist = pl;
	rep->taskStep = RECYCLE;

	return 0;
}

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
	l1buf[l1index] = pi->PageID;
	pi->L1Index = l1index;
	memset(record_writeaddr, 0xff, conptr->zonep->l4infolen);

	singlelist_for_each(pos,&pl->head) {
		if (total_sectorcount == sector_count)
			break;

		pl_node = singlelist_entry(pos,PageList,head);

		for (j = l4index; j < l4count; j++) {
			if (pl_node->startPageID * spp + pl_node->OffsetBytes / SECTOR_SIZE == ((unsigned int *)(current_pageinfo->L4Info))[j])
				break;
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
	int blmid = (int)((Context *)(rep->context))->blm;

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

		pagelist = (PageList *)BuffListManager_getTopNode(blmid, sizeof(PageList));
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
				BuffListManager_mergerList(blmid,(void*)(&pagelist->head),(void*)(&rep->force_pagelist->head));
			}
			else
				pagelist->head.next = NULL;
			rep->force_pagelist = pagelist;
		}
		else {
			if(rep->pagelist != NULL){
				BuffListManager_mergerList(blmid,(void*)(&pagelist->head),(void*)(&rep->pagelist->head));
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
	BuffListManager *blm = conptr->blm;

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
	pl = (PageList *)BuffListManager_getTopNode((int)blm, sizeof(PageList));

	for(i = 0 ; i < len ; i++) {
		alloc_num--;
		px = (PageList *)BuffListManager_getNextNode((int)blm, (void *)pl, sizeof(PageList));
		if(px == NULL) {
			ndprint(RECYCLE_ERROR,"PANIC ERROR func %s line %d \n",__FUNCTION__,__LINE__);
			return -1;
		}

		px->startPageID = addr[write_cursor] / spp;
		px->OffsetBytes = 0;
		px->pData = NULL;
		px->retVal = 0;
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
	BuffListManager_freeList((int)blm, (void **)&pl, (void *)pl, sizeof(PageList));

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
	BuffListManager *blm = ((Context *)(rep->context))->blm;

	if (rep->force) {
		writepageinfo = rep->force_writepageinfo;
		bufpage = rep->force_buflen / rep->force_rZone->vnand->BytePerPage;
	}
	else {
		writepageinfo = rep->writepageinfo;
		bufpage = rep->buflen / rep->rZone->vnand->BytePerPage;
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
			rep->write_pagecount++;
		}
		ret = vNand_CopyData(wzone->vnand, read_pagelist, write_pagelist);
		if(ret != 0) {
			ndprint(RECYCLE_ERROR,"vNand_CopyData error func %s line %d ret=%d startblockid=%d badblock=%08x\n",
					__FUNCTION__,__LINE__,ret,wzone->startblockID,wzone->badblock);
			return -1;
		}
		BuffListManager_freeAllList((int)blm,(void **)&read_pagelist,sizeof(PageList));
		BuffListManager_freeAllList((int)blm,(void **)&write_pagelist,sizeof(PageList));
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
	BuffListManager *blm = ((Context *)(rep->context))->blm;

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
			rep->write_pagecount++;
		}

	ret = vNand_CopyData(wzone->vnand, read_pagelist, write_pagelist);
	if(ret != 0) {
		ndprint(RECYCLE_ERROR,"vNand_CopyData error func %s line %d ret=%d\n",
				__FUNCTION__,__LINE__,ret);
		return -1;
	}
	BuffListManager_freeAllList((int)blm,(void **)&read_pagelist,sizeof(PageList));
	BuffListManager_freeAllList((int)blm,(void **)&write_pagelist,sizeof(PageList));

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

	if (zone) {
		ZoneManager_SetPrevZone(context,zone);
		ZoneManager_FreeZone(context,zone);
	}

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
	Zone_Init(new_zone,prev,next);

	return new_zone;
}

/**
 *	fill_ahead_zone - alloc 4 zone beforehand
 *
 *  	@context: global variable
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
static int RecycleReadWrite(Recycle *rep)
{
	Zone *wzone = NULL;
	unsigned int recyclepage = L4UNITSIZE(rep->rZone->vnand) / rep->rZone->vnand->BytePerPage;
	unsigned int zonepage = 0;
	int ret = 0;
	unsigned int write_sectorcount = 0;
	int spp = rep->rZone->vnand->BytePerPage / SECTOR_SIZE;
	unsigned int recyclesector = recyclepage * spp;

	wzone = get_current_write_zone(rep->context);
	if (!wzone)
		wzone = alloc_new_zone_write(rep->context, wzone);
	zonepage = Zone_GetFreePageCount(wzone);
	if (zonepage <= rep->rZone->vnand->v2pp->_2kPerPage) {
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
	if(zonepage >= recyclepage + wzone->vnand->v2pp->_2kPerPage) {
		alloc_update_l1l2l3l4(rep,wzone,rep->writepageinfo,recyclesector);
		ret = copy_data(rep, wzone, recyclepage);
		if(ret != 0) {
			ndprint(RECYCLE_ERROR,"all recycle buflen error func %s line %d \n",
						__FUNCTION__,__LINE__);
			goto err;
		}
	}
	else {
		if(zonepage > wzone->vnand->v2pp->_2kPerPage) {
			alloc_update_l1l2l3l4(rep,wzone,rep->writepageinfo, (zonepage - wzone->vnand->v2pp->_2kPerPage) * spp);
			ret = copy_data(rep, wzone, zonepage - wzone->vnand->v2pp->_2kPerPage);
			if(ret != 0) {
				ndprint(RECYCLE_ERROR,"all recycle buflen error func %s line %d \n",
							__FUNCTION__,__LINE__);
				goto err;
			}

			recyclesector = recyclesector - (zonepage - wzone->vnand->v2pp->_2kPerPage) * spp;
			recyclepage = (recyclesector + spp - 1) / spp;
		}

		wzone = alloc_new_zone_write(rep->context, wzone);
		if(wzone == NULL) {
			ndprint(RECYCLE_ERROR,"alloc new zone func %s line %d \n",
						__FUNCTION__,__LINE__);
			goto err;
		}

		alloc_update_l1l2l3l4(rep,wzone,rep->writepageinfo,recyclesector);
		ret = copy_data(rep, wzone, recyclepage);
		if(ret != 0) {
			ndprint(RECYCLE_ERROR,"all recycle buflen error func %s line %d \n",
						__FUNCTION__,__LINE__);
			goto err;
		}
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
		else if((ret & 0xffff) < 0) {
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

#if 1
static BlockList *create_blocklist(Recycle *rep, int start_blockid, int total_blockcount, int *first_ok_blockid)
{
	int i = 0;
	BlockList *bl = NULL;
	BlockList *bl_node = NULL;
	unsigned int badblockinfo;
	Context *conptr = (Context *)(rep->context);
	int blm = (int)conptr->blm;

	if (rep->force)
		badblockinfo = rep->force_rZone->badblock;
	else
		badblockinfo = rep->rZone->badblock;

	for (i = start_blockid; i < start_blockid + total_blockcount; i++) {
		if (!nm_test_bit(i, &badblockinfo)) {//block is ok
			if (*first_ok_blockid == -1)
				*first_ok_blockid = i;

			if (bl == NULL) {
				bl = (BlockList *)BuffListManager_getTopNode(blm,sizeof(BlockList));
				bl_node = bl;
			} else
				bl_node = (BlockList *)BuffListManager_getNextNode(blm,(void *)bl,sizeof(BlockList));

			bl_node->startBlock = i;
			bl_node->BlockCount = 1;
		}
	}

	return bl;
}
#else
static BlockList *create_blocklist(Recycle *rep, int start_blockid, int total_blockcount, int *first_ok_blockid)
{
	int i = 0;
	int j = 0;
	BlockList *bl = NULL;
	BlockList *bl_node = NULL;
	int blockcount = 0;
	unsigned int badblockinfo;
	Context *conptr = (Context *)(rep->context);
	VNandInfo *vnand = &conptr->vnand;
	int blm = (int)conptr->blm;

	if (rep->force)
		badblockinfo = rep->force_rZone->badblock;
	else
		badblockinfo = rep->rZone->badblock;

	for (i = start_blockid; i < start_blockid + total_blockcount; i++) {
		if (!vNand_IsBadBlock(vnand,i) && !nm_test_bit(j++,&badblockinfo)) {//block is ok
			blockcount++;
			if (*first_ok_blockid == -1)
				*first_ok_blockid = i;
		}
		else {
			if (blockcount) {
				if (bl == NULL) {
					bl = (BlockList *)BuffListManager_getTopNode(blm,sizeof(BlockList));
					bl_node = bl;
				}
				else
					bl_node = (BlockList *)BuffListManager_getNextNode(blm,(void *)bl,sizeof(BlockList));

				bl_node->startBlock = i - blockcount;
				bl_node->BlockCount = blockcount;
				blockcount = 0;
			}
		}
	}

	if (blockcount) {
		if (bl == NULL) {
			bl = (BlockList *)BuffListManager_getTopNode(blm,sizeof(BlockList));
			bl_node = bl;
		}
		else
			bl_node = (BlockList *)BuffListManager_getNextNode(blm,(void *)bl,sizeof(BlockList));

		bl_node->startBlock = start_blockid + total_blockcount - blockcount;
		bl_node->BlockCount = blockcount;
	}

	return bl;
}
#endif

/**
 *	FreeZone - Free recycle zone
 *
 *	@rep: operate object
 */
static int FreeZone ( Recycle *rep)
{
	int ret = 0;
	struct singlelist *pos;
	BlockList *bl = NULL;
	BlockList *bl_node = NULL;
	PageList px;
	PageList *pl = &px;
	NandSigZoneInfo *nandsigzoneinfo = (NandSigZoneInfo *)(rep->rZone->mem0);
	ZoneManager *zonep = ((Context *)(rep->context))->zonep;
	BuffListManager *blm = ((Context *)(rep->context))->blm;
	int start_blockid = rep->rZone->startblockID;
	int next_start_blockid = 0;
	int blockcount = 0;
	int first_ok_blockid = -1;

	if (rep->rZone->ZoneID == zonep->pt_zonenum - 1)
		blockcount = zonep->vnand->TotalBlocks - rep->rZone->startblockID;
	else {
		//next_start_blockid = BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,rep->rZone->ZoneID + 1);
		next_start_blockid = (rep->rZone->ZoneID + 1) * BLOCKPERZONE(zonep->vnand);
		blockcount = next_start_blockid - start_blockid;
	}

	bl = create_blocklist(rep, start_blockid, blockcount, &first_ok_blockid);
	if (bl) {
		ret = vNand_MultiBlockErase(rep->rZone->vnand,bl);
		if(ret < 0) {
			first_ok_blockid = -1;
			singlelist_for_each(pos,&bl->head){
				bl_node = singlelist_entry(pos, BlockList, head);
				if(bl_node->retVal == -1)
					nm_set_bit(bl_node->startBlock - start_blockid, (unsigned int *)&(rep->rZone->sigzoneinfo->badblock));
				else if (-1 == first_ok_blockid)
					first_ok_blockid = bl_node->startBlock;
			}

			if (-1 == first_ok_blockid) {
				ndprint(RECYCLE_ERROR, "ERROR: first_ok_blockid has not found!, %s, line:%d\n", __func__, __LINE__);
				BuffListManager_freeAllList((int)blm,(void **)&bl,sizeof(BlockList));
				rep->rZone = NULL;
				return -1;
			}
		}
		BuffListManager_freeAllList((int)blm,(void **)&bl,sizeof(BlockList));
	}

	pl->pData = (void *)nandsigzoneinfo;
	pl->Bytes = rep->rZone->vnand->BytePerPage;
	pl->retVal = 0;
	(pl->head).next = NULL;
	pl->OffsetBytes = 0;
	pl->startPageID = first_ok_blockid * rep->rZone->vnand->PagePerBlock ;

	memset(nandsigzoneinfo,0xff,rep->rZone->vnand->BytePerPage);
	nandsigzoneinfo->ZoneID = rep->rZone->sigzoneinfo - rep->rZone->top;
	rep->rZone->sigzoneinfo->lifetime++;
	rep->rZone->sigzoneinfo->pre_zoneid = -1;
	rep->rZone->sigzoneinfo->next_zoneid = -1;
	nandsigzoneinfo->lifetime = rep->rZone->sigzoneinfo->lifetime;
	nandsigzoneinfo->badblock = rep->rZone->sigzoneinfo->badblock;

	ret = Zone_RawMultiWritePage(rep->rZone,pl);
	if(ret != 0) {
		ndprint(RECYCLE_ERROR,"Zone Raw multi write page error func %s line %d \n",__FUNCTION__,__LINE__);
		goto err;
	}

	if (rep->junk_zoneid != -1) {
		Release_MaxJunkZone(((Context *)(rep->context))->junkzone, rep->junk_zoneid);
		rep->junk_zoneid = -1;
	}
	else
		Delete_JunkZone(((Context *)(rep->context))->junkzone, rep->rZone->ZoneID);

	ZoneManager_FreeRecyclezone(rep->context,rep->rZone);
	rep->rZone = NULL;
	rep->taskStep = RECYIDLE;
	return 0;
err:
	return -1;
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
	InitNandMutex(&rep->mutex);
	rep->context = context;
	rep->force = 0;
	rep->junk_zoneid = -1;

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
	return MergerSectorID(rep);
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
static int  OnForce_MergerSectorID ( Recycle *rep);
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

	ndprint(RECYCLE_INFO, "start force recycle--------->\n");

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

		recycle_pagecount += rep->force_rZone->sumpage - rep->write_pagecount - 3;
		ZoneManager_FreeRecyclezone(rep->context,rep->force_rZone);
		rep->force_rZone = NULL;
		if (need_pagecount == -1 || recycle_pagecount >= need_pagecount)
			break;

		zoneid = 0xffff;
	}

	OnForce_Deinit(rep);

	ndprint(RECYCLE_INFO, "force recycle finished--------->\n\n");

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
		if (rep->force_junk_zoneid != -1)
			Release_MaxJunkZone(((Context *)(rep->context))->junkzone, rep->force_junk_zoneid);
		return -1;
	}

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

		OnForce_MergerSectorID (rep);

		ret = OnForce_RecycleReadWrite(rep);
		if (ret == -1)
			goto exit;
	}

	ret = OnForce_FreeZone(rep);
exit:
	if (rep->force_junk_zoneid != -1)
		Release_MaxJunkZone(((Context *)(rep->context))->junkzone, rep->force_junk_zoneid);
	else
		Delete_JunkZone(((Context *)(rep->context))->junkzone, rep->force_rZone->ZoneID);
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
	rep->write_pagecount = 0;
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
		rep->force_rZone = zone;
		zone = alloc_new_zone_write(rep->context,zone);
		if(zone == NULL) {
			ndprint(RECYCLE_ERROR,"alloc new zone func %s line %d \n",
						__FUNCTION__,__LINE__);
			return -1;
		}
		ndprint(RECYCLE_INFO, "recycle current write zone...\n");
	}
	else {
		if (ZoneID == 0xffff) {
		GET_FORCE_RECYCLE_JUNKZONE:
			ZoneID = Get_MaxJunkZone(((Context *)(rep->context))->junkzone);
			if ((ZoneID != 0xffff) && rep->rZone && (ZoneID == rep->rZone->ZoneID)) {
				Release_MaxJunkZone(((Context *)(rep->context))->junkzone, ZoneID);
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

		ret = Zone_FindNextPageInfo(rep->force_rZone,pi);
		if (ISNOWRITE(ret)) {
			rep->force_end_findnextpageinfo = 1;
			return 0;
		}
		else if ((ret & 0xffff) < 0) {
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

		ret = Zone_FindNextPageInfo(rep->force_rZone,pi);
		if (ISNOWRITE(ret)) {
			rep->force_end_findnextpageinfo = 1;
			return 0;
		}
		else if((ret & 0xffff) < 0) {
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
	BuffListManager *blm = ((Context *)(rep->context))->blm;

	l4count = rep->force_curpageinfo->L4InfoLen >> 2;
	spp = rep->force_rZone->vnand->BytePerPage / SECTOR_SIZE;
	l4info = (unsigned int *)(rep->force_curpageinfo->L4Info);
	latest_l4info = (unsigned int *)(rep->force_writepageinfo->L4Info);

	pl = NULL;
	tpl = NULL;
	for(i = 0; i < l4count; i += k) {
		k = 1;
		if ((int)l4info[i] == -1)
			continue;

		tmp0 = l4info[i] / spp;
		tmp1 = latest_l4info[i] / spp;

		if ((tmp0 == tmp1 || latest_l4info[i] == -1) && data_in_rzone(rep, tmp0)) {
			for(j = i + 1; j < spp + i && j < l4count; j++) {
				if ((int)l4info[j] == -1)
					break;

				tmp2 = l4info[j] / spp;
				tmp3 = latest_l4info[j] / spp;
				if(tmp2 == tmp0 && (tmp2 == tmp3 || latest_l4info[j] == -1))
					k++;
				else
					break;
			}
			if(tpl == NULL){
				pl = (PageList *)BuffListManager_getTopNode((int)blm, sizeof(PageList));
				tpl = pl;
			}else tpl = (PageList *)BuffListManager_getNextNode((int)blm, (void *)tpl, sizeof(PageList));
			tpl->startPageID = tmp0;
			tpl->OffsetBytes = l4info[i] % spp * SECTOR_SIZE;
			tpl->Bytes = k * SECTOR_SIZE;
			tpl->retVal = 0;
			tpl->pData = NULL;
		}
	}
   	rep->force_pagelist = pl;

	return 0;
}

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

	wzone = get_current_write_zone(rep->context);
	if (!wzone)
		wzone = alloc_new_zone_write(rep->context, wzone);
	zonepage = Zone_GetFreePageCount(wzone);
	if (zonepage == 0) {
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
		recyclepage = (recyclesector + spp - 1) / spp;
	}

	rep->write_pagecount += recyclepage;

	if(zonepage >= recyclepage + wzone->vnand->v2pp->_2kPerPage) {
		alloc_update_l1l2l3l4(rep,wzone,rep->force_writepageinfo,recyclesector);
		ret = copy_data(rep, wzone, recyclepage);
		if(ret != 0) {
			ndprint(RECYCLE_ERROR,"all recycle buflen error func %s line %d \n",
						__FUNCTION__,__LINE__);
			goto err;
		}
	}
	else {
		if(zonepage > wzone->vnand->v2pp->_2kPerPage) {
			alloc_update_l1l2l3l4(rep,wzone,rep->force_writepageinfo,(zonepage - wzone->vnand->v2pp->_2kPerPage) * spp);
			ret = copy_data(rep, wzone, zonepage - wzone->vnand->v2pp->_2kPerPage);
			if(ret != 0) {
				ndprint(RECYCLE_ERROR,"all recycle buflen error func %s line %d \n",
							__FUNCTION__,__LINE__);
				goto err;
			}

			recyclesector = recyclesector - (zonepage - wzone->vnand->v2pp->_2kPerPage) * spp;
			recyclepage = (recyclesector + spp - 1) / spp;
		}
		else if (zonepage == wzone->vnand->v2pp->_2kPerPage)
			rep->write_pagecount += wzone->vnand->v2pp->_2kPerPage;

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
	int ret = 0;
	struct singlelist *pos;
	BlockList *bl = NULL;
	BlockList *bl_node = NULL;
	PageList px;
	PageList *pl = &px;
	NandSigZoneInfo *nandsigzoneinfo = (NandSigZoneInfo *)(rep->force_rZone->mem0);
	ZoneManager *zonep = ((Context *)(rep->context))->zonep;
	BuffListManager *blm = ((Context *)(rep->context))->blm;
	int start_blockid = rep->force_rZone->startblockID;
	int next_start_blockid = 0;
	int blockcount = 0;
	int first_ok_blockid = -1;

	if (rep->force_rZone->ZoneID == zonep->pt_zonenum - 1)
		blockcount = zonep->vnand->TotalBlocks - rep->force_rZone->startblockID;
	else {
		//next_start_blockid = BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,rep->force_rZone->ZoneID + 1);
		next_start_blockid = (rep->force_rZone->ZoneID + 1) * BLOCKPERZONE(zonep->vnand);
		blockcount = next_start_blockid - start_blockid;
	}

	bl = create_blocklist(rep, start_blockid, blockcount, &first_ok_blockid);
	if (bl) {
		ret = vNand_MultiBlockErase(rep->force_rZone->vnand,bl);
		if(ret < 0) {
			first_ok_blockid = -1;
			singlelist_for_each(pos,&bl->head){
				bl_node = singlelist_entry(pos, BlockList, head);
				if(bl_node->retVal == -1)
					nm_set_bit(bl_node->startBlock - start_blockid, (unsigned int *)&(rep->force_rZone->sigzoneinfo->badblock));
				else if (-1 == first_ok_blockid)
					first_ok_blockid = bl_node->startBlock;
			}
			if (-1 == first_ok_blockid) {
				ndprint(RECYCLE_ERROR, "ERROR: first_ok_blockid has not found!, %s, line:%d\n", __func__, __LINE__);
				BuffListManager_freeAllList((int)blm,(void **)&bl,sizeof(BlockList));
				rep->force_rZone = NULL;
				return -1;
			}
		}
		BuffListManager_freeAllList((int)blm,(void **)&bl,sizeof(BlockList));
	}

	pl->pData = (void *)nandsigzoneinfo;
	pl->Bytes = rep->force_rZone->vnand->BytePerPage;
	pl->retVal = 0;
	(pl->head).next = NULL;
	pl->OffsetBytes = 0;
	pl->startPageID = first_ok_blockid * rep->force_rZone->vnand->PagePerBlock ;

	memset(nandsigzoneinfo,0xff,rep->force_rZone->vnand->BytePerPage);
	nandsigzoneinfo->ZoneID = rep->force_rZone->sigzoneinfo - rep->force_rZone->top;
	rep->force_rZone->sigzoneinfo->lifetime++;
	rep->force_rZone->sigzoneinfo->pre_zoneid = -1;
	rep->force_rZone->sigzoneinfo->next_zoneid = -1;
	nandsigzoneinfo->lifetime = rep->force_rZone->sigzoneinfo->lifetime;
	nandsigzoneinfo->badblock = rep->force_rZone->sigzoneinfo->badblock;

	ret = Zone_RawMultiWritePage(rep->force_rZone,pl);
	if(ret != 0) {
		ndprint(RECYCLE_ERROR,"Zone Raw multi write page error func %s line %d \n",__FUNCTION__,__LINE__);
		goto err;
	}

	return 0;
err:
	return -1;
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
			else if((ret & 0xffff) < 0) {
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
		if (ret == -1)
			goto exit;

		ret = OnForce_FindValidSector(rep);
		if (ret == -1)
			goto exit;

		OnForce_MergerSectorID (rep);

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

	ndprint(RECYCLE_INFO, "follow recycle finished--------->\n\n");

exit:
	OnForce_Deinit(rep);
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
	rep->force_rZone = zonep->last_zone;

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
	Zone_Init(zone,prev,next);

	return 0;
}

/**
 *	Recycle_OnBootRecycle - boot recycle
 *
 *	@context: global variable
 */
int Recycle_OnBootRecycle ( int context )
{
	int i;
	int ret = 0;
	Recycle *rep = ((Context *)context)->rep;

	ndprint(RECYCLE_INFO, "start boot recycle--------->\n");

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

		OnForce_MergerSectorID (rep);

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

	ndprint(RECYCLE_INFO, "boot recycle finished--------->\n\n");

exit:
	OnForce_Deinit(rep);
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
