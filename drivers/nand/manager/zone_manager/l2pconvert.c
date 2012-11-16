#include "l2pconvert.h"
#include "singlelist.h"
#include "cachemanager.h"
#include "zone.h"
#include "zonemanager.h"
#include "sigzoneinfo.h"
#include "vNand.h"
#include "l2vNand.h"
#include "taskmanager.h"
#include "zonemanager.h"
#include "recycle.h"
#include "NandAlloc.h"
#include "nandmanagerinterface.h"
#include "nanddebug.h"
#include "errhandle.h"
#include "nmbitops.h"
#include "clib.h"
#include "timeinterface.h"
#include "pageinfodebug.h"
//#include "badblockinfo.h"

static BuffListManager *Blm;
/**
 *	Idle_Handler  -  function in idle thread
 *
 *	@data: argument which is transfered from caller
*/
#ifndef NO_ERROR
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
	long long l2pwritetime;

#ifdef 	TEST_ZONE_MANAGER
	nm_sleep(2);
#endif

	while(1){
		Recycle_Lock(context);
		free_count = zonep->freeZone->count;
		used_count = zonep->useZone->usezone_count;
		maxlifetime = ZoneManager_Getmaxlifetime(context);
		minlifetime = ZoneManager_Getminlifetime(context);
		l2pwritetime = conptr->t_startrecycle;
		Recycle_Unlock(context);
		if ( ((nd_getcurrentsec_ns() >= (l2pwritetime + INTERNAL_TIME)) &&
			 conptr->rep->taskStep == RECYIDLE &&
		     (used_count > 2 * free_count || maxlifetime - minlifetime > MAXDIFFTIME)) || Get_JunkZoneRecycleTrig(conptr->junkzone))
			Recycle_OnNormalRecycle(context);

		nm_sleep(1);
	}
#endif
	return 0;
}
#endif

/**
 *	calc_L1L2L3_len - Calculate length of L1, L2 and L3
 *
 *	@zonep: operate object
 */
