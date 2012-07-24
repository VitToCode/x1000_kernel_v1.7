#include "l2pconvert.h"
#include "singlelist.h"
#include "cachemanager.h"
#include "delay.h"
#include "zone.h"
#include "zonemanager.h"
#include "sigzoneinfo.h"
#include "vNand.h"
#include "taskmanager.h"
#include "zonemanager.h"
#include "recycle.h"
#include "NandAlloc.h"
#include "nandmanagerinterface.h"
#include "nanddebug.h"

static BuffListManager *Blm;

/**
 *	Idle_Handler  -  function in idle thread
 *
 *	@data: argument which is transfered from caller
*/
static int Idle_Handler(int data)
{
#ifndef TEST_FORCERECYCLE
	Context *conptr = (Context *)data;
	int context = (int)data;
	ZoneManager *zonep = conptr->zonep;
	unsigned int free_count;
	unsigned int used_count;
	unsigned int maxlifetime;
	unsigned int minlifetime;

#ifdef 	TEST_ZONE_MANAGER
#include <unistd.h>
	sleep(2);
#endif

	while(1){
		free_count = zonep->freeZone->count;
		used_count = zonep->useZone->usezone_count;
		maxlifetime = ZoneManager_Getmaxlifetime(context);
		minlifetime = ZoneManager_Getminlifetime(context);
		if (used_count > 2 * free_count || maxlifetime - minlifetime > MAXDIFFTIME) {
			ndprint(1, "start recycle----------------------> \n");
			Recycle_OnNormalRecycle(context);
			ndprint(1, "recycle finished!\n\n");
		}

		sleep(1);
	}
#endif
	return 0;
}

/**
 *	L2PConvert_ZMOpen  -  open operation
 *
 *	@vnand: virtual nand
 *	@pt: physical partition
*/
int L2PConvert_ZMOpen(VNandInfo *vnand, PPartition *pt)
{
	int ret = 0;
	Context *conptr;

	if (pt->mode != ZONE_MANAGER) {
		ndprint(1,"ERROR: func %s line %d \n", __FUNCTION__, __LINE__);
		return -1;
	}

	conptr = (Context *)Nand_VirtualAlloc(sizeof(Context));
	if (!conptr) {
		ndprint(1,"ERROR: func %s line %d \n", __FUNCTION__, __LINE__);
		return -1;
	}
	
	conptr->blm = NULL;
	conptr->cacheinfo = NULL;
	conptr->cachemanager = NULL;
	conptr->l1info = NULL;
	conptr->rep = NULL;
	conptr->top = NULL;
	conptr->zonep = NULL;

	CONV_PT_VN(pt,&conptr->vnand);
	conptr->vnand.pt_badblock_info = vnand->pt_badblock_info;
	conptr->blm = Blm;

	ret = Recycle_Init((int)conptr);
	if (ret != 0) {
		ndprint(1,"Recycle_Init failed func %s line %d \n",
			__FUNCTION__, __LINE__);
		return -1;
	}

	conptr->thandle = Task_Init((int)conptr);
	if (conptr->thandle == -1) {
		ndprint(1,"Task_Init failed func %s line %d \n",
			__FUNCTION__, __LINE__);
		return -1;
	}
#ifndef NO_ERROR
	Task_RegistMessageHandle(conptr->thandle, Idle_Handler, IDLE_MSG_ID);
	Task_RegistMessageHandle(conptr->thandle, Recycle_OnForceRecycle, FORCE_RECYCLE_ID);
	Task_RegistMessageHandle(conptr->thandle, Recycle_OnBootRecycle, BOOT_RECYCLE_ID);
#endif
	ret = ZoneManager_Init((int)conptr);
	if (ret != 0) {
		ndprint(1,"ZoneManager_Init failed func %s line %d \n",
			__FUNCTION__, __LINE__);
		return -1;
	}

	if (!(conptr->cachemanager)) {
		ret = CacheManager_Init((int)conptr);
		if (ret != 0) {
			ndprint(1,"CacheManager_Init failed func %s line %d \n",
				__FUNCTION__, __LINE__);
			return -1;
		}
	}

	return (int)conptr;
}

