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
#include "taskmanager.h"
#include "nanddebug.h"
#include "nandsigzoneinfo.h"

#define L4UNITSIZE(context) 128 * 1024
#define BLOCKPERZONE(context)   	8
#define RECYCLECACHESIZE		VNANDCACHESIZE

static int getRecycleZone ( Recycle *rep);
static int FindFirstPageInfo ( Recycle *rep);
static int FindValidSector ( Recycle *rep);
static int MergerSectorID ( Recycle *rep);
static int RecycleReadWrite ( Recycle *rep);
static int FindNextPageInfo ( Recycle *rep);
static int FreeZone ( Recycle *rep );
int Recycle_OnForceRecycle ( int context );

/** 
 *	Recycle_OnFragmentHandle - Process of normal recycle
 *
 *	@context: global variable
 */
int Recycle_OnFragmentHandle ( int context )
{
	int ret = 0;
	Recycle *rep = ((Context *)context)->rep;

	switch(rep->taskStep) {
		case RECYSTART:
		case GETZONE:
			ret = getRecycleZone(rep);
			if (ret == -1)
				goto ERROR;
			break;
		case READFIRSTINFO:
			ret = FindFirstPageInfo(rep);
			if (ret == -1)
				goto ERROR;
			break;
		case FINDVAILD:
			ret = FindValidSector(rep);
			if (ret == -1)
				goto ERROR;
			break;
		case MERGER:
			ret = MergerSectorID(rep);
			if (ret == -1)
				goto ERROR;
			break;
		case RECYCLE:
			ret = RecycleReadWrite(rep);
			if (ret == -1)
			 goto ERROR;
			break;
		case READNEXTINFO:
			ret = FindNextPageInfo(rep);
			if (ret == -1)
				goto ERROR;
			break;
		case FINISH:
			ret = FreeZone(rep);
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
 *	is_suspend - whether suspend or not
 *
 *	@rep: operate object
 */
static int is_suspend(Recycle *rep)
{
	int suspend;
	
	NandMutex_Lock(&rep->suspend_mutex);
	suspend = rep->suspend;
	NandMutex_Unlock(&rep->suspend_mutex);

	return suspend;
}
/** 
 *	do_nothing - do nothing
 *
 *	@rep: operate object
 */
static void do_nothing(Recycle *rep)
{
	Semaphore_wait(&rep->sem);
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

	rep->record_writeadd = (unsigned int *)Nand_ContinueAlloc(conptr->zonep->l4infolen);
	if(rep->record_writeadd == NULL) {
		ndprint(1,"Force recycle alloc error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}

	rep->force = 0;
	if (rep->taskStep == RECYIDLE)
		rep->taskStep = RECYSTART;

	while(1) {
		if (ret == -1 || rep->taskStep == RECYIDLE)
			break;

		if (is_suspend(rep))
			do_nothing(rep);
		else
			ret = Recycle_OnFragmentHandle(context);
	}
	
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
 *	alloc_pageinfo  -  alloc L1Info, L2Info, L3Info and L4Info of pageinfo
 *
 *	@rep: to konw whether L2InfoLen and L3InfoLen are 0 or not
 */
static PageInfo *alloc_pageinfo(Recycle *rep)
{
	PageInfo *pi = NULL;
	ZoneManager *zonep = ((Context *)(rep->context))->zonep;
	
	pi = (PageInfo *)Nand_VirtualAlloc(sizeof(PageInfo));
	if (!pi) {
		ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	if (zonep->l2infolen) {
		pi->L2Info = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * zonep->l2infolen);
		if (!(pi->L2Info)) {
			ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
			goto ERROR1;
		}
	}
	
	if (zonep->l3infolen) {
		pi->L3Info = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * zonep->l3infolen);
		if (!(pi->L3Info)) {
			ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
			goto ERROR2;
		}
	}

	pi->L4Info = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * zonep->l4infolen);
	if (!(pi->L4Info)) {
		ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		goto ERROR3;
		return NULL;
	}
	
	pi->L1InfoLen = zonep->vnand->BytePerPage;
	pi->L2InfoLen = zonep->l2infolen;
	pi->L3InfoLen = zonep->l3infolen;
	pi->L4InfoLen = zonep->l4infolen;

	return pi;

ERROR3:
	if (zonep->l3infolen)
		Nand_VirtualFree(pi->L3Info);
ERROR2:
	if (zonep->l2infolen)
		Nand_VirtualFree(pi->L2Info);
ERROR1:
	return NULL;
}

/**
 *	free_pageinfo  -  free L1Info, L2Info, L3Info and L4Info of pageinfo
 *
 *	@rep: operate object
 *	@pageinfo: which need to free
 */
static void free_pageinfo(Recycle *rep, PageInfo *pageinfo)
{
	ZoneManager *zonep = NULL;

	if (!pageinfo)
		return;
		
	zonep = ((Context *)(rep->context))->zonep;

	if (zonep->l2infolen)
		Nand_VirtualFree(pageinfo->L2Info);
	
	if (zonep->l3infolen)
		Nand_VirtualFree(pageinfo->L3Info);

	Nand_VirtualFree(pageinfo->L4Info);
	Nand_VirtualFree(pageinfo);
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
		ndprint(1,"PANIC ERROR func %s line %d \n",
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
		ndprint(1,"PANIC ERROR func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;			
	}

	minlifetime = ZoneManager_Getminlifetime(context);
	maxlifetime = ZoneManager_Getmaxlifetime(context);
	lifetime = minlifetime + ( maxlifetime - minlifetime ) / 3;

	if (lifetime == minlifetime)
		return ZoneManager_ForceRecyclezoneID(context,lifetime + 1);

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

	ZoneID = get_normal_zoneID(context);
	if(ZoneID == 0xffff) {
		ndprint(1,"PANIC ERROR func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;			
	}

	NandMutex_Lock(&rep->mutex);

	rep->rZone = ZoneManager_AllocRecyclezone(context,ZoneID);
	if(rep->rZone == NULL) {
		ndprint(1,"alloc recycle zone error func %s line %d \n",
					__FUNCTION__,__LINE__);
		ret = -1;
		goto err;
	}
	rep->taskStep = READFIRSTINFO;	
err:
	NandMutex_Unlock(&rep->mutex);
	return ret;
}

/** 
 *	read_no_context - whether is no write data before read
 *
 *	@ret: return value of read operation
 */
static int read_no_context(int ret)
{
	return (ret & 0xffff) == -6;
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
	PageInfo *prev_pi = NULL;
	PageInfo *pi = NULL;
	
	prev_pi = alloc_pageinfo(rep);
	if(prev_pi == NULL) {
		ndprint(1,"force recycle alloc pageinfo error func %s line %d \n",
				__FUNCTION__,__LINE__);
		return -1;
	}

	NandMutex_Lock(&rep->mutex);
	
	ret = Zone_FindFirstPageInfo(rep->rZone,prev_pi);
	if((ret & 0xffff) < 0) {
		ndprint(1,"Find First Pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
		goto err1;			
	}
	
	rep->curpageinfo = prev_pi;
	do{
		if(rep->rZone->NextPageInfo == 0) {
			rep->end_findnextpageinfo = 1;
			goto exit;
		}

		pi = alloc_pageinfo(rep);
		if(pi == NULL) {
			ndprint(1,"force recycle alloc pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
			goto err0;
		}
		
		ret = Zone_FindNextPageInfo(rep->rZone,pi);
		if (read_no_context(ret)) {
			rep->end_findnextpageinfo = 1;
			goto exit;
		}
		else if((ret & 0xffff) < 0) {
			ndprint(1,"find next pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
			goto err1;			
		}

		rep->prevpageinfo = rep->curpageinfo;
		rep->curpageinfo = pi;
	}while(is_same_L4(rep->prevpageinfo, rep->curpageinfo));
	
	rep->nextpageinfo = rep->curpageinfo;
	rep->curpageinfo = rep->prevpageinfo;

exit:
	rep->taskStep = FINDVAILD;
	NandMutex_Unlock(&rep->mutex);
	return 0;

err1:
	free_pageinfo(rep,pi);
err0:
	NandMutex_Unlock(&rep->mutex);
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

	if (l1index == 0xffff)
		return -1;

	if(pi->L3InfoLen == 0)
		start_sectorID = l1index * l1unitlen;
	else {
		if (l3index == 0xffff)
			return -1;

		if(pi->L2InfoLen == 0)
			start_sectorID = l1index * l1unitlen + l3index * l3unitlen;
		else {
			if (l2index == 0xffff)
				return -1;
			
			start_sectorID = l3index * l3unitlen + 
				l2index * l2unitlen + l1index * l1unitlen;
		}
	}

	NandMutex_Lock(&rep->mutex);
	rep->startsectorID = start_sectorID;
	rep->writepageinfo = get_pageinfo_from_cache(rep,rep->startsectorID);
	rep->taskStep = MERGER;
	NandMutex_Unlock(&rep->mutex);

	return 0;
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
	
	return pageid >= zonep->startblockID[zoneid] * ppb
		&& pageid < zonep->startblockID[zoneid + 1] * ppb;
}

/**
 *	data_in_prev_zone - whether L4 data in previous of recycle zone or not
 *
 *	@rep: operate object
 *	@pageid: number of page
*/
static int data_in_prev_zone (Recycle *rep, unsigned int pageid)
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
	zoneid = zone->prevzone- zone->top;
	ppb = vnand->PagePerBlock;
	
	return pageid >= zonep->startblockID[zoneid] * ppb
		&& pageid < zonep->startblockID[zoneid + 1] * ppb;
}

/**
 *	data_in_next_zone - whether L4 data in next of recycle zone or not
 *
 *	@rep: operate object
 *	@pageid: number of page
*/
static int data_in_next_zone (Recycle *rep, unsigned int pageid)
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
	zoneid = zone->nextzone- zone->top;
	ppb = vnand->PagePerBlock;
	
	return pageid >= zonep->startblockID[zoneid] * ppb
		&& pageid < zonep->startblockID[zoneid + 1] * ppb;
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
	unsigned int l4count = 0;
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int k = 0;
	unsigned int *l4info = NULL;
	unsigned int *latest_l4info = NULL;
	unsigned int spp = 0;
	int first_flag = 1;
	BuffListManager *blm = ((Context *)(rep->context))->blm;

	l4count = rep->curpageinfo->L4InfoLen >> 2;
	spp = rep->rZone->vnand->BytePerPage / SECTOR_SIZE;
	l4info = (unsigned int *)(rep->curpageinfo->L4Info);
	latest_l4info = (unsigned int *)(rep->writepageinfo->L4Info);

	NandMutex_Lock(&rep->mutex);
	pl = (PageList *)BuffListManager_getTopNode((int)blm, sizeof(PageList));

	rep->write_cursor = 0;

	for(i = 0; i < l4count; i += k) {
		k = 1;
		if ((int)l4info[i] == -1)
			continue;

		tmp0 = l4info[i] / spp;
		tmp1 = latest_l4info[i] / spp;

		if (tmp0 == tmp1 && data_in_3_zone(rep, tmp0)) {
			if (first_flag) {
				rep->write_cursor = i;
				first_flag = 0;
			}
			
			for(j = i + 1; j < spp + i && j < l4count; j++) {
				if ((int)l4info[j] == -1)
					break;
				
				tmp2 = l4info[j] / spp;
				if(tmp2 == tmp0)
					k++;
				else
					break;	
			}

			tpl = (PageList *)BuffListManager_getNextNode((int)blm, (void *)pl, sizeof(PageList));
			tpl->startPageID = tmp0;
			tpl->OffsetBytes = l4info[i] % spp;
			tpl->Bytes = k * SECTOR_SIZE;
			tpl->retVal = 0;
			tpl->pData = NULL;
		}
	}

	BuffListManager_freeList((int)blm, (void **)&pl, (void *)pl, sizeof(PageList));
	rep->pagelist = pl;

	rep->taskStep = RECYCLE;
	NandMutex_Unlock(&rep->mutex);
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
static void alloc_update_l1l2l3l4(Recycle *rep,Zone *wzone,PageInfo *pi, unsigned int count)
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
	unsigned int startsectorid;
	unsigned int *record_writeaddr;
	unsigned int write_cursor;
	Zone *rzone;
	Context *conptr = (Context *)(rep->context);
	CacheManager *cachemanager = conptr->cachemanager;
	VNandInfo *vnand = &conptr->vnand;
	unsigned int spp = vnand->BytePerPage / SECTOR_SIZE;
	
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
		write_cursor = rep->force_write_cursor;
		rzone = rep->force_rZone;
		rep->force_alloc_num = count;
		pi->zoneID = rep->force_rZone->ZoneID;
	}
	else {
		startsectorid = rep->startsectorID;
		record_writeaddr = rep->record_writeadd;
		write_cursor = rep->write_cursor;
		rzone = rep->rZone;
		rep->alloc_num = count;
		pi->zoneID = rep->rZone->ZoneID;
	}
	
	l1index = startsectorid / l1unitlen;
	pi->PageID = Zone_AllocNextPage(wzone);

	l1buf[l1index] = pi->PageID;
	pi->L1Index = l1index;

	for(i = write_cursor; i < count * spp + write_cursor; i++) {
		if ((i - write_cursor) % spp == 0)
			pageid = Zone_AllocNextPage(wzone);

		record_writeaddr[i] = pageid * spp + (i - write_cursor) % spp;
		l4buf[i] = record_writeaddr[i];
	}

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
	PageList *pv = NULL;
	struct singlelist *sg = NULL;
	Zone *rzone = NULL;
	unsigned int datalen = 0;
	unsigned int offset = 0;
	unsigned int len = 0;
	unsigned int flag = 0;

	if (rep->force) {
		rzone = rep->force_rZone;
		pl = rep->force_pagelist;
		pv = rep->force_pagelist;
	}
	else {
		rzone = rep->rZone;
		pl = rep->pagelist;
		pv = rep->pagelist;
	}

	datalen = pagenum * rzone->vnand->BytePerPage;
	len = datalen;

	singlelist_for_each(sg,&pl->head) {
		px = singlelist_entry(sg,PageList,head);
		datalen -= px->Bytes;
		if((int)datalen <= 0)
			break;
		pv = px;
	}

	if(offset > len) {
		len = offset - len;
		px->Bytes = px->Bytes - len;
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
		pv->head.next = NULL;
		px->OffsetBytes = px->Bytes;
		px->Bytes = len;
		if (rep->force) {
			px->head.next = &rep->force_pagelist->head;
			rep->force_pagelist = px;
		}
		else {
			px->head.next = &rep->pagelist->head;
			rep->pagelist = px;
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
	unsigned int write_cursor;
	unsigned alloc_num;
	unsigned int *addr;
	VNandInfo *vnand = &((Context *)(rep->context))->vnand;
	unsigned int spp = vnand->BytePerPage / SECTOR_SIZE;
	BuffListManager *blm = ((Context *)(rep->context))->blm;

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

	pl = (PageList *)BuffListManager_getTopNode((int)blm, sizeof(PageList));

	for(i = 0 ; i < len ; i++) {
		alloc_num--;
		px = (PageList *)BuffListManager_getNextNode((int)blm, (void *)pl, sizeof(PageList));
		if(px == NULL) {
			ndprint(1,"PANIC ERROR func %s line %d \n",__FUNCTION__,__LINE__);
			return -1;
		}
		px->startPageID =  addr[write_cursor] / spp;
		px->OffsetBytes = 0;
		px->Bytes = vnand->BytePerPage;
		px->retVal = 0;
		write_cursor += spp;
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

	if (rep->force) {
		writepageinfo = rep->force_writepageinfo;
		bufpage = rep->force_buflen / rep->force_rZone->vnand->BytePerPage;
	}
	else {
		writepageinfo = rep->writepageinfo;
		bufpage = rep->buflen / rep->rZone->vnand->BytePerPage;
	}

	for( i = 0 ; i < count; i++) {
		ret = Create_read_pagelist(rep,bufpage);
		if(ret != 0) {
			ndprint(1,"creat read pagelist error func %s line %d \n",
							__FUNCTION__,__LINE__);
			return -1;				
		}

		ret = Create_write_pagelist(rep,bufpage);
		if(ret != 0) {
			ndprint(1,"creat write pagelist error func %s line %d \n",
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
			ret = Zone_MultiWritePage(wzone,0,NULL,writepageinfo);
			if(ret != 0) {
				ndprint(1,"zone multi write error func %s line %d \n",
							__FUNCTION__,__LINE__);
				return -1;			
			}
		}

		ret = vNand_CopyData(wzone->vnand, read_pagelist, write_pagelist);
		if(ret != 0) {
			ndprint(1,"vNand_CopyData error func %s line %d \n",
							__FUNCTION__,__LINE__);
			return -1;				
		}
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

	if(numpage == 0)
		return 0;

	ret = Create_read_pagelist(rep,numpage);
	if(ret != 0) {
		ndprint(1,"creat read pagelist error func %s line %d \n",
						__FUNCTION__,__LINE__);
		return -1;				
	}
	
	ret = Create_write_pagelist(rep,numpage);
	if(ret != 0) {
		ndprint(1,"creat write pagelist error func %s line %d \n",
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
		ret = Zone_MultiWritePage(wzone,0,NULL,writepageinfo);
		if(ret != 0) {
			ndprint(1,"zone multi write error func %s line %d \n",
						__FUNCTION__,__LINE__);
			return -1;			
		}
	}
	
	ret = vNand_CopyData(wzone->vnand, read_pagelist, write_pagelist);
	if(ret != 0) {
		ndprint(1,"vNand_CopyData error func %s line %d \n",
						__FUNCTION__,__LINE__);
		return -1;				
	}
	
	return 0;
}


/** 
 *	write_data_prepare - alloc 4 zone beforehand
 *         
 *  	@context: global variable
 */
static int write_data_prepare ( int context )
{
	int i;
	unsigned int count = 0;
	Zone *zone = NULL;
	Context *conptr = (Context *)context;
	Message force_recycle_msg;
	int msghandle;

	count = ZoneManager_GetAheadCount(context);
	for (i = 0; i < 4 - count; i++){
		zone = ZoneManager_AllocZone(context);
		if (!zone) {
			/* force recycle */
			ndprint(1,"start force recycle------->\n");

			force_recycle_msg.msgid = FORCE_RECYCLE_ID;
			force_recycle_msg.prio = FORCE_RECYCLE_PRIO;
			force_recycle_msg.data = context;
			
			msghandle = Message_Post(conptr->thandle, &force_recycle_msg, WAIT);
			Message_Recieve(conptr->thandle, msghandle);
			
			zone = ZoneManager_AllocZone(context);
		}
		ZoneManager_SetAheadZone(context,zone);
	}	

	count = ZoneManager_GetAheadCount(context);
	if (count > 0 && count < 4) {
		ndprint(1,"WARNING: can't alloc four zone beforehand, free zone count is %d \n", count);
		return 0;
	}
	else if (count == 0){
		ndprint(1,"ERROR: There is no free zone exist!!!!!! \n");
		return -1;
	}

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

	ZoneManager_SetPrevZone(context,zone);
	ZoneManager_FreeZone(context,zone);

	if (ZoneManager_GetAheadCount(context) == 0)
		write_data_prepare(context);

	ZoneManager_GetAheadZone(context, &new_zone);
	ZoneManager_SetCurrentWriteZone(context,new_zone);
	prev = ZoneManager_GetPrevZone(context);
	next = ZoneManager_GetNextZone(context);
	Zone_Init(new_zone,prev,next);

	return new_zone;
}

/**
 *	get_writepagecount - get count of page in write operation
 *
 *	@pagelist: calculate object
*/
static int get_pagecount(PageList *pagelist)
{
	struct singlelist *pos;
	int pagecount = 0;

	singlelist_for_each(pos, &pagelist->head)
		pagecount++;

	return pagecount;
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
			ndprint(1,"all recycle buflen error func %s line %d \n",
						__FUNCTION__,__LINE__);
			return -1;
		}
	}
	
	if (temppage1) {
		ret = part_recycle_buflen(rep,wzone,temppage1,!temppage);
		if(ret != 0) {
			ndprint(1,"part recycle buflen error func %s line %d \n",
						__FUNCTION__,__LINE__);
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
	unsigned int write_pagecount = 0;

	wzone = get_current_write_zone(rep->context);
	zonepage = Zone_GetFreePageCount(wzone);
	if (zonepage == 0) {
		wzone = alloc_new_zone_write(rep->context, wzone);
		zonepage = Zone_GetFreePageCount(wzone);
	}

	write_pagecount = get_pagecount(rep->pagelist);
	if (write_pagecount == 0)
		goto exit;
	else if (write_pagecount < recyclepage)
		recyclepage = write_pagecount;
	
	NandMutex_Lock(&rep->mutex);

	if(zonepage >= recyclepage + 1) {
		alloc_update_l1l2l3l4(rep,wzone,rep->writepageinfo,recyclepage);
		ret = copy_data(rep, wzone, recyclepage);
		if(ret != 0) {
			ndprint(1,"all recycle buflen error func %s line %d \n",
						__FUNCTION__,__LINE__);
			return -1;
		}
	}
	else {
		if(zonepage > 1) {
			alloc_update_l1l2l3l4(rep,wzone,rep->writepageinfo, zonepage -1);
			ret = copy_data(rep, wzone, zonepage -1);
			if(ret != 0) {
				ndprint(1,"all recycle buflen error func %s line %d \n",
							__FUNCTION__,__LINE__);
				return -1;
			}
			
			recyclepage = recyclepage - (zonepage - 1);
		}

		wzone = alloc_new_zone_write(rep->context, wzone);
		if(wzone == NULL) {
			ndprint(1,"zonemanager alloc zone func %s line %d \n",
						__FUNCTION__,__LINE__);
			return -1;
		}

		alloc_update_l1l2l3l4(rep,wzone,rep->writepageinfo,recyclepage);
		ret = copy_data(rep, wzone, recyclepage);
		if(ret != 0) {
			ndprint(1,"all recycle buflen error func %s line %d \n",
						__FUNCTION__,__LINE__);
			return -1;
		}
	}
	
exit:
	write_data_prepare(rep->context);
	rep->prevpageinfo = rep->curpageinfo;
	give_pageinfo_to_cache(rep,rep->writepageinfo);

	if (rep->end_findnextpageinfo) {
		rep->taskStep = FINISH;
		rep->end_findnextpageinfo = 0;
	}
	else
		rep->taskStep = READNEXTINFO;
	
	NandMutex_Unlock(&rep->mutex);

	return 0;
}

/** 
 *	FindNextPageInfo - Find next pageinfo of recycle zone
 *
 *	@rep: operate object
 */
static int FindNextPageInfo ( Recycle *rep)
{
	int ret = 0;
	PageInfo *pi = NULL;

	NandMutex_Lock(&rep->mutex);
	
	rep->curpageinfo = rep->nextpageinfo;
	do{
		if(rep->rZone->NextPageInfo == 0) {
			rep->end_findnextpageinfo = 1;
			goto exit;
		}

		pi = alloc_pageinfo(rep);
		if(pi == NULL) {
			ndprint(1,"recycle alloc pageinfo fail func %s line %d \n",
						__FUNCTION__,__LINE__);
			NandMutex_Unlock(&rep->mutex);
			return -1;
		}
		
		ret = Zone_FindNextPageInfo(rep->rZone,pi);
		if (read_no_context(ret)) {
			rep->end_findnextpageinfo = 1;
			goto exit;
		}
		else if((ret & 0xffff) < 0) {
			ndprint(1,"find next pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
			free_pageinfo(rep,pi);
			NandMutex_Unlock(&rep->mutex);
			return -1;			
		}

		rep->prevpageinfo = rep->curpageinfo;
		rep->curpageinfo = pi;
	}while(is_same_L4(rep->prevpageinfo, rep->curpageinfo));

	rep->nextpageinfo = rep->curpageinfo;
	rep->curpageinfo = rep->prevpageinfo;

exit:
	rep->taskStep = FINDVAILD;
	NandMutex_Unlock(&rep->mutex);

	return 0;
}

/** 
 *	zonemanager_is_badblock - whether block is bad or not
 *
 *	@vnand: virtual nand
 *	@blockno: number of block
 */
static int is_badblock_in_boot(VNandInfo *vnand, int blockno)
{
	int i;

	for (i = 0; i < vnand->BytePerPage >> 2; i++) {
		if (blockno != 0xffffffff && vnand->pt_badblock_info[i] == blockno)
			return 1;
	}

	return 0;
}

/** 
 *	FreeZone - Free recycle zone
 *
 *	@rep: operate object
 */
static int FreeZone ( Recycle *rep)
{
	int ret = 0;
	BlockList bx;
	BlockList *bl = &bx;
	PageList px;
	PageList *pl = &px;
	NandSigZoneInfo *nandsigzoneinfo = (NandSigZoneInfo *)(rep->rZone->mem0);
	int start_blockno = rep->rZone->startblockID;
	int next_start_blockno = ((Context *)(rep->context))->zonep->startblockID[rep->rZone->ZoneID + 1];
	int blockcount = next_start_blockno - start_blockno;
	int blockno = start_blockno;
	unsigned int i = 0;
	unsigned int short badblock = 0;

	bl->startBlock = start_blockno;
	bl->BlockCount = blockcount;
	bl->retVal = 0;
	(bl->head).next = NULL;

	pl->pData = (void *)nandsigzoneinfo;
	pl->Bytes = rep->rZone->vnand->BytePerPage;
	pl->retVal = 0;
	(pl->head).next = NULL;
	pl->OffsetBytes = 0;

	NandMutex_Lock(&rep->mutex);
	ret = vNand_MultiBlockErase(rep->rZone->vnand,bl);	
	if(ret < 0) {
		ndprint(1,"Multi block erase error func %s line %d \n"
					,__FUNCTION__,__LINE__);
		return -1;
	}

	while(i < blockcount) {
		if (vNand_IsBadBlock(rep->rZone->vnand,blockno)
			&& !is_badblock_in_boot(rep->rZone->vnand,blockno))
			badblock |= (1<< i);

		blockno++;
		i++;
	}

	while(i < 16) {
		if(!(badblock & (1 <<i)))
			break;
		i++;	
	}

	pl->startPageID = (start_blockno + i)* rep->rZone->vnand->PagePerBlock ;

	memset(nandsigzoneinfo,0xff,rep->rZone->vnand->BytePerPage);
	nandsigzoneinfo->ZoneID = rep->rZone->sigzoneinfo - rep->rZone->top;
	rep->rZone->sigzoneinfo->lifetime++;
	nandsigzoneinfo->lifetime = rep->rZone->sigzoneinfo->lifetime;
	nandsigzoneinfo->badblock = rep->rZone->sigzoneinfo->badblock;

	ret = Zone_RawMultiWritePage(rep->rZone,pl);
	if(ret != 0) {
		ndprint(1,"Zone Raw multi write page error func %s line %d \n",__FUNCTION__,__LINE__);
		goto err;
	}

	ZoneManager_FreeRecyclezone(rep->context,rep->rZone);
	free_pageinfo(rep,rep->curpageinfo);
	rep->taskStep = RECYIDLE;
	NandMutex_Unlock(&rep->mutex);

	return 0;
err:
	free_pageinfo(rep,rep->curpageinfo);
	NandMutex_Unlock(&rep->mutex);
	return -1;
	
}

/** 
 *	Recycle_Init - Initialize operation
 *
 *	@context: global variable
 */
int Recycle_Init(int context)
{
	Context *conptr = (Context *)context;
	Recycle *rep = NULL;
	conptr->rep = (Recycle *)Nand_ContinueAlloc(sizeof(Recycle));
	rep = conptr->rep;
	if(conptr->rep == NULL) {
		ndprint(1,"Recycle init alloc recycle error func %s line %d \n",__FUNCTION__,__LINE__);	
		return -1;
	}

	memset(conptr->rep,0x0,sizeof(Recycle));
	
	rep->taskStep = RECYSTART;
	rep->rZone = NULL;
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
	rep->suspend = 0;
	InitNandMutex(&rep->suspend_mutex);
	InitSemaphore(&rep->sem,0);

	return 0;
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

	Nand_ContinueFree(rep->record_writeadd);
	Nand_ContinueFree(rep);
}

/** 
 *	Recycle_Suspend - suspend recycle process
 *
 *	@context: global variable
 */
int Recycle_Suspend ( int context )
{
	Context *conptr = (Context *)context;
	Recycle *rep = conptr->rep;

	NandMutex_Lock(&rep->suspend_mutex);
	rep->suspend = 1;
	NandMutex_Unlock(&rep->suspend_mutex);

	ndprint(0, "recycle suspend...........\n");

	return 0;
}

/** 
 *	Recycle_Resume -resume recycle process
 *
 *	@context: global variable
 */
int Recycle_Resume ( int context )
{
	Context *conptr = (Context *)context;
	Recycle *rep = conptr->rep;

	ndprint(0, "recycle resume...........\n");

	NandMutex_Lock(&rep->suspend_mutex);
	rep->suspend = 0;
	NandMutex_Unlock(&rep->suspend_mutex);

	Semaphore_signal(&rep->sem);

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
static int OnForce_Init ( Recycle *rep );
static int OnForce_GetRecycleZone ( Recycle *rep );
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
 *	@context: global variable
 */
int Recycle_OnForceRecycle ( int context )
{
	int i;
	int ret = 0;
	Recycle *rep = ((Context *)context)->rep;

	ndprint(0, "start force recycle--------->\n");
	
	ret = OnForce_Init(rep);
	if (ret == -1)
		goto ERROR;

	ret = OnForce_GetRecycleZone(rep);
	if (ret == -1)
		goto ERROR;

	for (i = 0; ; i++) {
		if (rep->force_end_findnextpageinfo)
			break;
		
		if (i == 0)
			ret = OnForce_FindFirstValidPageInfo(rep);
		else
			ret = OnForce_FindNextValidPageInfo(rep);
		if (ret == -1)
			goto ERROR;

		ret = OnForce_FindValidSector(rep);
		if (ret == -1)
			goto ERROR;

		ret = OnForce_MergerSectorID (rep);
		if (ret == -1)
			goto ERROR;

		ret = OnForce_RecycleReadWrite(rep);
		if (ret == -1)
			goto ERROR;
	}
	
	ret = OnForce_FreeZone(rep);
	if (ret == -1)
		goto ERROR;

	OnForce_Deinit(rep);
	
	ndprint(0, "force recycle finished--------->\n\n");
	
ERROR:
	return ret;
}

/** 
 *	OnForce_Init - Force recycle initialize operation
 *
 *	@rep: operate object
 */
static int OnForce_Init ( Recycle *rep )
{
	ZoneManager *zonep = ((Context *)(rep->context))->zonep;

	rep->force_record_writeadd = (unsigned int *)Nand_ContinueAlloc(zonep->l4infolen);
	if(rep->force_record_writeadd == NULL) {
		ndprint(1,"Force recycle alloc error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}

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
	rep->force = 1;

	return 0;
}

/** 
 *	OnForce_GetRecycleZone - Get force recycle zone
 *
 *	@rep: operate object
 */
static int OnForce_GetRecycleZone ( Recycle *rep)
{
	unsigned short ZoneID = 0;

	ZoneID = get_force_zoneID(rep->context);
	if(ZoneID == 0xffff) {
		ndprint(1,"PANIC ERROR func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;			
	}

	rep->force_rZone = ZoneManager_AllocRecyclezone(rep->context,ZoneID);
	if(rep->force_rZone == NULL) {
		ndprint(1,"alloc force recycle zone error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
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
	PageInfo *prev_pi = NULL;
	PageInfo *pi = NULL;
	
	prev_pi = alloc_pageinfo(rep);
	if(prev_pi == NULL) {
		ndprint(1,"force recycle alloc pageinfo error func %s line %d \n",
				__FUNCTION__,__LINE__);
		return -1;
	}
	
	ret = Zone_FindFirstPageInfo(rep->force_rZone,prev_pi);
	if((ret & 0xffff) < 0) {
		ndprint(1,"Find First Pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
		free_pageinfo(rep,prev_pi);
		return -1;			
	}
	
	rep->force_curpageinfo = prev_pi;
	do{
		if(rep->force_rZone->NextPageInfo == 0) {
			rep->force_end_findnextpageinfo = 1;
			return 0;
		}

		pi = alloc_pageinfo(rep);
		if(pi == NULL) {
			ndprint(1,"force recycle alloc pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
			return -1;
		}
		
		ret = Zone_FindNextPageInfo(rep->force_rZone,pi);
		if (read_no_context(ret)) {
			rep->force_end_findnextpageinfo = 1;
			return 0;
		}
		else if((ret & 0xffff) < 0) {
			ndprint(1,"find next pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
			free_pageinfo(rep,pi);
			return -1;			
		}

		rep->force_prevpageinfo = rep->force_curpageinfo;
		rep->force_curpageinfo = pi;
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
	PageInfo *pi = NULL;
	
	rep->force_curpageinfo = rep->force_nextpageinfo;
	do{
		if(rep->force_rZone->NextPageInfo == 0) {
			rep->force_end_findnextpageinfo = 1;
			return 0;
		}

		pi = alloc_pageinfo(rep);
		if(pi == NULL) {
			ndprint(1,"recycle alloc pageinfo fail func %s line %d \n",
						__FUNCTION__,__LINE__);
			return -1;
		}
		
		ret = Zone_FindNextPageInfo(rep->force_rZone,pi);
		if (read_no_context(ret)) {
			rep->force_end_findnextpageinfo = 1;
			return 0;
		}
		else if((ret & 0xffff) < 0) {
			ndprint(1,"find next pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
			free_pageinfo(rep,pi);
			return -1;			
		}

		rep->force_prevpageinfo = rep->force_curpageinfo;
		rep->force_curpageinfo = pi;
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

	if (l1index == 0xffff)
		return -1;
	
	if(pi->L3InfoLen == 0)
		start_sectorID = l1index * l1unitlen;
	else {
		if (l3index == 0xffff)
			return -1;
		
		if(pi->L2InfoLen == 0)
			start_sectorID = l1index * l1unitlen + l3index * l3unitlen;
		else {
			if (l2index == 0xffff)
				return -1;
			start_sectorID = l3index * l3unitlen + 
				l2index * l2unitlen + l1index * l1unitlen;
		}
	}

	rep->force_startsectorID = start_sectorID;
	rep->force_writepageinfo = get_pageinfo_from_cache(rep,rep->force_startsectorID);

	return 0;
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
	unsigned int l4count = 0;
	unsigned int i = 0;
	unsigned int j = 0;
	unsigned int k = 0;
	unsigned int *l4info = NULL;
	unsigned int *latest_l4info = NULL;
	unsigned int spp = 0;
	int first_flag = 1;
	BuffListManager *blm = ((Context *)(rep->context))->blm;

	l4count = rep->force_curpageinfo->L4InfoLen >> 2;
	spp = rep->force_rZone->vnand->BytePerPage / SECTOR_SIZE;
	rep->force_write_cursor = 0;
	l4info = (unsigned int *)(rep->force_curpageinfo->L4Info);
	latest_l4info = (unsigned int *)(rep->force_writepageinfo->L4Info);

	pl = (PageList *)BuffListManager_getTopNode((int)blm, sizeof(PageList));

	for(i = 0; i < l4count; i += k) {
		k = 1;
		if ((int)l4info[i] == -1)
			continue;

		tmp0 = l4info[i] / spp;
		tmp1 = latest_l4info[i] / spp;

		if (tmp0 == tmp1 && data_in_rzone(rep, tmp0)) {
			if (first_flag) {
				rep->force_write_cursor = i;
				first_flag = 0;
			}
			
			for(j = i + 1; j < spp + i && j < l4count; j++) {
				if ((int)l4info[j] == -1)
					break;
				
				tmp2 = l4info[j] / spp;
				if(tmp2 == tmp0)
					k++;
				else
					break;
					
			}
		
			tpl = (PageList *)BuffListManager_getNextNode((int)blm, (void *)pl, sizeof(PageList));
			tpl->startPageID = tmp0;
			tpl->OffsetBytes = l4info[i] % spp;
			tpl->Bytes = k * SECTOR_SIZE;
			tpl->retVal = 0;
			tpl->pData = NULL;
		}
	}

	BuffListManager_freeList((int)blm, (void **)&pl, (void *)pl, sizeof(PageList));
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
	unsigned int write_pagecount = 0;

	wzone = get_current_write_zone(rep->context);
	zonepage = Zone_GetFreePageCount(wzone);
	if (zonepage == 0) {
		wzone = alloc_new_zone_write(rep->context, wzone);
		zonepage = Zone_GetFreePageCount(wzone);
	}

	write_pagecount = get_pagecount(rep->force_pagelist);
	if (write_pagecount == 0)
		goto exit;
	else if (write_pagecount < recyclepage)
		recyclepage = write_pagecount;

	if(zonepage >= recyclepage + 1) {
		alloc_update_l1l2l3l4(rep,wzone,rep->force_writepageinfo,recyclepage);
		ret = copy_data(rep, wzone, recyclepage);
		if(ret != 0) {
			ndprint(1,"all recycle buflen error func %s line %d \n",
						__FUNCTION__,__LINE__);
			return -1;
		}
	}
	else {
		if(zonepage > 1) {
			alloc_update_l1l2l3l4(rep,wzone,rep->force_writepageinfo,zonepage -1);
			ret = copy_data(rep, wzone, zonepage -1);
			if(ret != 0) {
				ndprint(1,"all recycle buflen error func %s line %d \n",
							__FUNCTION__,__LINE__);
				return -1;
			}

			recyclepage = recyclepage - (zonepage - 1);
		}

		wzone = alloc_new_zone_write(rep->context, wzone);
		if(wzone == NULL) {
			ndprint(1,"zonemanager alloc zone func %s line %d \n",
						__FUNCTION__,__LINE__);
			return -1;
		}

		alloc_update_l1l2l3l4(rep,wzone,rep->force_writepageinfo,recyclepage);
		
		ret = copy_data(rep, wzone, recyclepage);
		if(ret != 0) {
			ndprint(1,"all recycle buflen error func %s line %d \n",
						__FUNCTION__,__LINE__);
			return -1;
		}
	}

exit:
	write_data_prepare(rep->context);
	rep->force_prevpageinfo = rep->force_curpageinfo;
	give_pageinfo_to_cache(rep,rep->force_writepageinfo);

	return 0;
}

/** 
 *	OnForce_FreeZone - Frdd force recycle zone
 *
 *	@rep: operate object
 */
static int OnForce_FreeZone ( Recycle *rep)
{
	int ret = 0;
	BlockList bx;
	BlockList *bl = &bx;
	PageList px;
	PageList *pl = &px;
	NandSigZoneInfo *nandsigzoneinfo = (NandSigZoneInfo *)(rep->force_rZone->mem0);
	int start_blockno = rep->force_rZone->startblockID;
	int next_start_blockno = ((Context *)(rep->context))->zonep->startblockID[rep->force_rZone->ZoneID + 1];
	int blockcount = next_start_blockno - start_blockno;
	int blockno = start_blockno;
	unsigned int i = 0;
	unsigned int short badblock = 0;

	bl->startBlock = start_blockno;
	bl->BlockCount = blockcount;
	bl->retVal = 0;
	(bl->head).next = NULL;

	pl->pData = (void *)nandsigzoneinfo;
	pl->Bytes = rep->force_rZone->vnand->BytePerPage;
	pl->retVal = 0;
	(pl->head).next = NULL;
	pl->OffsetBytes = 0;

	ret = vNand_MultiBlockErase(rep->force_rZone->vnand,bl);
	if(ret < 0) {
		ndprint(1,"Multi block erase error func %s line %d \n"
					,__FUNCTION__,__LINE__);
		return -1;
	}

	while(i < blockcount) {
		if (vNand_IsBadBlock(rep->force_rZone->vnand,blockno)
			&& !is_badblock_in_boot(rep->force_rZone->vnand,blockno))
			badblock |= (1<< i);

		blockno++;
		i++;
	}

	while(i < 16) {
		if(!(badblock & (1 <<i)))
			break;
		i++;	
	}

	pl->startPageID = (start_blockno + i)* rep->force_rZone->vnand->PagePerBlock ;

	memset(nandsigzoneinfo,0xff,rep->force_rZone->vnand->BytePerPage);
	nandsigzoneinfo->ZoneID = rep->force_rZone->sigzoneinfo - rep->force_rZone->top;
	rep->force_rZone->sigzoneinfo->lifetime++;
	nandsigzoneinfo->lifetime = rep->force_rZone->sigzoneinfo->lifetime;
	nandsigzoneinfo->badblock = rep->force_rZone->sigzoneinfo->badblock;

	ret = Zone_RawMultiWritePage(rep->force_rZone,pl);
	if(ret != 0) {
		ndprint(1,"Zone Raw multi write page error func %s line %d \n",__FUNCTION__,__LINE__);
		goto err;
	}

	ZoneManager_FreeRecyclezone(rep->context,rep->force_rZone);
	free_pageinfo(rep,rep->force_curpageinfo);

	return 0;
err:
	free_pageinfo(rep,rep->force_curpageinfo);
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
	Nand_ContinueFree(rep->force_record_writeadd);	
}




/* ************************ boot rcycle **************************** */

/** 
 *	OnForce_GetBootRecycleZone - Get boot recycle zone
 *
 *	@rep: operate object
 */
static int OnForce_GetBootRecycleZone ( Recycle *rep)
{
	ZoneManager *zonep = ((Context *)(rep->context))->zonep;
	unsigned short ZoneID = zonep->last_rzone_id;

	rep->force_rZone = ZoneManager_AllocRecyclezone(rep->context,ZoneID);
	if(rep->force_rZone == NULL) {
		ndprint(1,"alloc force recycle zone error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}
		
	return 0;
}

/** 
 *	OnBoot_FindFirstValidPageInfo - Find first valid pageinfo of boot recycle zone
 *
 *	@rep: operate object
 */
static int OnBoot_FindFirstValidPageInfo ( Recycle *rep)
{
	int ret = 0;
	PageInfo *prev_pi = NULL;
	PageInfo *pi = NULL;
	Context *conptr = (Context *)(rep->context);
	ZoneManager *zonep = conptr->zonep;
	int pageperzone = conptr->vnand.PagePerBlock * BLOCKPERZONE(conptr->vnand);

	prev_pi = alloc_pageinfo(rep);
	if(prev_pi == NULL) {
		ndprint(1,"force recycle alloc pageinfo error func %s line %d \n",
				__FUNCTION__,__LINE__);
		return -1;
	}
	
	ret = Zone_FindFirstPageInfo(rep->force_rZone,prev_pi);
	if((ret & 0xffff) < 0) {
		ndprint(1,"Find First Pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
		free_pageinfo(rep,prev_pi);
		return -1;			
	}
	
	rep->force_curpageinfo = prev_pi;
	while (pageperzone--) {
		do{
			if(rep->force_rZone->NextPageInfo == 0) {
				rep->force_end_findnextpageinfo = 1;
				return 0;
			}

			pi = alloc_pageinfo(rep);
			if(pi == NULL) {
				ndprint(1,"force recycle alloc pageinfo error func %s line %d \n",
						__FUNCTION__,__LINE__);
				return -1;
			}
			
			ret = Zone_FindNextPageInfo(rep->force_rZone,pi);
			if (read_no_context(ret)) {
				rep->force_end_findnextpageinfo = 1;
				return 0;
			}
			else if((ret & 0xffff) < 0) {
				ndprint(1,"find next pageinfo error func %s line %d \n",
						__FUNCTION__,__LINE__);
				free_pageinfo(rep,pi);
				return -1;			
			}

			rep->force_prevpageinfo = rep->force_curpageinfo;
			rep->force_curpageinfo = pi;
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
 *	Recycle_OnBootRecycle - boot recycle
 *
 *	@context: global variable
 */
int Recycle_OnBootRecycle ( int context )
{
	int i;
	int ret = 0;
	Recycle *rep = ((Context *)context)->rep;

	ndprint(0, "start boot recycle--------->\n");
	
	ret = OnForce_Init(rep);
	if (ret == -1)
		goto ERROR;

	ret = OnForce_GetBootRecycleZone(rep);
	if (ret == -1)
		goto ERROR;

	for (i = 0; ; i++) {
		if (rep->force_end_findnextpageinfo)
			break;

		if (i == 0)
			ret = OnBoot_FindFirstValidPageInfo(rep);
		else
			ret = OnForce_FindNextValidPageInfo(rep);
		if (ret == -1)
			goto ERROR;
		
		ret = OnForce_FindValidSector(rep);
		if (ret == -1)
			goto ERROR;

		ret = OnForce_MergerSectorID (rep);
		if (ret == -1)
			goto ERROR;

		ret = OnForce_RecycleReadWrite(rep);
		if (ret == -1)
			goto ERROR;
	}
	
	ret = OnForce_FreeZone(rep);
	if (ret == -1)
		goto ERROR;

	OnForce_Deinit(rep);
	
	ndprint(0, "boot recycle finished--------->\n\n");
	
ERROR:
	return ret;
}