static void calc_L1L2L3_len(Context *contr)
{
	unsigned int l2l3len = 0;
	unsigned int temp = 0;
	unsigned int value = 1;
	unsigned int i = 0;
	unsigned int sectornum;
	unsigned int totalsectornum;
	unsigned int l1_secnum;
	unsigned int l4_secnum;
	VNandInfo *vnand = &contr->vnand;
	int nandsize;
    	int l4size;

	sectornum = vnand->BytePerPage / SECTOR_SIZE;
	totalsectornum = vnand->PagePerBlock * vnand->TotalBlocks * sectornum;
	l2l3len = (vnand->BytePerPage - (L4INFOLEN + sizeof(NandPageInfo))) / sizeof(unsigned int);

	l1_secnum = vnand->BytePerPage / sizeof(unsigned int);
	l4_secnum = L4INFOLEN / sizeof(unsigned int);
	if(l1_secnum * l4_secnum >= totalsectornum)
		temp = 0;
	else
		temp = (totalsectornum + (l1_secnum * l4_secnum) -1) / (l1_secnum * l4_secnum);

	while(1)
	{
		i = temp / value ;
		if(i <= l2l3len)
			break;
		value = value * 2;
	}

	if(value == 1)
		contr->L2InfoLen = 0;
	else
		contr->L2InfoLen = value * sizeof(unsigned int);

	contr->l1info->len = vnand->BytePerPage;
	contr->L3InfoLen = (temp+value-1) / value * sizeof(unsigned int);
	contr->L4InfoLen = L4INFOLEN;

	nandsize = vnand->TotalBlocks * vnand->PagePerBlock * vnand->BytePerPage;
	l4size = L4INFOLEN / sizeof(unsigned int) * SECTOR_SIZE;
	if (nandsize < vnand->BytePerPage / sizeof(unsigned int) * l4size )
		contr->l1info->len = (nandsize + l4size -1) / l4size * sizeof(unsigned int);
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
	Context *conptr = NULL;
	L2pConvert *l2p = NULL;
	int zonecount;
	if (pt->mode != ZONE_MANAGER) {
		ndprint(L2PCONVERT_ERROR,"ERROR: func %s line %d \n", __FUNCTION__, __LINE__);
		return -1;
	}
	conptr = (Context *)Nand_VirtualAlloc(sizeof(Context));
	if (!conptr) {
		ndprint(L2PCONVERT_ERROR,"ERROR: func %s line %d \n", __FUNCTION__, __LINE__);
		return -1;
	}

	l2p = (L2pConvert *)Nand_VirtualAlloc(sizeof(L2pConvert));
	if (!l2p) {
		ndprint(L2PCONVERT_ERROR,"ERROR: func %s line %d \n", __FUNCTION__, __LINE__);
		return -1;
	}

	l2p->sectorid = (int *)Nand_ContinueAlloc(L4INFOLEN);
	if (!(l2p->sectorid)) {
		ndprint(L2PCONVERT_ERROR,"ERROR: func %s line %d \n", __FUNCTION__, __LINE__);
		return -1;
	}

	conptr->l2pid = (int)l2p;
	conptr->blm = Blm;
	conptr->cacheinfo = NULL;
	conptr->cachemanager = NULL;
	conptr->l1info = NULL;
	conptr->rep = NULL;
	conptr->top = NULL;
	conptr->zonep = NULL;
	conptr->t_startrecycle = 0;
#ifdef STATISTICS_DEBUG
	conptr->timebyte = (TimeByte*)Nd_TimerdebugInit();
	conptr->vnand.timebyte = (TimeByte*)Nd_TimerdebugInit();
#endif

	CONV_PT_VN(pt,&conptr->vnand);
	conptr->l1info = (L1Info *)Nand_VirtualAlloc(sizeof(L1Info));
	if (!conptr->l1info) {
		ndprint(L2PCONVERT_ERROR,"ERROR: func %s line %d \n", __FUNCTION__, __LINE__);
		return -1;
	}
	conptr->l1info->page = (unsigned int *)Nand_ContinueAlloc(conptr->vnand.BytePerPage);
	if(conptr->l1info->page == NULL) {
		ndprint(L2PCONVERT_ERROR,"ERROR: func %s line %d \n", __FUNCTION__, __LINE__);
		return -1;
	}
	memset((void *)(conptr->l1info->page), 0xff, conptr->vnand.BytePerPage);
	InitNandMutex(&conptr->l1info->mutex);
	calc_L1L2L3_len(conptr);
/*
	zonecount = conptr->vnand.TotalBlocks * 10 / 100;
	if(zonecount < 8)
		zonecount = 8;
	if(zonecount > 64)
		zonecount = 64;
*/
        zonecount = (conptr->vnand.TotalBlocks + BLOCKPERZONE(conptr->vnand) - 1)
                        / BLOCKPERZONE(conptr->vnand);
        conptr->junkzone = Init_JunkZone(zonecount);
	ret = Recycle_Init((int)conptr);
	if (ret != 0) {
		ndprint(L2PCONVERT_ERROR,"ERROR:Recycle_Init failed func %s line %d \n",
			__FUNCTION__, __LINE__);
		return -1;
	}
	conptr->thandle = Task_Init((int)conptr);
	if (conptr->thandle == -1) {
		ndprint(L2PCONVERT_ERROR,"ERROR:Task_Init failed func %s line %d \n",
			__FUNCTION__, __LINE__);
		return -1;
	}

#ifndef NO_ERROR
	Task_RegistMessageHandle(conptr->thandle, Recycle_OnForceRecycle, FORCE_RECYCLE_ID);
	Task_RegistMessageHandle(conptr->thandle, Recycle_OnFollowRecycle, FOLLOW_RECYCLE_ID);
	Task_RegistMessageHandle(conptr->thandle, Recycle_OnBootRecycle, BOOT_RECYCLE_ID);
	Task_RegistMessageHandle(conptr->thandle, read_first_pageinfo_err_handler, READ_FIRST_PAGEINFO_ERROR_ID);
	Task_RegistMessageHandle(conptr->thandle, Recycle_OnForceRecycle, WRITE_READ_ECC_ERROR_ID);
	Task_RegistMessageHandle(conptr->thandle, read_page0_err_handler, READ_PAGE0_ERROR_ID);
	Task_RegistMessageHandle(conptr->thandle, read_page1_err_handler, READ_PAGE1_ERROR_ID);
	Task_RegistMessageHandle(conptr->thandle, read_page2_err_handler, READ_PAGE2_ERROR_ID);
	Task_RegistMessageHandle(conptr->thandle, read_ecc_err_handler, READ_ECC_ERROR_ID);
#endif
	ret = ZoneManager_Init((int)conptr);
	if (ret != 0) {
		ndprint(L2PCONVERT_ERROR,"ERROR:ZoneManager_Init failed func %s line %d \n",
			__FUNCTION__, __LINE__);
		return -1;
	}
	ret = CacheManager_Init((int)conptr);
	if (ret != 0) {
		ndprint(L2PCONVERT_ERROR,"ERROR:CacheManager_Init failed func %s line %d \n",
			__FUNCTION__, __LINE__);
		return -1;
	}
	if(CacheManager_CheckCacheAll ((int)conptr->cachemanager,0,0)){
		while(1);
	}
#ifndef NO_ERROR
	Task_RegistMessageHandle(conptr->thandle, Idle_Handler, IDLE_MSG_ID);
#endif
#ifdef L2P_PAGEINFO_DEBUG
	l2p->debug = Init_L2p_Debug((int)conptr);
#endif
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
	L2pConvert *l2p = (L2pConvert *)(conptr->l2pid);
#ifdef L2P_PAGEINFO_DEBUG
	Deinit_L2p_Debug(l2p->debug);
#endif

#ifdef STATISTICS_DEBUG
	Nd_TimerdebugDeinit(conptr->timebyte);
	Nd_TimerdebugDeinit(conptr->vnand.timebyte);
#endif
	Nand_ContinueFree(l2p->sectorid);
	Nand_VirtualFree(l2p);
	Nand_ContinueFree(conptr->l1info->page);
	Nand_VirtualFree(conptr->l1info);
	DeinitNandMutex(&conptr->l1info->mutex);
	Deinit_JunkZone(conptr->junkzone);
	Task_Deinit(conptr->thandle);
	Recycle_DeInit(context);
	CacheManager_DeInit(context);
	ZoneManager_DeInit(context);
	Nand_VirtualFree(conptr);

	return 0;
}