/**
 *	L2PConvert_ZMClose  -  Close operation
 *
 *	@handle: return value of L2PConvert_ZMOpen
*/	
int L2PConvert_ZMClose(int handle)
{
	int context = handle;
	Context *conptr = (Context *)context;
	BuffListManager *blm = conptr->blm;

	Task_Deinit(conptr->thandle);
	Recycle_DeInit(context);
	CacheManager_DeInit(context);
	ZoneManager_DeInit(context);
	BuffListManager_BuffList_DeInit((int)blm);

	return 0;
}

/**
 *	get_writepagecount  -  get count of page in write operation
 *
 *	@sectorperpage: count of sector per page
 *	@sectornode: object need to calculate
 *	
 *	Calculate how mang page need to read or write
 *	with only one node of SectorList
*/
static int get_writepagecount(int sectorperpage, SectorList *sectornode)
{
	return (sectornode->sectorCount - 1) / sectorperpage + 1;
}

/**
 *	Read_sectornode_to_pagelist  -  Convert one node of SectorList to a PageList in read operation
 *
 *	@context: global variable
 *	@sectorperpage: count of sector per page
 *	@sectornode: object need to calculate
 *	@pagelist: which created when Convert finished
*/
static int Read_sectornode_to_pagelist(int context, int sectorperpage, SectorList *sectornode, PageList *pagelist)
{
	int i, j, k;
	unsigned int sectorid;
	unsigned int pageid_prev, pageid_next;
	unsigned int pageid_by_sector_prev, pageid_by_sector_next;
	PageList *pagenode;
	Context *conptr = (Context *)context;
	unsigned l4count = conptr->cachemanager->L4InfoLen >> 2;
	int sectorcount = sectornode->sectorCount;
	void *pData = sectornode->pData;
	unsigned int left_sector_count = 0;

	sectorid = sectornode->startSector;
	for (i = sectorid; i < sectorid + sectorcount; i += k) {
		k = 1;
		pageid_by_sector_prev = CacheManager_getPageID(context, i);
		if (pageid_by_sector_prev == -1)
			return -1;
		
		pageid_prev = pageid_by_sector_prev / sectorperpage;

		if (l4count - i % l4count < 4 && i % l4count != 0)
			left_sector_count = l4count - i % l4count;
		else {
			if (pageid_by_sector_prev % sectorperpage == 0)
				left_sector_count = sectorperpage;
			else
				left_sector_count = pageid_by_sector_prev % sectorperpage;
		}

		for(j = i + 1; j < (left_sector_count + i) && j < sectorid + sectorcount; j++) {
			pageid_by_sector_next = CacheManager_getPageID(context, j);
			if (pageid_by_sector_next == -1)
				return -1;
			
			pageid_next = pageid_by_sector_next / sectorperpage;
			if(pageid_next == pageid_prev)
				k++;
			else
				break;
		}
		
		pagenode = (PageList *)BuffListManager_getNextNode((int)(conptr->blm), (void *)pagelist, sizeof(PageList));
		pagenode->startPageID = pageid_prev;
		pagenode->OffsetBytes = pageid_by_sector_prev % sectorperpage * SECTOR_SIZE;
		pagenode->Bytes = k * SECTOR_SIZE;
		pagenode->pData = pData;
		pData += pagenode->Bytes;
	}

	return 0;
}

/**
 *	Write_sectornode_to_pagelist  -  Convert one node of SectorList to a PageList in write operation
 *
 *	@context: global variable
 *	@sectorperpage: count of sector per page
 *	@sectornode: object need to calculate
 *	@pagelist: which created when Convert finished
*/
static int Write_sectornode_to_pagelist(int context, int sectorperpage, SectorList *sectornode, PageList *pagelist)
{
	int i;
	PageList *pagenode;
	int pagecount;
	Context *conptr = (Context *)context;
	
	pagecount = get_writepagecount(sectorperpage, sectornode);

	for (i = 0; i < pagecount; i++) {
		pagenode = (PageList *)BuffListManager_getNextNode((int)(conptr->blm), (void *)pagelist, sizeof(PageList));

		/* fill Bytes */
		if (pagecount == 1)
			pagenode->Bytes = sectornode->sectorCount * SECTOR_SIZE;
		else {
			if (i == pagecount - 1 && sectornode->sectorCount % sectorperpage != 0)
				pagenode->Bytes = sectornode->sectorCount % sectorperpage * SECTOR_SIZE;
			else
				pagenode->Bytes = conptr->vnand.BytePerPage;
		}

		/* fill OffsetBytes */
		pagenode->OffsetBytes = 0;

		/* fill pData */
		pagenode->pData  = sectornode->pData + conptr->vnand.BytePerPage * i;
	}

	return 0;
}

/**
 *	analyze_sectorlist - analyze sectorlist
 *
 *	@context: global variable
 *	@sl: which need to analyze
 *
 *	if one node of sl is overflow l4cache, then need to divide it to some node 
 */
static SectorList * new_sectorlist(int context, SectorList *sl)
{
	SectorList *sl_node;
	SectorList *sl_new;
	struct singlelist *pos;
	Context *conptr = (Context *)context;
	CacheManager *cachemanager = conptr->cachemanager;
	unsigned l4count = conptr->cachemanager->L4InfoLen >> 2;
   	SectorList *sl_new_top;
	unsigned char *pdata;
	int sectorcount;
	int startsector;
	if(sl->sectorCount <= 0){
		ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		return 0;
	}
	sl_new = (SectorList *)BuffListManager_getTopNode((int)(conptr->blm), sizeof(SectorList));
	sl_new_top = sl_new;
	
	singlelist_for_each(pos, &sl->head) {
		sl_node = singlelist_entry(pos, SectorList, head);

		if (sl_node->startSector + sl_node->sectorCount > cachemanager->L1UnitLen * cachemanager->L1InfoLen >> 2
			|| sl_node->sectorCount <= 0 ||sl_node->startSector < 0) {
			ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
			goto newsectorlist_error1;
		}
		
		sectorcount = sl_node->sectorCount;
		startsector = sl_node->startSector;
	    pdata = sl_node->pData;
		while(sectorcount>0){
			if(!sl_new)
				sl_new = (SectorList *)BuffListManager_getNextNode((int)(conptr->blm), (void *)sl_new, sizeof(SectorList));
			sl_new->startSector = startsector;
			sl_new->pData = (void *)pdata;
			if(sectorcount + sl_node->startSector % l4count > l4count){
				sl_new->sectorCount = l4count - sl_node->startSector % l4count;
			}else{
				sl_new->sectorCount = sectorcount - sl_node->startSector % l4count;
			}
			startsector += sl_new->sectorCount;
			sectorcount -= sl_new->sectorCount;
			pdata += sl_new->sectorCount * SECTOR_SIZE;
		}
		sl_new = NULL;
	}
	return sl_new_top;
newsectorlist_error1:
	BuffListManager_freeAllList((int)(conptr->blm), (void **)&sl_new_top, sizeof(SectorList));
	return NULL;	
}
/**
 *	L2PConvert_ReadSector  -  Read operation
 *
 *	@handle: return value of L2PConvert_ZMOpen
 *	@sl: which need to read
 *
 *	Transform SectorList to PageList, 
 *	use L2P cache, get a pageid from it by sectorid.
*/
int L2PConvert_ReadSector ( int handle, SectorList *sl )
{
	struct singlelist *pos;
	int ret, sectorperpage;
	SectorList *sl_node;
	PageList *pl;
	int context = handle;
	Context *conptr = (Context *)context;
	CacheManager *cachemanager = conptr->cachemanager;

	if (!sl) {
		ndprint(1,"ERROR: fun %s line %d\n", __FUNCTION__, __LINE__);
		return -1;
	}	

	/* head node will not be use */
	pl =(PageList *) BuffListManager_getTopNode((int)(conptr->blm), sizeof(PageList));
	if (!(pl)) {
		ndprint(1,"ERROR: fun %s line %d\n", __FUNCTION__, __LINE__);
		return -1;
	}

	sectorperpage = conptr->vnand.BytePerPage / SECTOR_SIZE;
	
	singlelist_for_each(pos, &(sl->head)) {
		sl_node = singlelist_entry(pos, SectorList, head);
		if (sl_node->startSector + sl_node->sectorCount > cachemanager->L1UnitLen * cachemanager->L1InfoLen >> 2
			|| sl_node->sectorCount <= 0 ||sl_node->startSector < 0) {
			ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
			return -1;
		}

		ret = Read_sectornode_to_pagelist(context, sectorperpage, sl_node, pl);
		if (ret == -1) {
			ndprint(1,"ERROR: fun %s line %d\n", __FUNCTION__, __LINE__);
			return -1;
		}
	}

	/* delete head node of Pagelist */
	BuffListManager_freeList((int)(conptr->blm), (void **)&pl, (void *)pl, sizeof(PageList));

	ret = vNand_MultiPageRead(&conptr->vnand, pl);

	BuffListManager_freeAllList((int)(conptr->blm), (void **)&pl, sizeof(PageList));

	return ret; 	
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
#ifndef NO_ERROR
	Message force_recycle_msg;
	int msghandle;
#endif
	count = ZoneManager_GetAheadCount(context);
	for (i = 0; i < 4 - count; i++){
		zone = ZoneManager_AllocZone(context);
		if (!zone) {
			ndprint(1,"WARNING: There is not enough zone and start force recycle \n");
#ifndef NO_ERROR
			/* force recycle */
			force_recycle_msg.msgid = FORCE_RECYCLE_ID;
			force_recycle_msg.prio = FORCE_RECYCLE_PRIO;
			force_recycle_msg.data = context;
			
			msghandle = Message_Post(conptr->thandle, &force_recycle_msg, WAIT);
			Message_Recieve(conptr->thandle, msghandle);
#endif
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
 *	get_write_zone - get current write zone
 *
 *	@context: global variable
 */
static Zone *get_write_zone(int context)
{
	Zone *zone = NULL;
	SigZoneInfo *prev = NULL;
	SigZoneInfo *next = NULL;

	zone = ZoneManager_GetCurrentWriteZone(context);
	if(zone == NULL) {
		write_data_prepare(context);
		ZoneManager_GetAheadZone(context, &zone);
		ZoneManager_SetCurrentWriteZone(context,zone);
		prev = ZoneManager_GetPrevZone(context);
		next = ZoneManager_GetNextZone(context);
		Zone_Init(zone,prev,next);
	}

	return zone;
}

/**
 *	set_last_pageinfo - copy the second last pageinfo write to the last page of zone
 *
 *	@zone: operate object
 *	@sectorid: number of sector
 */
static int set_last_pageinfo(Zone *zone, unsigned int sectorid)
{
	int ret = 0;
	PageInfo *pi;

	CacheManager_lockCache(zone->context, sectorid, &pi);

	pi->PageID = Zone_AllocNextPage(zone);
	pi->zoneID = 0xffff;
	ret = Zone_MultiWritePage(zone,0,NULL,pi);
	if(ret != 0) {
		ndprint(1,"vNand MultiPage Write error func %s line %d \n",
			__FUNCTION__,__LINE__);
		return -1;
	}
	
	CacheManager_unlockCache(zone->context, pi);

	return ret;
}

/** 
 *	update_l1l2l3l4 - update Pageinfo l1l2l3l4 and return PageList 
 *
 *	@sl: one node of sectorlist
 *	@pi: the latest pageinfo in zone
 *	@zone: where the pageinfo get from
 */
PageList *update_l1l2l3l4 (SectorList *sl, PageInfo *pi, Zone *zone)
{
	unsigned int l1index = 0;
	unsigned int l2index = 0;
	unsigned int l3index = 0;
	unsigned int l4index = 0;
	unsigned int l1unitlen = 0;
	unsigned int l2unitlen = 0;
	unsigned int l3unitlen = 0;
	unsigned int l4unitlen = 0;
	unsigned int *l1buf = NULL;
	unsigned int *l2buf = NULL;
	unsigned int *l3buf = NULL;
	unsigned int *l4buf = NULL;
	unsigned int i = 0;
	struct singlelist *pos;
	PageList *pl = NULL;
	PageList *pl_node = NULL;
	unsigned int spp = zone->vnand->BytePerPage / SECTOR_SIZE;
	unsigned int sectorid = sl->startSector;
	CacheManager *cachemanager = ((Context *)(zone->context))->cachemanager;
	BuffListManager *blm = ((Context *)(zone->context))->blm;

	l1buf = (unsigned int *)(pi->L1Info);
	l2buf = (unsigned int *)(pi->L2Info);
	l3buf = (unsigned int *)(pi->L3Info);
	l4buf = (unsigned int *)(pi->L4Info);

	l1unitlen = cachemanager->L1UnitLen;
	l2unitlen = cachemanager->L2UnitLen;
	l3unitlen = cachemanager->L3UnitLen;
	l4unitlen = cachemanager->L4UnitLen;

	pi->L1Index = 0xffff;
	pi->L2Index = 0xffff;
	pi->L3Index = 0xffff;
	pi->zoneID = 0xffff;
	pi->PageID = Zone_AllocNextPage(zone);

	/* update l1 */
	l1index = sectorid / l1unitlen;
	l1buf[l1index] = pi->PageID;
	pi->L1Index = l1index;

	/* update l2 and l3 */
	if(pi->L3InfoLen != 0) {
		if(pi->L2InfoLen != 0) {
			l2index = sectorid % l1unitlen / l2unitlen;
			l2buf[l2index] = pi->PageID;
			pi->L2Index = l2index;
			l3index = sectorid % l2unitlen / l3unitlen;
			l3buf[l3index] = pi->PageID;
			pi->L3Index = l3index;
		}
		else {
			l3index = sectorid % l1unitlen / l3unitlen;
			l3buf[l3index] = pi->PageID;
			pi->L3Index = l3index;
		}
	}

	pl = (PageList *)BuffListManager_getTopNode((int)blm, sizeof(PageList));
	Write_sectornode_to_pagelist(zone->context, spp, sl, pl);

	singlelist_for_each(pos, pl->head.next) {
		pl_node = singlelist_entry(pos, PageList, head);
		pl_node->startPageID = Zone_AllocNextPage(zone);

		/* update l4 */
		if(pi->L3InfoLen != 0)
			l4index = sectorid % l3unitlen / l4unitlen;
		else if(pi->L2InfoLen != 0)
			l4index = sectorid  % l2unitlen / l4unitlen;
		else
			l4index = sectorid % l1unitlen / l4unitlen;

		for (i = l4index; i < pl_node->Bytes / SECTOR_SIZE + l4index; i++)
			l4buf[i] = pl_node->startPageID * spp + (i - l4index);

		sectorid += pl_node->Bytes / SECTOR_SIZE;
	}

	BuffListManager_freeList((int)blm, (void **)&pl, (void *)pl, sizeof(PageList));

	return pl;
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
 *	divide_sectornode - divide one node into first node and next node
 *
 *	@sl_node: need to divide
 *	@first: the first node of sl_node when divide finish
 *	@next: the second node of sl_node when divide finish
 *	@count: free page count of current zone
 *	@spp: sectors per page
 */
static void divide_sectornode(SectorList *sl_node, SectorList *first, SectorList *next, unsigned int count, unsigned int spp)
{
	first->startSector = sl_node->startSector;
	first->sectorCount = (count - 1) * spp;
	first->pData = sl_node->pData;

	next->startSector = sl_node->startSector + first->sectorCount;
	next->sectorCount = sl_node->sectorCount - first->sectorCount;
	next->pData = sl_node->pData + first->sectorCount * SECTOR_SIZE;
}

/**
 *	write_sectornode - write one node of sectorlist to nand
 *
 *	@zone: which the data will be written to
 *	@sectornode: lockcache and pi accordint to it
 *	@pi: which pageinfo to write
 *	@pl: which data to write
 *	@pagecount: page count of write data
 */
static int write_sectornode(Zone *zone, SectorList *sectornode, PageInfo *pi, PageList *pl, unsigned int pagecount)
{
	int ret = 0;
	int context = zone->context;

	CacheManager_lockCache(context, sectornode->startSector, &pi);

	pl = update_l1l2l3l4(sectornode, pi, zone);

	zone->currentLsector = sectornode->startSector; 
	ret = Zone_MultiWritePage(zone, pagecount, pl, pi);
	if(ret != 0) {
		ndprint(1,"vNand MultiPage Write error func %s line %d \n",
			__FUNCTION__,__LINE__);
		return -1;
	}

	CacheManager_unlockCache(context, pi);

	return ret;
}

/**
 *	L2PConvert_WriteSector  -  Write operation
 *
 *	@handle: return value of L2PConvert_ZMOpen
 *	@sl: which need to write
 *
 *	Get PageList from Zone, then write data,
 *	at last, update the cache.
*/
int L2PConvert_WriteSector ( int handle, SectorList *sl )
{
	unsigned int freepage_count = 0;
	Zone *zone = NULL;
	PageInfo *pi = NULL;
	PageList *pl = NULL;
	SectorList *sl_node = NULL;
	SectorList *first_sectornode = NULL;
	SectorList *next_sectornode = NULL;
	int context = handle;
	Context *conptr = (Context *)context;
	unsigned int sectorperpage = conptr->vnand.BytePerPage / SECTOR_SIZE;
	int ret = 0;
	struct singlelist *pos = NULL;
	unsigned int pagecount = 0;
	SectorList *sl_new;


	zone = get_write_zone(context);
	if(zone == NULL) {
		ndprint(1,"get_write_zone error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}

	sl_new = new_sectorlist(context, sl);
	if(sl_new == NULL) {
		ndprint(1,"new_sectorlist error func %s line %d \n",
			__FUNCTION__,__LINE__);
		return -1;
	}

	singlelist_for_each(pos,&sl_new->head) {	
		sl_node = singlelist_entry(pos, SectorList, head);
		pagecount = get_writepagecount(sectorperpage, sl_node);
		freepage_count = Zone_GetFreePageCount(zone);

		if(freepage_count < pagecount + 1) {
			if(freepage_count == 1) {
				ret = set_last_pageinfo(zone, sl_node->startSector);
				if(ret == -1) {
					ndprint(1,"set_last_pageinfo error func %s line %d \n",
						__FUNCTION__,__LINE__);
					break;
				}
			}
			else if(freepage_count > 1) {
				first_sectornode = (SectorList *)BuffListManager_getTopNode((int)(conptr->blm), sizeof(SectorList));
				next_sectornode = (SectorList *)BuffListManager_getNextNode((int)(conptr->blm), (void *)first_sectornode, sizeof(SectorList));
				divide_sectornode(sl_node, first_sectornode, next_sectornode,freepage_count, sectorperpage);

				/* write first_sectornode data */
				ret = write_sectornode(zone, first_sectornode, pi, pl, freepage_count -1);
				if(ret == -1) {
					ndprint(1,"write_sectornode error func %s line %d \n",
						__FUNCTION__,__LINE__);
					break;
				}
			}

			/* alloc a new zone to write */
			zone = alloc_new_zone_write (context,zone);
		}

		if (next_sectornode) {
			sl_node = next_sectornode;
			pagecount = (next_sectornode->sectorCount - 1) / sectorperpage + 1;
			next_sectornode = NULL;
		}

		/* write data */
		ret = write_sectornode(zone, sl_node, pi, pl, pagecount);	
		if(ret == -1) {
			ndprint(1,"write_sectornode error func %s line %d \n",
				__FUNCTION__,__LINE__);
			break;
		}

		if (first_sectornode)
			BuffListManager_freeAllList((int)(conptr->blm), (void **)&first_sectornode, sizeof(SectorList));

		/* alloc 4 zone beforehand */
		ret = write_data_prepare(context);
		if (ret == -1) {
			ndprint(1,"write_data_prepare error func %s line %d \n",
				__FUNCTION__,__LINE__);
			break;
		}
	}
	BuffListManager_freeAllList((int)(conptr->blm), (void **)&sl_new, sizeof(SectorList));
	return ret;
}

/**
 *	L2PConvert_Ioctrl  -  IO command
 *
 *	@handle: return value of L2PConvert_ZMOpen
 *	@cmd: command
 *	@argv: argument of command
*/
int L2PConvert_Ioctrl(int handle, int cmd, int argv)
{
	switch (cmd) {
		case SUSPEND:
			return Recycle_Suspend(handle);
		case RESUME:
			return Recycle_Resume(handle);
		default:
			break;
	}
	
	return 0;
}

PartitionInterface l2p_nand_ops = {
	.PartitionInterface_iOpen	= L2PConvert_ZMOpen,
	.PartitionInterface_iClose	= L2PConvert_ZMClose,
	.PartitionInterface_Read	= L2PConvert_ReadSector,
	.PartitionInterface_Write	= L2PConvert_WriteSector,
	.PartitionInterface_Ioctrl	= L2PConvert_Ioctrl,
};

/**
 *	L2PConvert_Init  -  Initialize operation
 *
 *	@pm: pmanager
*/
int L2PConvert_Init(PManager *pm)
{
	Blm = pm->bufferlist;
#ifndef TEST_L2P	
	return NandManger_Register_Manager((int)pm, ZONE_MANAGER, &l2p_nand_ops);
#else
	return 0;
#endif
}

/**
 *	L2PConvert_Deinit  -  Deinit operation
 *
 *	@handle: return value of L2PConvert_ZMOpen
*/
void L2PConvert_Deinit(int handle)
{	
	
}