/**
 *	get_pagecount  -  get count of page in write operation
 *
 *	@sectorperpage: count of sector per page
 *	@sectornode: object need to calculate
 *
 *	Calculate how mang page need to read or write
 *	with only one node of SectorList
*/
static int get_pagecount(int sectorperpage, SectorList *sectornode)
{
	return (sectornode->sectorCount - 1) / sectorperpage + 1;
}

static void handle_unCached_sector(SectorList *sectornode, int sector_offset) {
	void *start = (void *)(unsigned char *)sectornode->pData + sector_offset * SECTOR_SIZE;
	memset(start, 0xff, SECTOR_SIZE);
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
	int i, j, k, l;
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
	for (i = sectorid; i < sectorid + sectorcount; i += (k + l)) {
		l = k = 0;
		pageid_by_sector_prev = CacheManager_getPageID((int)conptr->cachemanager, i);
		if (pageid_by_sector_prev == -1) {
			ndprint(L2PCONVERT_INFO,"CacheManager_getPageID when sectorid = %d fun %s line %d\n",
				i, __FUNCTION__, __LINE__);
			handle_unCached_sector(sectornode, i - sectorid);
			l++;
			continue;
		}
		k++;

		pageid_prev = pageid_by_sector_prev / sectorperpage;

		if (l4count - i % l4count < 4 && i % l4count != 0)
			left_sector_count = l4count - i % l4count;
		else {
			if (pageid_by_sector_prev % sectorperpage == 0)
				left_sector_count = sectorperpage;
			else
				left_sector_count = sectorperpage - pageid_by_sector_prev % sectorperpage;
		}

		for(j = i + 1; j < (left_sector_count + i) && j < sectorid + sectorcount; j++) {
			pageid_by_sector_next = CacheManager_getPageID((int)conptr->cachemanager, j);
			if (pageid_by_sector_next == -1) {
				ndprint(L2PCONVERT_INFO,"CacheManager_getPageID when sectorid = %d fun %s line %d\n",
						i, __FUNCTION__, __LINE__);
				handle_unCached_sector(sectornode, j - sectorid);
				l++;
				break;
			}

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
		pagenode->retVal = 1;
		pData += pagenode->Bytes;
	}
	if (pagenode)
		pagenode->retVal = 0;

	return 0;
}

/**
 *	Write_sectornode_to_pagelist  -  Convert one node of SectorList to a PageList in write operation
 *
 *	@zone: current write zone
 *	@sectorperpage: count of sector per page
 *	@sectornode: object need to calculate
 *	@pagelist: which created when Convert finished
 *  @brokenflag: 1--current sectorlist node not be mergeable
 *		            with next sectorlist node on physical address
*/
static int Write_sectornode_to_pagelist(Zone *zone, int sectorperpage, SectorList *sectornode, PageList **pagelist, int left_sectorcount, int brokenflag)
{
	int i;
	PageList *pagenode = NULL;
	SectorList sl_new;
	int pagecount;
	int sectorcount;
	Context *conptr = (Context *)(zone->context);

	if (left_sectorcount != 0) {
		if (sectornode->sectorCount <= left_sectorcount)
			sectorcount = sectornode->sectorCount;
		else
			sectorcount = left_sectorcount;

		pagenode = (PageList *)BuffListManager_getNextNode((int)(conptr->blm), (void *)pagelist, sizeof(PageList));
		pagenode->startPageID = -1;
		pagenode->Bytes = sectorcount * SECTOR_SIZE;
		pagenode->OffsetBytes = (sectorperpage - left_sectorcount) * SECTOR_SIZE;
		pagenode->pData = sectornode->pData;
		pagenode->retVal = 1;

		if (sectornode->sectorCount <= left_sectorcount) {
			if (brokenflag)
				pagenode->retVal = 0;
			return 0;
		}
		sl_new.sectorCount = sectornode->sectorCount - left_sectorcount;
		sl_new.pData = sectornode->pData + pagenode->Bytes;
		sectornode = &sl_new;
	}

	pagecount = get_pagecount(sectorperpage, sectornode);

	for (i = 0; i < pagecount; i++) {
		if (*pagelist == NULL) {
			pagenode = (PageList *)BuffListManager_getTopNode((int)(conptr->blm), sizeof(PageList));
			*pagelist = pagenode;
		}
		else
			pagenode = (PageList *)BuffListManager_getNextNode((int)(conptr->blm), (void *)(*pagelist), sizeof(PageList));

		pagenode->startPageID = -1;

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

		pagenode->retVal = 1;
	}

	if (brokenflag)
		pagenode->retVal = 0;

	return 0;
}

/**
 *	start_ecc_error_handle - start to deal with the error when read ecc error
 *
 *	@zonep: operate object
 *	@zoneid: error zoneid
 */
static int start_ecc_error_handle(int context, unsigned int pageid)
{
	//int i;
	Message read_ecc_error_msg;
	int blockid,ret = 0;
	//int badblock_count = 0;
	int zone_start_blockid;
	int msghandle;
	Context *conptr = (Context *)context;
	//ZoneManager *zonep = conptr->zonep;
	ErrInfo errinfo;
	errinfo.context = context;
	errinfo.err_zoneid = ZoneManager_convertPageToZone(context, pageid);

	blockid = pageid / conptr->vnand.PagePerBlock;
	//zone_start_blockid = BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,errinfo.err_zoneid);
	zone_start_blockid = errinfo.err_zoneid * BLOCKPERZONE(zonep->vnand);

	/*
	for (i = zone_start_blockid; i < blockid; i++) {
		if (vNand_IsBadBlock(&conptr->vnand,i))
			badblock_count++;
	}
	nm_set_bit(blockid - zone_start_blockid - badblock_count,
		(unsigned int *)&(conptr->top + errinfo.err_zoneid)->badblock);
	*/

	read_ecc_error_msg.msgid = READ_ECC_ERROR_ID;
	read_ecc_error_msg.prio = READ_ECC_ERROR_PRIO;
	read_ecc_error_msg.data = (int)&errinfo;

	msghandle = Message_Post(conptr->thandle, &read_ecc_error_msg, WAIT);
	ret = Message_Recieve(conptr->thandle, msghandle);
	nm_set_bit(blockid - zone_start_blockid, (unsigned int *)&(conptr->top + errinfo.err_zoneid)->badblock);

	return ret;
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
	PageList *pl_node;
	int context = handle;
	Context *conptr = (Context *)context;
	CacheManager *cachemanager = conptr->cachemanager;

	if (!sl) {
		ndprint(L2PCONVERT_ERROR,"ERROR: fun %s line %d\n", __FUNCTION__, __LINE__);
		return -1;
	}

	Recycle_Lock(context);
#ifdef STATISTICS_DEBUG
	Get_StartTime(conptr->timebyte,0);
#endif
	/* head node will not be use */
	pl =(PageList *) BuffListManager_getTopNode((int)(conptr->blm), sizeof(PageList));
	if (!(pl)) {
		ndprint(L2PCONVERT_ERROR,"ERROR: fun %s line %d\n", __FUNCTION__, __LINE__);
		conptr->t_startrecycle = nd_getcurrentsec_ns();
		Recycle_Unlock(context);
		return -1;
	}

	sectorperpage = conptr->vnand.BytePerPage / SECTOR_SIZE;

	singlelist_for_each(pos, &(sl->head)) {
		sl_node = singlelist_entry(pos, SectorList, head);
		if (sl_node->startSector + sl_node->sectorCount > cachemanager->L1UnitLen * cachemanager->L1InfoLen >> 2
			|| sl_node->sectorCount <= 0 ||sl_node->startSector < 0) {
			ndprint(L2PCONVERT_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
			conptr->t_startrecycle = nd_getcurrentsec_ns();
			Recycle_Unlock(context);
			return -1;
		}

		ret = Read_sectornode_to_pagelist(context, sectorperpage, sl_node, pl);
		/*
		if (ret == -1)
			goto first_read;
		*/
	}

	/* delete head node of Pagelist */
	BuffListManager_freeList((int)(conptr->blm), (void **)&pl, (void *)pl, sizeof(PageList));
	if (!pl)
		goto null_pl;

	ret = vNand_MultiPageRead(&conptr->vnand, pl);
	singlelist_for_each(pos, &(pl->head)) {
		pl_node = singlelist_entry(pos, PageList, head);
		if (pl_node->retVal != pl_node->Bytes){
			if (ISNOWRITE(pl_node->retVal)){
				ndprint(L2PCONVERT_ERROR,"ERROR:ret shouldn't be -6!!!!\n");
				while(1);
				}
			if (ISERROR(pl_node->retVal) || ISDATAMOVE(pl_node->retVal)) {
				ndprint(L2PCONVERT_INFO,"start ecc_error_handle,ret=%d \n",pl_node->retVal);
				ret = start_ecc_error_handle(context, pl_node->startPageID);
				if (ret != 0) {
					ndprint(L2PCONVERT_ERROR,"ecc_error_handle error func %s line %d ret:%d\n",
							__FUNCTION__, __LINE__,ret);
					goto exit;
				}
				break;
			}
		}

	}

#ifdef STATISTICS_DEBUG
	Calc_Speed(conptr->timebyte, (void*)sl, 0, 1);
#endif

exit:
	BuffListManager_freeAllList((int)(conptr->blm), (void **)&pl, sizeof(PageList));
null_pl:
	conptr->t_startrecycle = nd_getcurrentsec_ns();
	Recycle_Unlock(context);
	return 0;
/*
first_read:
	ret = 0;
	BuffListManager_freeAllList((int)(conptr->blm), (void **)&pl, sizeof(PageList));
	singlelist_for_each(pos, &(sl->head)) {
		sl_node = singlelist_entry(pos, SectorList, head);
		memset(sl_node->pData, 0xff, sl_node->sectorCount * SECTOR_SIZE);
	}
	conptr->t_startrecycle = nd_getcurrentsec_ns();
	Recycle_Unlock(context);

	return ret;
*/
}

/*recycle zone when freezone < sumzone*4% */
static void recycle_zone_prepare(int context)
{
	unsigned int ptzonenum;
	Context *conptr = (Context *)context;
	Message force_recycle_msg;
	int msghandle;
	ForceRecycleInfo frinfo;

	ptzonenum = ZoneManager_Getptzonenum(context) * 4 / 100;
	if(ptzonenum == 0)
		ptzonenum = 1;
	if (ZoneManager_Getfreecount(context) < ptzonenum ){
		frinfo.context = context;
		frinfo.pagecount = -1;
		frinfo.suggest_zoneid = -1;
		force_recycle_msg.msgid = FORCE_RECYCLE_ID;
		force_recycle_msg.prio = FORCE_RECYCLE_PRIO;
		force_recycle_msg.data = (int)&frinfo;

		msghandle = Message_Post(conptr->thandle, &force_recycle_msg, WAIT);
		Message_Recieve(conptr->thandle, msghandle);
	}
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
	unsigned int freecount = 0;
#ifndef NO_ERROR
	int ret = 0;
	Context *conptr = (Context *)context;
	Message force_recycle_msg;
	int msghandle;
	ForceRecycleInfo frinfo;
	VNandInfo *vnand = &conptr->vnand;
#endif

	count = ZoneManager_GetAheadCount(context);
	for (i = 0; i < 4 - count; i++){
		zone = ZoneManager_AllocZone(context);
		freecount = ZoneManager_Getfreecount(context);
		if ((!zone || freecount < 9) && count > 0) {
			ndprint(L2PCONVERT_INFO,"WARNING: There is not enough zone and start force recycle,i:%d count:%d freecount:%d\n",i,count,freecount);
#ifndef NO_ERROR
			/* force recycle */
			frinfo.context = context;
			frinfo.pagecount = vnand->PagePerBlock + 1;
			frinfo.suggest_zoneid = -1;
			force_recycle_msg.msgid = FORCE_RECYCLE_ID;
			force_recycle_msg.prio = FORCE_RECYCLE_PRIO;
			force_recycle_msg.data = (int)&frinfo;

			msghandle = Message_Post(conptr->thandle, &force_recycle_msg, WAIT);
			ret = Message_Recieve(conptr->thandle, msghandle);
			if (ret != 0) {
				ndprint(L2PCONVERT_INFO,"ERROR: Force recycle failed!\n");
				return -1;
			}

			zone = ZoneManager_AllocZone(context);
			if (!zone) {
				ndprint(L2PCONVERT_INFO,"WARNING: There is not enough zone!\n");
				while(1)
					ndprint(L2PCONVERT_INFO,".");
			}
#endif
		}
		ZoneManager_SetAheadZone(context,zone);
	}

	count = ZoneManager_GetAheadCount(context);
	if (count > 0 && count < 4) {
		ndprint(L2PCONVERT_INFO,"WARNING: can't alloc four zone beforehand, free zone count is %d \n", count);
		return 0;
	}
	else if (count == 0){
		ndprint(L2PCONVERT_ERROR,"ERROR: There is no free zone exist!!!!!! \n");
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
	int ret;
	int count = 0;
	zone = ZoneManager_GetCurrentWriteZone(context);
	if(zone == NULL) {
retry:
		write_data_prepare(context);
		ZoneManager_GetAheadZone(context, &zone);
		ZoneManager_SetCurrentWriteZone(context,zone);
		prev = ZoneManager_GetPrevZone(context);
		next = ZoneManager_GetNextZone(context);
		ret = Zone_Init(zone,prev,next);
		if(ret != 0) {
			count++;
			if(count > 4){
				ndprint(L2PCONVERT_ERROR,"too many zone\n");
			}
			ZoneManager_DropZone(context,zone);
			goto retry;
		}
	}

	return zone;
}

static void fill_l2p_sectorid(L2pConvert *l2p, SectorList *sl)
{
	int i = 0;
	int j = 0;

	for (i = 0; i < l2p->l4count; i++) {
		if (j == sl->sectorCount)
			break;
		if (l2p->sectorid[i] == -1) {
			l2p->sectorid[i] = sl->startSector + j;
			j++;
		}
	}
}

static void new_pagelist(L2pConvert *l2p, SectorList *sl, PageList **pagelist, Zone *zone, int brokenflag)
{
	unsigned int spp = zone->vnand->BytePerPage / SECTOR_SIZE;

	if (l2p->l4_is_new) {
		l2p->page_left_sector_count = 0;
		l2p->l4_is_new = 0;
	}

	if (l2p->zone_is_new) {
		l2p->page_left_sector_count = 0;
		l2p->zone_is_new = 0;
	}

	Write_sectornode_to_pagelist(zone, spp, sl, pagelist, l2p->page_left_sector_count, brokenflag);

	if (sl->sectorCount <= l2p->page_left_sector_count)
		l2p->page_left_sector_count -= sl->sectorCount;
	else
		l2p->page_left_sector_count = (spp - (sl->sectorCount - l2p->page_left_sector_count) % spp) % spp;
}

static int update_l1l2l3l4 (L2pConvert *l2p, PageInfo *pi, PageList *pagelist, Zone *zone)
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
	unsigned int j = 0;
	unsigned int old_startpageid = 0;
	struct singlelist *pos;
	PageList *pl_node = NULL;
	int sector_count = 0;
	unsigned int spp = zone->vnand->BytePerPage / SECTOR_SIZE;
	unsigned int sectorid = l2p->sectorid[j];
	CacheManager *cachemanager = ((Context *)(zone->context))->cachemanager;

	int jzone = ((Context *)(zone->context))->junkzone;
	int oldzoneid;
	int sectorcount = 0;
	unsigned int jsectorid;

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

	jsectorid = -1;
	singlelist_for_each(pos, &pagelist->head) {
		pl_node = singlelist_entry(pos, PageList, head);
		if (sector_count == 0) {
			pl_node->startPageID = Zone_AllocNextPage(zone);
			old_startpageid = pl_node->startPageID;
			l2p->pagecount++;
		}
		else
			pl_node->startPageID = old_startpageid;

		/* update l4 */
		if(pi->L3InfoLen != 0)
			l4index = sectorid % l3unitlen / l4unitlen;
		else
			l4index = sectorid % l1unitlen / l4unitlen;

		for (i = l4index; i < pl_node->Bytes / SECTOR_SIZE + l4index; i++){
			if(l4buf[i] != -1){
				if(jsectorid == -1) jsectorid = l4buf[i];
				if(jsectorid != l4buf[i]){
					oldzoneid = ZoneManager_convertPageToZone(zone->context,(jsectorid - 1)/spp);
					if (oldzoneid == -1) {
						ndprint(L2PCONVERT_ERROR,"ERROR:func: %s line: %d pageid = %d \n",
							__func__,__LINE__, (jsectorid - 1)/spp);
						return -1;
					}
					else if(oldzoneid != zone->ZoneID)
						Insert_JunkZone(jzone,sectorcount,oldzoneid);
					jsectorid = l4buf[i];
					jsectorid++;
					sectorcount = 1;
				}else{
					jsectorid++;
					sectorcount++;
				}
			}else{
				if(sectorcount && jsectorid != -1){
					oldzoneid = ZoneManager_convertPageToZone(zone->context,(jsectorid - 1)/spp);
					if (oldzoneid == -1) {
						ndprint(L2PCONVERT_ERROR,"ERROR:func: %s line: %d pageid = %d \n",
							__func__,__LINE__, (jsectorid - 1)/spp);
						return -1;
					}
					else if(oldzoneid != zone->ZoneID)
						Insert_JunkZone(jzone,sectorcount,oldzoneid);
					jsectorid = -1;
					sectorcount = 0;
				}
			}

			l4buf[i] = pl_node->startPageID * spp + sector_count;
			sector_count++;
			if (sector_count == spp)
				sector_count = 0;
		}

		j += i - l4index;
		sectorid = l2p->sectorid[j];
	}

	if(jsectorid != -1 && sectorcount){
		oldzoneid = ZoneManager_convertPageToZone(zone->context,(jsectorid-1)/spp);
		if (oldzoneid == -1) {
			ndprint(L2PCONVERT_ERROR,"ERROR:func: %s line: %d pageid = %d \n",
				__func__,__LINE__, (jsectorid - 1)/spp);
			return -1;
		}
		else if(oldzoneid != zone->ZoneID)
			Insert_JunkZone(jzone,sectorcount,oldzoneid);
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
	int ret;
	int count = 0;
	if (zone) {
		ZoneManager_SetPrevZone(context,zone);
		ZoneManager_FreeZone(context,zone);
	}
retry:
	if (ZoneManager_GetAheadCount(context) == 0)
		write_data_prepare(context);

	ZoneManager_GetAheadZone(context, &new_zone);
	ZoneManager_SetCurrentWriteZone(context,new_zone);
	prev = ZoneManager_GetPrevZone(context);
	next = ZoneManager_GetNextZone(context);
	ret = Zone_Init(new_zone,prev,next);
	if(ret != 0) {
		count++;
		ZoneManager_DropZone(context,new_zone);
		write_data_prepare(context);
		if(count > 4) {
			ndprint(L2PCONVERT_ERROR,"too many bad block\n");
		}
		goto retry;
	}
	return new_zone;
}

static PageList *create_pagelist (L2pConvert *l2p,PageInfo *pi, Zone **czone)
{
	struct singlelist *pos = NULL;
	SectorList *sl_node = NULL;
	SectorList sl_node_new;
	SectorList *sl_node_old = NULL;
	PageList *pl = NULL;
	int total_sectorcount = 0;
	unsigned short free_pagecount = 0;
	Zone *zone = *czone;
	Context *conptr = (Context *)(zone->context);
	CacheManager *cm = conptr->cachemanager;
	unsigned int l4count = l2p->l4count;
	int spp = conptr->vnand.BytePerPage / SECTOR_SIZE;
	int brokenflag = 1;
        free_pagecount = Zone_GetFreePageCount(zone);
	if (free_pagecount < zone->vnand->v2pp->_2kPerPage) {
		zone = alloc_new_zone_write(zone->context,zone);
		free_pagecount = Zone_GetFreePageCount(zone);
		*czone = zone;
		l2p->zone_is_new = 1;
		l2p->alloced_new_zone = 1;
	}
	pi->PageID = Zone_AllocNextPage(zone);
	if (zone->vnand->v2pp->_2kPerPage > 1) {
		while (pi->PageID % zone->vnand->v2pp->_2kPerPage)
			pi->PageID = Zone_AllocNextPage(zone);
	}
#ifdef L2P_PAGEINFO_DEBUG
	L2p_Debug_SetstartPageid(l2p->debug,pi->PageID);
#endif

	l2p->prev_node = l2p->follow_node;
	singlelist_for_each(pos,&l2p->follow_node->head) {
		sl_node = singlelist_entry(pos, SectorList, head);
		brokenflag = 1;
		if (sl_node->startSector + sl_node->sectorCount > cm->L1UnitLen * cm->L1InfoLen >> 2
			|| sl_node->sectorCount <= 0 ||sl_node->startSector < 0) {
			ndprint(L2PCONVERT_ERROR,"ERROR: startsectorid = %d, sectorcount = %d func %s line %d\n",
					sl_node->startSector, sl_node->sectorCount, __FUNCTION__, __LINE__);
			goto err;
		}

		sl_node_old = sl_node;
		if (l2p->L4_startsectorid == -1)
			l2p->L4_startsectorid = sl_node->startSector / l4count * l4count;
		else if (l2p->L4_startsectorid != sl_node->startSector / l4count * l4count && !IS_BREAK(l2p->break_type)) {
			NOT_SAME_L4(l2p->break_type);
			l2p->follow_node = sl_node_old;
			l2p->node_left_sector_count = sl_node->sectorCount;
			break;
		}

		if (IS_BREAK(l2p->break_type)) {
			if (sl_node->sectorCount != l2p->node_left_sector_count) {
				sl_node_new.startSector = sl_node->startSector + sl_node->sectorCount - l2p->node_left_sector_count;
				sl_node_new.sectorCount = l2p->node_left_sector_count;
				sl_node_new.pData = sl_node->pData + (sl_node->sectorCount - l2p->node_left_sector_count) * SECTOR_SIZE;
				sl_node_new.head.next = l2p->follow_node->head.next;
				sl_node = &sl_node_new;
				l2p->L4_startsectorid = sl_node->startSector / l4count * l4count;
			}
			l2p->break_type = 0;
		}

		if (sl_node->startSector % l4count + sl_node->sectorCount > l4count) {
			NOT_SAME_L4(l2p->break_type);
			l2p->follow_node = sl_node_old;
			l2p->node_left_sector_count = sl_node->sectorCount - (l4count - sl_node->startSector % l4count);
			sl_node_new.startSector = sl_node->startSector;
			sl_node_new.sectorCount = l4count - sl_node->startSector % l4count;
			sl_node_new.pData = sl_node->pData;
			sl_node_new.head.next = sl_node->head.next;
			sl_node = &sl_node_new;
			brokenflag = 0;
		}

		total_sectorcount += sl_node->sectorCount;
		if (total_sectorcount > (free_pagecount - 1) * spp ||
			(total_sectorcount == (free_pagecount - 1) * spp && sl_node->head.next != NULL)) {
			NO_ENOUGH_PAGES(l2p->break_type);
			if (total_sectorcount != (free_pagecount - 1) * spp) {
				l2p->follow_node = sl_node_old;
				if (IS_NOT_SAME_L4(l2p->break_type)) {
					l2p->node_left_sector_count += total_sectorcount - (free_pagecount - 1) * spp;
					sl_node_new.sectorCount = sl_node->sectorCount - (total_sectorcount - (free_pagecount - 1) * spp);
					brokenflag = 0;
				}
				else {
					l2p->node_left_sector_count = total_sectorcount - (free_pagecount - 1) * spp;
					sl_node_new.sectorCount = sl_node->sectorCount - l2p->node_left_sector_count;
				}
				sl_node_new.startSector = sl_node->startSector;
				sl_node_new.pData = sl_node->pData;
				sl_node_new.head.next = sl_node->head.next;
				sl_node = &sl_node_new;
			}
			else if (!IS_NOT_SAME_L4(l2p->break_type)) {
				l2p->follow_node = (SectorList *)sl_node_old->head.next;
				l2p->node_left_sector_count = l2p->follow_node->sectorCount;
			}
		}

		new_pagelist(l2p, sl_node, &pl, zone, brokenflag);

		fill_l2p_sectorid(l2p, sl_node);

		if (IS_BREAK(l2p->break_type))
			break;
		else if (sl_node->head.next == NULL)
			l2p->follow_node = NULL;
	}
	return pl;
err:
	return NULL;
}

static void lock_cache(int cm, L2pConvert *l2p, PageInfo **pi)
{
	if (l2p->L4_startsectorid == -1)
		CacheManager_lockCache(cm,l2p->follow_node->startSector,pi);
	else
		CacheManager_lockCache(cm,l2p->follow_node->startSector + l2p->follow_node->sectorCount - l2p->node_left_sector_count,pi);
}

static void unlock_cache(int cm, PageInfo *pi)
{
	CacheManager_unlockCache(cm,pi);
}

static int start_reread_ecc_error_handle(int context, unsigned int zoneid)
{
	Message reread_ecc_error_msg;
	int msghandle;
	Context *conptr = (Context *)context;
	ForceRecycleInfo forceinfo;

	forceinfo.context = context;
	forceinfo.pagecount = -1;
	forceinfo.suggest_zoneid = zoneid;
	reread_ecc_error_msg.msgid = WRITE_READ_ECC_ERROR_ID;
	reread_ecc_error_msg.prio = READ_ECC_ERROR_PRIO;
	reread_ecc_error_msg.data = (int)&forceinfo;

	msghandle = Message_Post(conptr->thandle, &reread_ecc_error_msg, WAIT);
	return Message_Recieve(conptr->thandle, msghandle);
}

int L2PConvert_WriteSector ( int handle, SectorList *sl )
{
	Zone *zone = NULL;
	PageInfo *pi = NULL;
	PageList *pl = NULL;
	int context = handle;
	Context *conptr = (Context*)context;
	CacheManager *cm = conptr->cachemanager;
	BuffListManager *blm = conptr->blm;
	int ret = 0;
	L2pConvert *l2p = (L2pConvert *)(conptr->l2pid);
	int pageinfo_eccislarge = 0;
	int is_not_ecc_error = 1;

	if (sl == NULL){
		ndprint(L2PCONVERT_ERROR,"ERROR:func: %s line: %d. SECTORLIST IS NULL!\n",__func__,__LINE__);
		return -1;
	}

	Recycle_Lock(context);
#ifdef STATISTICS_DEBUG
	Get_StartTime(conptr->timebyte,1);
#endif

	INIT_L2P(l2p);

	recycle_zone_prepare(context);

	zone = get_write_zone(context);
	if(zone == NULL) {
		ndprint(L2PCONVERT_ERROR,"ERROR:get_write_zone error func %s line %d \n",
					__FUNCTION__,__LINE__);
		ret = -1;
		goto exit;
	}

	l2p->follow_node = sl;
	while (1) {
		if (l2p->follow_node == NULL)
			break;

		lock_cache((int)cm,l2p,&pi);
#ifdef L2P_PAGEINFO_DEBUG
		L2p_Debug_SaveCacheData(l2p->debug,pi);
#endif
		pl = create_pagelist(l2p,pi,&zone);

		ret = update_l1l2l3l4(l2p,pi,pl,zone);
		if(ret != 0) {
			ndprint(L2PCONVERT_ERROR,"ERROR:update_l1l2l3l4 error func %s line %d \n",
				__FUNCTION__,__LINE__);
			goto exit;
		}
		zone->currentLsector = l2p->sectorid[0];
		ret = Zone_MultiWritePage(zone, l2p->pagecount, pl, pi);
#ifdef L2P_PAGEINFO_DEBUG
		L2p_Debug_CheckData(l2p->debug,pi,l2p->pagecount + 1);
#endif
		if(ISECCERROR(ret)) {
			ndprint(L2PCONVERT_INFO,"Start reread pageinfo ecc error handle,func %s line %d \n",
					__FUNCTION__,__LINE__);
			CacheManager_DropCache ((int)cm,l2p->sectorid);
			unlock_cache((int)cm, pi);
			start_reread_ecc_error_handle(context, zone->ZoneID);
			zone = ZoneManager_GetCurrentWriteZone(context);
			INIT_L2P(l2p);
			l2p->follow_node = l2p->prev_node;
			is_not_ecc_error = 0;
			ret = 0;
		}else {
			if(ECCTOOLARGE(ret)){
				pageinfo_eccislarge = 1;
			}
			unlock_cache((int)cm, pi);
		}
		BuffListManager_freeAllList((int)blm,(void **)&pl,sizeof(PageList));

		if (IS_NOT_SAME_L4(l2p->break_type))
			l2p->l4_is_new = 1;
		if (IS_NO_ENOUGH_PAGES(l2p->break_type)) {
			zone = alloc_new_zone_write (context,zone);
			l2p->zone_is_new = 1;
		}

		memset(l2p->sectorid, 0xff, cm->L4InfoLen);
		l2p->pagecount = 0;

		/* alloc 4 zone beforehand */
		if (l2p->zone_is_new || l2p->alloced_new_zone) {
			ret = write_data_prepare(context);
			if (ret == -1) {
				ndprint(L2PCONVERT_ERROR,"ERROR:write_data_prepare error func %s line %d \n",
					__FUNCTION__,__LINE__);
				break;
			}
			l2p->alloced_new_zone = 0;
			zone = ZoneManager_GetCurrentWriteZone(context);
		}
	}
#ifdef STATISTICS_DEBUG
	Calc_Speed(conptr->timebyte, (void*)sl, 1, 1);
#endif
exit:
	conptr->t_startrecycle = nd_getcurrentsec_ns();
	if(pageinfo_eccislarge && is_not_ecc_error){
		ndprint(L2PCONVERT_INFO,"Pageinfo ecc is too large,Start reread pageinfo ecc error handle\n");
		start_reread_ecc_error_handle(context, zone->ZoneID);
	}
	Recycle_Unlock(context);
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
