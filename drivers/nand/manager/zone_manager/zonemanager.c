#include "clib.h"
#include "nanddebug.h"
#include "zonemanager.h"
#include "zone.h"
#include "zonememory.h"
#include "vnandinfo.h"
#include "context.h"
#include "zone.h"
#include "pagelist.h"
#include "NandAlloc.h"
#include "vNand.h"
#include "l2vNand.h"
#include "zonemanager.h"
#include "nandsigzoneinfo.h"
#include "nandzoneinfo.h"
#include "taskmanager.h"
#include "nmbitops.h"
#include "nandpageinfo.h"
#include "cachemanager.h"
#include "errhandle.h"
//#include "badblockinfo.h"

#define ZONEPAGE1INFO(vnand)      ((vnand)->v2pp->_2kPerPage)
#define ZONEPAGE2INFO(vnand)      ((vnand)->v2pp->_2kPerPage + 1)
#define ZONEMEMSIZE(vnand)      ((vnand)->BytePerPage * 4)
#define L4INFOLEN 1024

void  ZoneManager_SetCurrentWriteZone(int context,Zone *zone);

/**
 *	init_free_node - Initialize hashnode of freezone
 *
 *	@zonep: operate object
 */
static int init_free_node(ZoneManager *zonep)
{
	return HashNode_init(&zonep->freeZone, zonep->sigzoneinfo, zonep->zoneID, zonep->zoneIDcount);
}

/**
 *	deinit_free_node - Deinit hashnode of freezone
 *
 *	@zonep: operate object
 */
static void deinit_free_node(ZoneManager *zonep)
{
	HashNode_deinit(&zonep->freeZone);
}

/**
 *	alloc_free_node - Get a free zone
 *
 *	@zonep: operate object
 */
static SigZoneInfo *alloc_free_node(ZoneManager *zonep)
{
	return HashNode_get(zonep->freeZone);
}

/**
 *	insert_free_node - Insert a zone to freezone
 *
 *	@zonep: operate object
 *	@zoneID: which zone to insert
 */
static int insert_free_node(ZoneManager *zonep,unsigned short zoneID)
{
	return HashNode_insert(zonep->freeZone, &(zonep->sigzoneinfo)[zoneID]);
}

/**
 *	init_used_node - Initialize hash of usezone
 *
 *	@zonep: operate object
 */
static int init_used_node(ZoneManager *zonep)
{
	return Hash_init(&zonep->useZone, zonep->sigzoneinfo, zonep->zoneID, zonep->zoneIDcount);
}

/**
 *	deinit_used_node - Deinit hash of usezone
 *
 *	@zonep: operate object
 */
static void deinit_used_node(ZoneManager * zonep)
{
	Hash_deinit(&zonep->useZone);
}

/**
 *	insert_used_node - Insert a zone to usezone
 *
 *	@zonep: operate object
 *	@zoneID: which zone to insert
 */
static int insert_used_node(ZoneManager *zonep,SigZoneInfo *zone)
{
	return Hash_Insert(zonep->useZone, zone);
}

/**
 *	delete_used_node - Delete a zone from usezone
 *
 *	@zonep: operate object
 *	@zoneID: which zone to delete
 */
static int delete_used_node(ZoneManager *zonep,unsigned short zoneID)
{
	return Hash_delete(zonep->useZone, zonep->sigzoneinfo + zoneID);
}

/**
 *	ZoneManager_Move_UseZone_to_FreeZone - Move Use Zone to Free Zone
 *
 *	@zonep: operate object
 *	@zoneID: which zone to insert
 */
int ZoneManager_Move_UseZone_to_FreeZone(ZoneManager *zonep,unsigned short zoneID)
{
	int ret;
	ret = delete_used_node(zonep, zoneID);
	if (ret < 0) {
		ndprint(ZONEMANAGER_ERROR,"delete_used_node error func %s line %d \n",
				__FUNCTION__, __LINE__);
	}
	return insert_free_node(zonep, zoneID);
}

/**
 *	get_L2L3_len - get length of L2 and L3
 *
 *	@zonep: operate object
 */
static void get_L2L3_len(ZoneManager *zonep)
{
	Context *conptr = (Context *)(zonep->context);

	zonep->l2infolen = conptr->L2InfoLen;
	zonep->l3infolen = conptr->L3InfoLen;
	zonep->l4infolen = conptr->L4InfoLen;
}

/**
 *	alloc_zonemanager_memory - alloc some memory
 *
 *	@zonep: operate object
 *	@vnand: virtual nand
 */
static int alloc_zonemanager_memory(ZoneManager *zonep,VNandInfo *vnand)
{
	int i;
	int ret = 0;
	unsigned int zonenum = 0;
	Context *conptr = (Context *)(zonep->context);

	zonep->vnand = vnand;
	zonenum = zonep->pt_zonenum;

	zonep->zoneID = (unsigned short *)Nand_VirtualAlloc(zonenum * sizeof(unsigned short));
	if(zonep->zoneID == NULL)
	{
		ndprint(ZONEMANAGER_ERROR,"alloc memory fail func %s line %d \n",
			__FUNCTION__,__LINE__);

		return -1;
	}

	zonep->sigzoneinfo = (SigZoneInfo *)Nand_VirtualAlloc(zonenum * sizeof(SigZoneInfo));
	ndprint(ZONEMANAGER_INFO, "zonep->sigzoneinfo = %p\n",zonep->sigzoneinfo);
	if(zonep->sigzoneinfo == NULL)
	{
		ndprint(ZONEMANAGER_ERROR,"alloc memory fail func %s line %d \n",
			__FUNCTION__,__LINE__);

		ret = -1;
		goto err0;
	}

	zonep->mem0 = (unsigned char *)Nand_ContinueAlloc(2 * ZONEMEMSIZE(vnand));
	if(zonep->mem0 == NULL)
	{
		ndprint(ZONEMANAGER_ERROR,"alloc memoey fail func %s line %d \n",
			__FUNCTION__,__LINE__);

		ret = -1;
		goto err1;
	}

	for(i = 0;i < zonenum;i++)
		INIT_SIGZONEINFO(zonep->sigzoneinfo+i);

	memset(zonep->memflag,0x0,sizeof(zonep->memflag));
	zonep->L1 = conptr->l1info;
	zonep->maxserial = 0;
	zonep->last_zone = NULL;
	zonep->write_zone = NULL;
	zonep->prev = NULL;
	zonep->next = NULL;
	zonep->last_data_buf = NULL;
	zonep->last_pi = NULL;
	zonep->pl = NULL;
	zonep->last_data_read_error = 0;
	zonep->old_l1info = -1;
	zonep->page0error_zoneidlist = NULL;
	zonep->page1error_zoneidlist = NULL;
	zonep->page2_error_dealt = 0;
	for(i = 0; i < 4 ; i++) {
		zonep->aheadflag[i] = 0;
		zonep->ahead_zone[i] = NULL;
	}
	conptr->top = zonep->sigzoneinfo;

	InitNandMutex(&zonep->HashMutex);

	get_L2L3_len(zonep);
	zonep->zonemem = ZoneMemory_Init(sizeof(Zone));
	zonep->zoneIDcount = zonenum;
	return 0;

err1:
	Nand_ContinueFree(zonep->sigzoneinfo);
err0:
	Nand_ContinueFree(zonep->zoneID);
	return ret;

}

/**
 *	free_zonemanager_memory - free memory
 *
 *	@zonep: operate object
 */
static void free_zonemanager_memory(ZoneManager *zonep)
{
	Nand_VirtualFree(zonep->zoneID);
	Nand_VirtualFree(zonep->sigzoneinfo);
	Nand_ContinueFree(zonep->mem0);
	DeinitNandMutex(&zonep->HashMutex);
	ZoneMemory_DeInit(zonep->zonemem);
}

/**
 *	read_zone_info_page - read zone infopage
 *
 *	@zonep: operate object
 *	@zoneid: id of zone
 *	@pl: pagelist
 *	@pageid: offset pageid
 */
static int read_zone_info_page(ZoneManager *zonep,unsigned short zoneid,PageList *pl,unsigned int page)
{
	//unsigned int startblockno = BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,zoneid);
	unsigned int startblockno = zoneid * BLOCKPERZONE(zonep->vnand);
	pl->startPageID = startblockno * zonep->vnand->PagePerBlock + page;

	return vNand_MultiPageRead(zonep->vnand,pl);
}

/**
 *	read_zone_page0 - read page0 of zone
 *
 *	@zonep: operate object
 *	@zoneid: id of zone
 *	@pl: pagelist
 */
static int read_zone_page0(ZoneManager *zonep,unsigned short zoneid,PageList *pl)
{
	return read_zone_info_page(zonep,zoneid,pl,0);
}

/**
 *	read_zone_page1 - read page1 of zone
 *
 *	@zonep: operate object
 *	@zoneid: id of zone
 *	@pl: pagelist
 */
static int read_zone_page1(ZoneManager *zonep,unsigned short zoneid,PageList *pl)
{
	return read_zone_info_page(zonep,zoneid,pl,ZONEPAGE1INFO(zonep->vnand));
}

/**
 *	read_zone_page2 - read page2 of zone
 *
 *	@zonep: operate object
 *	@zoneid: id of zone
 *	@pl: pagelist
 */
static int read_zone_page2(ZoneManager *zonep,unsigned short zoneid,PageList *pl)
{
	return read_zone_info_page(zonep,zoneid,pl,ZONEPAGE2INFO(zonep->vnand));
}

/**
 *	start_read_first_pageinfo_error_handle - start to deal with the error when read first pageinfo of maximum serial zone
 *
 *	@zonep: operate object
 *	@zoneid: error zoneid
 */
static int start_read_first_pageinfo_error_handle(ZoneManager *zonep, unsigned short zoneid)
{
	Message read_first_pageinfo_error_msg;
	int msghandle;
	Context *conptr = (Context *)(zonep->context);
	ErrInfo errinfo;
	errinfo.context = zonep->context;
	errinfo.err_zoneid = zoneid;

	read_first_pageinfo_error_msg.msgid = READ_FIRST_PAGEINFO_ERROR_ID;
	read_first_pageinfo_error_msg.prio = READ_FIRST_PAGEINFO_ERROR_PRIO;
	read_first_pageinfo_error_msg.data = (int)&errinfo;

	msghandle = Message_Post(conptr->thandle, &read_first_pageinfo_error_msg, WAIT);
	return Message_Recieve(conptr->thandle, msghandle);
}
/**
 *	start_read_page0_error_handle - start to deal with the error when read page0
 *
 *	@zonep: operate object
 *	@zoneid: error zoneid
 */
static int start_read_page0_error_handle(ZoneManager *zonep, unsigned short zoneid)
{
	Message read_page0_error_msg;
	int msghandle;
	Context *conptr = (Context *)(zonep->context);
	ErrInfo errinfo;
	errinfo.context = zonep->context;
	errinfo.err_zoneid = zoneid;

	read_page0_error_msg.msgid = READ_PAGE0_ERROR_ID;
	read_page0_error_msg.prio = READ_PAGE0_ERROR_PRIO;
	read_page0_error_msg.data = (int)&errinfo;

	msghandle = Message_Post(conptr->thandle, &read_page0_error_msg, WAIT);
	return Message_Recieve(conptr->thandle, msghandle);
}

/**
 *	start_read_page1_error_handle - start to deal with the error when read page1
 *
 *	@zonep: operate object
 *	@zoneid: error zoneid
 */
static int start_read_page1_error_handle(ZoneManager *zonep, unsigned short zoneid)
{
	Message read_page1_error_msg;
	int msghandle;
	Context *conptr = (Context *)(zonep->context);
	ErrInfo errinfo;
	errinfo.context = zonep->context;
	errinfo.err_zoneid = zoneid;

	read_page1_error_msg.msgid = READ_PAGE1_ERROR_ID;
	read_page1_error_msg.prio = READ_PAGE1_ERROR_PRIO;
	read_page1_error_msg.data = (int)&errinfo;

	msghandle = Message_Post(conptr->thandle, &read_page1_error_msg, WAIT);
	return Message_Recieve(conptr->thandle, msghandle);
}

/**
 *	start_read_page2_error_handle - start to deal with the error when read page2
 *
 *	@zonep: operate object
 */
static int start_read_page2_error_handle(ZoneManager *zonep)
{
	Message read_page2_error_msg;
	int msghandle;
	Context *conptr = (Context *)(zonep->context);
	ErrInfo errinfo;
	errinfo.context = zonep->context;
	errinfo.err_zoneid = zonep->last_zone_id;

	read_page2_error_msg.msgid = READ_PAGE2_ERROR_ID;
	read_page2_error_msg.prio = READ_PAGE2_ERROR_PRIO;
	read_page2_error_msg.data = (int)&errinfo;

	msghandle = Message_Post(conptr->thandle, &read_page2_error_msg, WAIT);
	return Message_Recieve(conptr->thandle, msghandle);
}
/**
 *	unpackage_page0_info - unpake information of page0
 *
 *	@zonep: operate object
 *	@zoneid: id of zone
 */
static int unpackage_page0_info(ZoneManager *zonep,unsigned short zoneid)
{
	NandSigZoneInfo *nandsigzoneinfo = (NandSigZoneInfo *)(zonep->mem0);
	SigZoneInfo *sigzoneinfo = zonep->sigzoneinfo + zoneid;

	sigzoneinfo->badblock = nandsigzoneinfo->badblock;
	sigzoneinfo->lifetime = nandsigzoneinfo->lifetime;
	if (zonep->vnand->v2pp->_2kPerPage == 1)
		sigzoneinfo->validpage = zonep->vnand->PagePerBlock * BLOCKPERZONE(zonep->vnand) - 3;
	else
		sigzoneinfo->validpage = zonep->vnand->PagePerBlock * BLOCKPERZONE(zonep->vnand) - zonep->vnand->v2pp->_2kPerPage * 2;

	return 0;
}

/**
 *	find_maxserialnumber
 *
 *	@zonep: operate object
 *	@maxserial: maximum serial number
 *	@recordzone: record zone
 */
static int find_maxserialnumber(ZoneManager *zonep,

			unsigned int *maxserial,unsigned short *recordzone)
{
	unsigned short zoneid;
	unsigned int max = 0;
	NandZoneInfo *nandzoneinfo = (NandZoneInfo *)(zonep->mem0);
	SigZoneInfo *oldsigp = NULL;

	//update localzone info in zonep->sigzoneinfo
	zoneid = nandzoneinfo->localZone.ZoneID;
	if (zoneid != 0xffff && zoneid >= 0 && zoneid < zonep->pt_zonenum) {
		oldsigp = (SigZoneInfo *)(zonep->sigzoneinfo + zoneid);
		oldsigp->pre_zoneid = nandzoneinfo->preZone.ZoneID;
		oldsigp->next_zoneid = nandzoneinfo->nextZone.ZoneID;
		CONV_ZI_SZ(&nandzoneinfo->localZone,oldsigp);
	}

	//update prevzone info in zonep->sigzoneinfo
	zoneid = nandzoneinfo->preZone.ZoneID;
	if (zoneid != 0xffff && zoneid >= 0 && zoneid < zonep->pt_zonenum) {
		oldsigp = (SigZoneInfo *)(zonep->sigzoneinfo + zoneid);
		if (nandzoneinfo->preZone.ZoneID != 0xffff
			&& oldsigp->lifetime != 0xffffffff
			&& oldsigp->lifetime <= nandzoneinfo->preZone.lifetime)
			CONV_ZI_SZ(&nandzoneinfo->preZone,oldsigp);
	}

	//update nextzone info in zonep->sigzoneinfo
	zoneid = nandzoneinfo->nextZone.ZoneID;
	if (zoneid != 0xffff && zoneid >= 0 && zoneid < zonep->pt_zonenum) {
		oldsigp = (SigZoneInfo *)(zonep->sigzoneinfo + zoneid);
		if (nandzoneinfo->nextZone.ZoneID != 0xffff
			&& oldsigp->lifetime != 0xffffffff
			&& oldsigp->lifetime <= nandzoneinfo->nextZone.lifetime)
			CONV_ZI_SZ(&nandzoneinfo->nextZone,oldsigp);
	}

	max = nandzoneinfo->serialnumber;
	if(max > *maxserial)
	{
		*maxserial = max;
		*recordzone = nandzoneinfo->localZone.ZoneID;
	}

	return 0;
}

static void insert_zoneidlist(ZoneManager *zonep, int errtype, unsigned short zoneid)
{
	ZoneIDList *zl = NULL;
	ZoneIDList *zl_node;
	int blmid = (int)((Context *)(zonep->context))->blm;

	if (errtype == PAGE0)
		zl = zonep->page0error_zoneidlist;
	else if (errtype == PAGE1)
		zl = zonep->page1error_zoneidlist;

	if (zl == NULL) {
		zl = (ZoneIDList *)BuffListManager_getTopNode(blmid,sizeof(ZoneIDList));
		zl_node = zl;
		if (errtype == PAGE0)
			zonep->page0error_zoneidlist = zl;
		else if (errtype == PAGE1)
			zonep->page1error_zoneidlist = zl;
	}
	else
		zl_node = (ZoneIDList *)BuffListManager_getNextNode(blmid,(void *)zl,sizeof(ZoneIDList));

	zl_node->ZoneID = zoneid;
}

/**
 *	get_pt_badblock_count - get bad block number of partition
 *
 *	@vnand: virtual nand
 */
/*
static int get_pt_badblock_count(VNandInfo *vnand)
{
	int i;
	int count = 0;

	for (i = 0; i < (vnand->BytePerPage * BADBLOCKINFOSIZE) >> 2; i++) {
		if (vnand->pt_badblock_info[i] == 0xffffffff)
			break;

		count++;
	}

	return count;
}
*/

/**
 *	scan_sigzoneinfo_fill_node - scan sigzoneinfo and fill it to hashtable
 *
 *	@zonep: operate object
 *	@pl: pagelist
 */
static int scan_sigzoneinfo_fill_node(ZoneManager *zonep,PageList *pl)
{
	unsigned int i = 0;
	SigZoneInfo *sigp = NULL;
	int ret = -1;
	int last_zone_badblocknum = 0;
	Context *conptr = (Context *)(zonep->context);
	VNandInfo *vnand = &conptr->vnand;
	unsigned int zonenum = zonep->pt_zonenum;

	for(i = 0 ; i < zonenum ; i++)
	{
		sigp = (SigZoneInfo *)(zonep->sigzoneinfo + i);

		if(sigp->lifetime == 0xffffffff)
		{
			read_zone_page0(zonep,i,pl);
			ret = pl->retVal;
			pl->retVal = 0;
			if (!ISNOWRITE(ret)) {
				if(ISERROR(ret))
					insert_zoneidlist(zonep,PAGE0,i);
				unpackage_page0_info(zonep,i);
			}
			else {
				sigp->badblock = 0;
				sigp->lifetime = -1;
				if (vnand->v2pp->_2kPerPage == 1)
					sigp->validpage = zonep->vnand->PagePerBlock * BLOCKPERZONE(zonep->vnand) - 3;
				else
					sigp->validpage = zonep->vnand->PagePerBlock * BLOCKPERZONE(zonep->vnand) - zonep->vnand->v2pp->_2kPerPage * 2;
			}
			ret = insert_free_node(zonep,i);
			if(ret != 0)
			{
				ndprint(ZONEMANAGER_ERROR,"insert free node error func %s line %d \n",

							__FUNCTION__,__LINE__);
				return ret;
			}
		}
		else
		{
			ret = insert_used_node(zonep,sigp);
			if(ret != 0)
			{
				ndprint(ZONEMANAGER_ERROR,"insert used node error func %s line %d \n",

							__FUNCTION__,__LINE__);
				return ret;
			}
		}
	}

	/*last_zone_badblocknum = (BLOCKPERZONE(vnand) - (vnand->TotalBlocks - get_pt_badblock_count(vnand)) %
	  BLOCKPERZONE(vnand)) % BLOCKPERZONE(vnand);*/

	last_zone_badblocknum = (BLOCKPERZONE(vnand) - (vnand->TotalBlocks % BLOCKPERZONE(vnand))) % BLOCKPERZONE(vnand);

	for (i = 0; i < last_zone_badblocknum; i++)
		(zonep->sigzoneinfo + zonep->pt_zonenum - 1)->badblock |= (1 << (BLOCKPERZONE(vnand) - 1 - i));

	return 0;
}

/**
 *	fill_ahead_zone - alloc 4 zone beforehand
 *
 *	@zonep: operate object
 */
static void fill_ahead_zone ( ZoneManager *zonep )
{
	int i;
	unsigned int count = 0;
	Zone *zone = NULL;

	count = ZoneManager_GetAheadCount(zonep->context);
	for (i = 0; i < 4 - count; i++){
		zone = ZoneManager_AllocZone(zonep->context);
		if (!zone)
			return;
		ZoneManager_SetAheadZone(zonep->context,zone);
	}
}

/**
 *	scan_page_info - scan page info
 *
 *	@zonep: operate object
 */
static int scan_page_info(ZoneManager *zonep)
{
	PageList pagelist;
	PageList *plt = &pagelist;
	unsigned int max_serial = 0;
	unsigned short max_zoneid = -1;
	unsigned int i = 0;
	int ret = -1;
	unsigned int zonenum = zonep->pt_zonenum;

	plt->OffsetBytes = 0;
	plt->Bytes = zonep->vnand->BytePerPage;
	plt->retVal = 0;
	plt->pData = zonep->mem0;
	(plt->head).next = NULL;
 
	for(i = 0 ; i < zonenum ; i++)
	{
		read_zone_page1(zonep,i,plt);
		ret = plt->retVal;
		plt->retVal = 0;
		if (ISNOWRITE(ret))
			continue;
		else if (ISERROR(ret))
			insert_zoneidlist(zonep,PAGE1,i);
		else
			find_maxserialnumber(zonep,&max_serial,&max_zoneid);
		}

	ret = scan_sigzoneinfo_fill_node(zonep,plt);
	if (ret < 0) {
		ndprint(ZONEMANAGER_ERROR,"ERROR: scan_sigzoneinfo_fill_node func %s line %d\n",
			__FUNCTION__, __LINE__);
		return ret;
	}

	fill_ahead_zone(zonep);
	zonep->maxserial = max_serial;
	zonep->last_zone_id = max_zoneid;
	if (max_zoneid != 0xffff)
	{
		read_zone_page2(zonep,max_zoneid,plt);
		ret = plt->retVal;
		plt->retVal = 0;
		if(ISERROR(ret))
		{
			if(ISNOWRITE(ret)){
				memset(zonep->L1->page,0xff,zonep->vnand->BytePerPage);
			}
			else{
				zonep->page2_error_dealt = 1;
				ret = start_read_page2_error_handle(zonep);
				if (ret < 0) {
					ndprint(ZONEMANAGER_ERROR,"ERROR: page2_error_handle func %s line %d\n",
							__FUNCTION__, __LINE__);
					return ret;
				}
			}
		}
		else{
			memcpy(zonep->L1->page, zonep->mem0, zonep->vnand->BytePerPage);
		}
	}
	else{
		memset(zonep->L1->page,0xff,zonep->vnand->BytePerPage);
	}

	return 0;
}

/**
 *	error_handle - error handle
 *
 *	@zonep: operate object
 */
static int error_handle(ZoneManager *zonep)
{
	int ret = 0;
	struct singlelist *pos;
	ZoneIDList *zl;
	int blmid = (int)((Context *)(zonep->context))->blm;
	
	if (zonep->page0error_zoneidlist) {
		singlelist_for_each(pos,&zonep->page0error_zoneidlist->head) {
			zl = singlelist_entry(pos,ZoneIDList,head);
			ret = start_read_page0_error_handle(zonep,zl->ZoneID);
			if (ret < 0) {
				ndprint(ZONEMANAGER_ERROR,"ERROR: page0_error_handle func %s line %d\n",
					__FUNCTION__, __LINE__);
				return ret;
			}
		}
		BuffListManager_freeAllList(blmid,(void **)&zonep->page0error_zoneidlist,sizeof(ZoneIDList));
	}

	if (zonep->page1error_zoneidlist) {
		singlelist_for_each(pos,&zonep->page1error_zoneidlist->head) {
			zl = singlelist_entry(pos,ZoneIDList,head);
			ret = start_read_page1_error_handle(zonep,zl->ZoneID);
			if (ret < 0) {
				ndprint(ZONEMANAGER_ERROR,"ERROR: page1_error_handle func %s line %d\n",
					__FUNCTION__, __LINE__);
				return ret;
			}
		}
		BuffListManager_freeAllList(blmid,(void **)&zonep->page1error_zoneidlist,sizeof(ZoneIDList));
	}

	return ret;
}

/**
 *	alloc_zone - alloc memory of zone
 *
 *	@zonep: operate object
 */
static inline Zone *alloc_zone(ZoneManager *zonep)
{
	return (Zone *)ZoneMemory_NewUnit(zonep->zonemem);
}

/**
 *	free_zone - free memory of zone
 *
 *	@zonep: operate object
 *	@zone: which to free
 */
static inline void free_zone(ZoneManager *zonep,Zone *zone)
{
	ZoneMemory_DeleteUnit(zonep->zonemem,(void*)zone);
}

/**
 *	alloc_pageinfo  -  alloc L1Info, L2Info, L3Info and L4Info of pageinfo
 *
 *	@pageinfo: which need to alloc
 *	@zonep: to konw whether L2InfoLen and L3InfoLen are 0 or not
 */
static int alloc_pageinfo(PageInfo *pageinfo, ZoneManager *zonep)
{
	pageinfo->L1Info = (unsigned char *)(zonep->L1->page);

	if (zonep->l2infolen) {
		pageinfo->L2Info = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * zonep->l2infolen);
		if (!(pageinfo->L2Info)) {
			ndprint(ZONEMANAGER_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
			goto ERROR1;
		}
	}

	if (zonep->l3infolen) {
		pageinfo->L3Info = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * zonep->l3infolen);
		if (!(pageinfo->L3Info)) {
			ndprint(ZONEMANAGER_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
			goto ERROR2;
		}
	}

	pageinfo->L4Info = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * zonep->l4infolen);
	if (!(pageinfo->L4Info)) {
		ndprint(ZONEMANAGER_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		goto ERROR3;
	}

	return 0;

ERROR3:
	if (zonep->l3infolen)
		Nand_VirtualFree(pageinfo->L3Info);
ERROR2:
	if (zonep->l2infolen)
		Nand_VirtualFree(pageinfo->L2Info);
ERROR1:
	return -1;
}

/**
 *	free_pageinfo  -  free L1Info, L2Info, L3Info and L4Info of pageinfo
 *
 *	@pageinfo: which need to free
 *	@zonep: to konw whether L2InfoLen and L3InfoLen are 0 or not
 */
static void free_pageinfo(PageInfo *pageinfo, ZoneManager *zonep)
{
	if (!pageinfo)
		return;

	if (zonep->l2infolen)
		Nand_VirtualFree(pageinfo->L2Info);

	if (zonep->l3infolen)
		Nand_VirtualFree(pageinfo->L3Info);

	Nand_VirtualFree(pageinfo->L4Info);
}

/**
 *	get_usedzone - Get used zone when given zoneid
 *
 *	@zonep: operate object
 *	@zoneid: id for zone
 */
static Zone *get_usedzone(ZoneManager *zonep, unsigned short zoneid)
{
	Zone *zoneptr = NULL;
	unsigned int i = 0;
	int ret = 0;
	unsigned int badblockcount = 0;

	zoneptr = alloc_zone(zonep);
	if(zoneptr == NULL) {
		ndprint(ZONEMANAGER_ERROR,"alloc Zone error func %s line %d\n",
					__FUNCTION__,__LINE__);
		return NULL;
	}

	zoneptr->sigzoneinfo = zonep->sigzoneinfo + zoneid;
	if (zoneptr->sigzoneinfo->pre_zoneid != 0xffff)
		zoneptr->prevzone = zonep->sigzoneinfo + zoneptr->sigzoneinfo->pre_zoneid;
	else
		zoneptr->prevzone = NULL;
	if (zoneptr->sigzoneinfo->next_zoneid != 0xffff)
		zoneptr->nextzone = zonep->sigzoneinfo + zoneptr->sigzoneinfo->next_zoneid;
	else
		zoneptr->nextzone = NULL;
	zoneptr->badblock = (zonep->sigzoneinfo + zoneid)->badblock;
	zoneptr->validpage= (zonep->sigzoneinfo + zoneid)->validpage;
	//zoneptr->startblockID = BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,zoneid);
	zoneptr->startblockID = zoneid * BLOCKPERZONE(zonep->vnand);
	zoneptr->top = zonep->sigzoneinfo;
	zoneptr->L1InfoLen = zonep->L1->len;
	zoneptr->L2InfoLen = zonep->l2infolen;
	zoneptr->L3InfoLen = zonep->l3infolen;
	zoneptr->L4InfoLen = zonep->l4infolen;

	for(i = 0 ; i < 4 ; i++) {
		if(zonep->memflag[i] == 0)
			break;
	}

	if(i == 4) {
		ndprint(ZONEMANAGER_ERROR,"PANIC ERROR func %s line %d \n",
					__FUNCTION__,__LINE__);
		goto err;
	}

	ret = delete_used_node(zonep,zoneid);
	if(ret != 0) {
		ndprint(ZONEMANAGER_ERROR,"delete used node error func %s line %d\n",
					__FUNCTION__,__LINE__);
		goto err;
	}

	while(nm_test_bit(badblockcount,&(zoneptr->badblock)) && (++badblockcount));

	zoneptr->memflag = i;
	zoneptr->ZoneID = zoneid;
	zoneptr->mem0 = zonep->mem0 + i * zonep->vnand->BytePerPage;
	zoneptr->L1Info = (unsigned char *)(zonep->L1->page);
	zoneptr->vnand = zonep->vnand;
	zoneptr->context = zonep->context;
	return zoneptr;

err:
	free_zone(zonep,zoneptr);
	return NULL;
}

/**
 *	get_maxserial_zone - Get maximum serialnumber zone
 *
 *	@zonep: operate object
 */
static int get_maxserial_zone(ZoneManager *zonep)
{
	Zone *zoneptr = NULL;
	unsigned int badblockcount = 0;

	zoneptr = get_usedzone(zonep,zonep->last_zone_id);
	if(!zoneptr) {
		ndprint(ZONEMANAGER_ERROR,"get_usedzone error func %s line %d\n",
					__FUNCTION__,__LINE__);
		return -1;
	}

	while(nm_test_bit(badblockcount,&(zoneptr->badblock)) && (++badblockcount));

	zoneptr->sumpage = (BLOCKPERZONE(zonep->vnand) - badblockcount) * zonep->vnand->PagePerBlock;

	if (zoneptr->vnand->v2pp->_2kPerPage == 1) {
		zoneptr->pageCursor = badblockcount * zoneptr->vnand->PagePerBlock + ZONEPAGE1INFO(zoneptr->vnand);
		zoneptr->allocPageCursor = zoneptr->pageCursor + 1;
		zoneptr->allocedpage = 3;
	}
	else if (zoneptr->vnand->v2pp->_2kPerPage > 1) {
		zoneptr->pageCursor = badblockcount * zoneptr->vnand->PagePerBlock + ZONEPAGE1INFO(zoneptr->vnand);
		zoneptr->allocPageCursor = zoneptr->vnand->v2pp->_2kPerPage * 2 - 1;
		zoneptr->allocedpage = zoneptr->vnand->v2pp->_2kPerPage * 2;
	}

	zonep->last_zone = zoneptr;
	ZoneManager_SetCurrentWriteZone(zonep->context,zonep->last_zone);

	return 0;
}

/**
 *	copy_pageinfo - copy pageinfo from src_pi to des_pi
 *
 *	@zonep: operate object
 *	@des_pi: Destination pageinfo
 *	@src_pi:source pageinfo
 */
static void copy_pageinfo(ZoneManager *zonep, PageInfo *des_pi, PageInfo *src_pi)
{
	des_pi->L1Index 	= src_pi->L1Index;
	des_pi->L1InfoLen 	= src_pi->L1InfoLen;
	des_pi->L2Index 	= src_pi->L2Index;
	des_pi->L2InfoLen 	= src_pi->L2InfoLen;
	des_pi->L3Index 	= src_pi->L3Index;
	des_pi->L3InfoLen 	= src_pi->L3InfoLen;
	des_pi->L4InfoLen 	= src_pi->L4InfoLen;
	des_pi->PageID 		= src_pi->PageID;
	des_pi->zoneID 		= src_pi->zoneID;

	if (zonep->l2infolen)
		memcpy(des_pi->L2Info, src_pi->L2Info, zonep->l2infolen);
	if (zonep->l3infolen)
		memcpy(des_pi->L3Info, src_pi->L3Info, zonep->l3infolen);
	memcpy(des_pi->L4Info, src_pi->L4Info, zonep->l4infolen);
}

/**
 *	start_follow_recycle - start follow recycle
 *
 *	@zonep: operate object
 */
static void start_follow_recycle(ZoneManager *zonep)
{
#ifndef NO_ERROR
	Message follow_recycle_msg;
	int msghandle;
	Context *conptr = (Context *)(zonep->context);
	ndprint(ZONEMANAGER_INFO,"WARNNING: bootprepare find a error,Deal with it!\n");

	follow_recycle_msg.msgid = FOLLOW_RECYCLE_ID;
	follow_recycle_msg.prio = FOLLOW_RECYCLE_PRIO;
	follow_recycle_msg.data = zonep->context;

	msghandle = Message_Post(conptr->thandle, &follow_recycle_msg, WAIT);
	Message_Recieve(conptr->thandle, msghandle);
#endif
}

/**
 *	start_boot_recycle - start boot recycle
 *
 *	@zonep: operate object
 */
static void start_boot_recycle(ZoneManager *zonep)
{
#ifndef NO_ERROR
	Message boot_recycle_msg;
	int msghandle;
	Context *conptr = (Context *)(zonep->context);
	ndprint(ZONEMANAGER_INFO,"WARNNING: bootprepare find a error,Deal with it!\n");

	boot_recycle_msg.msgid = FOLLOW_RECYCLE_ID;
	boot_recycle_msg.prio = FOLLOW_RECYCLE_PRIO;
	boot_recycle_msg.data = zonep->context;

	msghandle = Message_Post(conptr->thandle, &boot_recycle_msg, WAIT);
	Message_Recieve(conptr->thandle, msghandle);
#endif
}

/**
 *	get_last_pageinfo - get last pageinfo of maximum serial number zone
 *
 *	@zonep: operate object
 *	@pi: which need to filled
 */
static int get_last_pageinfo(ZoneManager *zonep, PageInfo **pi)
{
	int ret = 0;
	int flag = 0;
	PageInfo pageinfo;
	Zone *zone = zonep->last_zone;
	PageInfo *prev_pi = &pageinfo;
	Context *conptr = (Context *)(zonep->context);
	unsigned int *l1info = conptr->l1info->page;

	ret = alloc_pageinfo(prev_pi, zonep);
	if (ret != 0) {
		ndprint(ZONEMANAGER_ERROR,"alloc_pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}

	ret = Zone_FindFirstPageInfo(zone,prev_pi);
	if (ISERROR(ret)) {
		if (ISNOWRITE(ret))
			return 1;
		else {
			ret = start_read_first_pageinfo_error_handle(zonep,zonep->last_zone_id);
			if (ret != 0) {
				ndprint(ZONEMANAGER_ERROR,"first_pageinfo_error_handle error func %s line %d \n",
							__FUNCTION__,__LINE__);
				return -1;
			}
			return 1;
		}
	}
	zonep->old_l1info = l1info[prev_pi->L1Index];
	l1info[prev_pi->L1Index] = prev_pi->PageID;

	while (1) {
		if (zone->NextPageInfo == 0)
			break;
		else if (zone->NextPageInfo == 0xffff) {
			ndprint(ZONEMANAGER_ERROR,"pageinfo data error func %s line %d \n",
						__FUNCTION__,__LINE__);
			return -1;
		}

		ret = Zone_FindNextPageInfo(zone,*pi);
		if (ISERROR(ret)) {
			if (ISNOWRITE(ret)) {
				copy_pageinfo(zonep, *pi, prev_pi);
				break;
			}
			else {
				CacheManager_Init(zonep->context);
				start_boot_recycle(zonep);
				flag = 1;
				goto exit;
			}
		}
		else {
			copy_pageinfo(zonep, prev_pi, *pi);
			zonep->old_l1info = l1info[prev_pi->L1Index];
			l1info[prev_pi->L1Index] = prev_pi->PageID;
		}
	}
exit:
	free_pageinfo(prev_pi, zonep);
	zonep->last_pi = *pi;
	if (flag)
		return 2;
	return 0;
}

/**
 *	page_in_current_zone - whether pageid in current zone or not
 *
 *	@zonep: operate object
 *	@zoneid: id of zone
 *	@pageid: number of page
*/
static int page_in_current_zone (ZoneManager *zonep, unsigned short zoneid, unsigned int pageid)
{
	int ppb = zonep->vnand->PagePerBlock;

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
 *	alloc_last_data_buf - alloc buf of last data
 *
 *	@zonep: operate object
 *	@pi: last pageinfo
 */
static int alloc_last_data_buf(ZoneManager *zonep, int pagecount)
{
	zonep->last_data_buf = (unsigned char *)Nand_ContinueAlloc(zonep->vnand->BytePerPage * pagecount);
	if (zonep->last_data_buf == NULL) {
		ndprint(ZONEMANAGER_ERROR,"Nand_ContinueAlloc error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}

	memset(zonep->last_data_buf, 0xff, zonep->vnand->BytePerPage * pagecount);

	return 0;
}

/**
 *	Create_read_pagelist - create read pagelist of last data
 *
 *	@zonep: operate object
 *	@pi: last pageinfo
 */
static PageList *Create_read_pagelist(ZoneManager *zonep, PageInfo *pi)
{
	int ret = 0;
	int i, j, k;
	int pagecount = 0;
	unsigned int tmp0, tmp1;
	unsigned int l4count = 0;
	struct singlelist *pos;
	PageList *pl = NULL;
	PageList *mpl = NULL;
	unsigned int spp = zonep->vnand->BytePerPage / SECTOR_SIZE;
	BuffListManager *blm = ((Context *)(zonep->context))->blm;
	unsigned short l4infolen = pi->L4InfoLen;
	unsigned int *l4info = (unsigned int *)(pi->L4Info);
	l4count = l4infolen >> 2;

	for(i = 0; i < l4count; i += k) {
		k = 1;
		if ((int)l4info[i] == -1)
			continue;

		tmp0 = l4info[i] / spp;
		if (tmp0 <= pi->PageID || !page_in_current_zone(zonep,zonep->last_zone->ZoneID,tmp0))
			continue;

		for(j = i + 1; j < spp + i && j < l4count; j++) {
			if ((int)l4info[j] == -1)
				break;

			tmp1 = l4info[j] / spp;
			if(tmp1 == tmp0)
				k++;
			else
				break;
		}

		if (pl == NULL){
			pl = (PageList *)BuffListManager_getTopNode((int)blm, sizeof(PageList));
			mpl = pl;
		}else
			pl = (PageList *)BuffListManager_getNextNode((int)blm, (void *)pl, sizeof(PageList));
		pl->startPageID = tmp0;
		pl->OffsetBytes = l4info[i] % spp;
		pl->Bytes = k * SECTOR_SIZE;
		pl->retVal = 0;
		pl->pData = NULL;

		pagecount++;
	}

	zonep->pagecount = pagecount;

	ret = alloc_last_data_buf(zonep,pagecount);
	if (ret != 0) {
		ndprint(ZONEMANAGER_ERROR,"alloc_last_data_buf error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return NULL;
	}

	pagecount = 0;
	singlelist_for_each(pos,&mpl->head) {
		pl = singlelist_entry(pos,PageList,head);
		pl->pData = zonep->last_data_buf + (pagecount++) * zonep->vnand->BytePerPage;
	}

	return mpl;
}

/**
 *	get_badblock_count_between_pageid - get badblock count between pageid
 *
 *	@zonep: operate object
 *	@pagecount: page count of last data
 */
static int get_badblock_count_between_pageid(ZoneManager *zonep, int pagecount)
{
	unsigned int badblockcount = 0;
	int blockcount = pagecount / zonep->vnand->PagePerBlock;

	while(nm_test_bit(badblockcount,&(zonep->last_zone->badblock)) && badblockcount < blockcount)
		badblockcount++;

	return badblockcount;
}

/**
 *	calc_alloced_page - calc alloced page of maximum serial zone
 *
 *	@zonep: operate object
 *	@last_right_pageid: the last pageid which data is right written
 */
static int calc_alloced_page(ZoneManager *zonep, unsigned int last_right_pageid)
{
	unsigned int badblockcount = 0;
	unsigned int start_pageid = zonep->last_zone->startblockID * zonep->vnand->PagePerBlock;

	badblockcount = get_badblock_count_between_pageid(zonep, last_right_pageid - start_pageid);

	return last_right_pageid - badblockcount * zonep->vnand->PagePerBlock - start_pageid;
}

/**
 *	get_current_write_zone_info - get maximun serial number zone info
 *
 *	@zonep: operate object
 */
static void get_current_write_zone_info(ZoneManager *zonep)
{
	int ret = 0;
	unsigned int last_right_pageid = 0;
	Zone *zone = zonep->last_zone;
	BuffListManager *blm = ((Context *)(zonep->context))->blm;

	zonep->pl = Create_read_pagelist(zonep, zonep->last_pi);
	if (zonep->pl) {
		ret = Zone_RawMultiReadPage(zonep->last_zone, zonep->pl);
		if (ret != 0)
			zonep->last_data_read_error = 1;

		last_right_pageid = zonep->last_zone->NextPageInfo + zone->startblockID * zonep->vnand->PagePerBlock;
		BuffListManager_freeAllList((int)blm,(void **)(&zonep->pl),sizeof(PageList));
	}

	zone->pageCursor = zonep->last_zone->NextPageInfo - 1;
	zone->allocPageCursor = zonep->last_zone->NextPageInfo - 1;
	zone->allocedpage = calc_alloced_page(zonep, last_right_pageid);
}

/**
 *	free_last_data_buf - free buf of last data
 *
 *	@zonep: operate object
 */
static void free_last_data_buf(ZoneManager *zonep)
{
	Nand_ContinueFree(zonep->last_data_buf);
}

/**
 *	deal_data - read last data and then deal with it
 *
 *	@zonep: operate object
 *	@pi: write pageinfo
 */
static int deal_last_pageinfo_data(ZoneManager *zonep, PageInfo *pi)
{
	PageList *pl = zonep->pl;
	Context *conptr = (Context *)(zonep->context);
	unsigned int *l1info = conptr->l1info->page;
	BuffListManager *blm = conptr->blm;

	if (!pl){
		BuffListManager_freeAllList((int)blm, (void **)&pl, sizeof(PageList));
		return 0;
	}

	if (zonep->last_data_read_error)
		l1info[pi->L1Index] = zonep->old_l1info;

	return 0;
}

/**
 *	alloc_zone - alloc memory of zone
 *
 *	@zonep: operate object
 */
static int deal_maxserial_zone(ZoneManager *zonep)
{
	int ret = 0;
	PageInfo pageinfo;
	PageInfo *pi = &pageinfo;

	if (zonep->useZone->usezone_count == 0) {
		ndprint(ZONEMANAGER_INFO,"no zone used. \n");
		return 0;
	}

	ret = alloc_pageinfo(pi, zonep);
	if (ret != 0) {
		ndprint(ZONEMANAGER_ERROR,"alloc_pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}

	ret = get_maxserial_zone(zonep);
	if (ret != 0) {
		ndprint(ZONEMANAGER_ERROR,"get_maxserial_zone error func %s line %d \n",
					__FUNCTION__,__LINE__);
		goto err;
	}

	ret = get_last_pageinfo(zonep, &pi);
	if (ret == -1) {
		ndprint(ZONEMANAGER_ERROR,"get_last_pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
		goto err;
	}
	else if (ret > 0) {
		if (ret == 1)
			ndprint(ZONEMANAGER_INFO,"first pageinfo deal OK in maximum serial number zone. \n");
		ret = 0;
		goto exit;
	}

	if (zonep->last_zone->NextPageInfo == 0) {
		ZoneManager_SetCurrentWriteZone(zonep->context,NULL);
		if (pi->zoneID == 0xffff) {
			ndprint(ZONEMANAGER_INFO,"Maximum serial number zone is already full.\n");
			goto exit;
		}
	}
	else
		get_current_write_zone_info(zonep);

	if (pi->zoneID != 0xffff) {
		if (zonep->last_data_read_error) {
			zonep->last_rzone_id = pi->zoneID;
			CacheManager_Init(zonep->context);
			start_follow_recycle(zonep);
		}
	}
	else
		ret = deal_last_pageinfo_data(zonep, pi);

exit:
	free_last_data_buf(zonep);
	ndprint(ZONEMANAGER_INFO,"Maximum serial number zone deal OK!!! \n");

err:
	free_pageinfo(pi,zonep);
	return ret;
}

Zone *ZoneManager_Get_Used_Zone(ZoneManager *zonep, unsigned short zoneid)
{
	return get_usedzone(zonep,zoneid);
}

/**
 *	ZoneManager_AllocZone - alloc a new zone
 *
 *	@context: global variable
 */
Zone* ZoneManager_AllocZone (int context)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;
	Zone *zoneptr = NULL;
	unsigned int i = 0;

	zoneptr = alloc_zone(zonep);
	if(zoneptr == NULL)
	{
		ndprint(ZONEMANAGER_ERROR,"alloc Zone error func %s line %d\n",
					__FUNCTION__,__LINE__);
		return NULL;
	}

	zoneptr->sigzoneinfo = alloc_free_node(zonep);
	if(zoneptr->sigzoneinfo == NULL)
	{
		ndprint(ZONEMANAGER_ERROR,"alloc free node error func %s line %d \n",
					__FUNCTION__,__LINE__);
		goto err;
	}

	zoneptr->top = zonep->sigzoneinfo;
	zoneptr->ZoneID = zoneptr->sigzoneinfo - zoneptr->top;
	//zoneptr->startblockID = BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,zoneptr->ZoneID);
	zoneptr->startblockID = zoneptr->ZoneID * BLOCKPERZONE(zonep->vnand);
	zoneptr->L1InfoLen = zonep->L1->len;
	zoneptr->L2InfoLen = zonep->l2infolen;
	zoneptr->L3InfoLen = zonep->l3infolen;
	zoneptr->L4InfoLen = zonep->l4infolen;
	zoneptr->validpage = zoneptr->sigzoneinfo->validpage;
	zoneptr->badblock = zoneptr->sigzoneinfo->badblock;

	for(i = 0 ; i < 4; i++)
	{
		if(zonep->memflag[i] == 0)
		break;
	}

	if(i == 4)
	{
		ndprint(ZONEMANAGER_INFO,"PANIC ERROR func %s line %d \n",
					__FUNCTION__,__LINE__);
		goto err;
	}

	zoneptr->memflag = i;
	zoneptr->mem0 = zonep->mem0 + i * zonep->vnand->BytePerPage;
	zoneptr->L1Info = (unsigned char *)(zonep->L1->page);
	zoneptr->vnand = zonep->vnand;
	zoneptr->context = (int)conptr;

	ndprint(ZONEMANAGER_INFO, "zoneID: %d, free count %d used count %d func %s \n",
		zoneptr->ZoneID, zonep->freeZone->count,zonep->useZone->usezone_count, __FUNCTION__);
	return zoneptr;
err:
	free_zone(zonep,zoneptr);
	return NULL;
}

/**
 *	ZoneManager_FreeZone - free full zone
 *
 *	@context: global variable
 *	@zone: which to free
 */
void ZoneManager_FreeZone (int context,Zone* zone )
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;

	zonep->memflag[zone->memflag] = 0;
	insert_used_node(zonep,zone->sigzoneinfo);

	ndprint(ZONEMANAGER_INFO, "zoneID: %d, free count %d used count %d func %s \n",
		zone->ZoneID, zonep->freeZone->count,zonep->useZone->usezone_count, __FUNCTION__);
	free_zone(zonep,zone);
}

/**
 *	ZoneManager_GetL1Info - get L1 info
 *
 *	@context: global variable
 */
L1Info* ZoneManager_GetL1Info (int context)
{
	Context *conptr = (Context *)context;
	return conptr->l1info;
}

/**
 *	ZoneManager_Init - Initialize operation
 *
 *	@context: global variable
 *
 *	include boot scan
 */
int ZoneManager_Init (int context )
{
	int ret = -1;
	Context *conptr  = (Context *)context;
#ifndef RECHECK_VALIDPAGE
	int pageperzone = conptr->vnand.PagePerBlock * BLOCKPERZONE(zone->vnand);
#endif
	ZoneManager *zonep = Nand_VirtualAlloc(sizeof(ZoneManager));
	if(zonep == NULL)
	{
		ndprint(ZONEMANAGER_ERROR,"alloc zonemanager fail func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}
	conptr->zonep = zonep;
	zonep->context = context;
	zonep->vnand = &conptr->vnand;
	/*
	  #ifdef TEST
	  memset(zonep->vnand->pt_badblock_info, 0xff, zonep->vnand->BytePerPage * BADBLOCKINFOSIZE);
	  #endif
	  zonep->badblockinfo = BadBlockInfo_Init((int *)zonep->vnand->pt_badblock_info,
	  zonep->vnand->startBlockID,zonep->vnand->TotalBlocks,BLOCKPERZONE(zonep->vnand));
	  zonep->pt_zonenum = BadBlockInfo_Get_ZoneCount(zonep->badblockinfo);
	*/
	zonep->pt_zonenum = (zonep->vnand->TotalBlocks + BLOCKPERZONE(zonep->vnand) - 1) / BLOCKPERZONE(zonep->vnand);
	if (zonep->pt_zonenum == 0) {
		ndprint(ZONEMANAGER_ERROR,"The partition has no zone that can be used!\n");
		return -1;
	}
#ifndef RECHECK_VALIDPAGE
	zonep->zonevalidinfo.zoneid = -1;
	zonep->zonevalidinfo.current_count = -1;
	zonep->zonevalidinfo.wpages = (Wpages *)Nand_ContinueAlloc(sizeof(Wpages) * pageperzone);
	if (zonep->zonevalidinfo.wpages == NULL)
	{
		ndprint(ZONEMANAGER_ERROR,"alloc zonemanager fail func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;

	}
#endif
	ret = alloc_zonemanager_memory(zonep,&conptr->vnand);
	if(ret != 0)
	{
		ndprint(ZONEMANAGER_ERROR,"alloc zonemanager memory error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return ret;
	}
	ret = init_free_node(zonep);
	if(ret != 0)
	{
		ndprint(ZONEMANAGER_ERROR,"init free node error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}
	ret = init_used_node(zonep);
	if(ret != 0)
	{
		ndprint(ZONEMANAGER_ERROR,"init used node error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}
	ret = scan_page_info(zonep);
	if (ret != 0)
	{
		ndprint(ZONEMANAGER_ERROR,"scan_page_info error func %s line %d\n",
			__FUNCTION__, __LINE__);
		return ret;
	}
	ret = error_handle(zonep);
	if (ret != 0)
	{
		ndprint(ZONEMANAGER_ERROR,"error_handle error func %s line %d\n",
			__FUNCTION__, __LINE__);
		return ret;
	}

	ndprint(ZONEMANAGER_INFO, "free count %d used count %d maxserial zoneid %d maxserialnum %d \n",
		zonep->freeZone->count, zonep->useZone->usezone_count, zonep->last_zone_id, zonep->maxserial);

	if (zonep->page2_error_dealt)
		return 0;
	ret = deal_maxserial_zone(zonep);
	if(ret != 0)
	{
		ndprint(ZONEMANAGER_ERROR,"deal_maxserial_zone error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}

	return 0;
}

/**
 *	ZoneManager_DeInit - Deinit operation
 *
 *	@context: global variable
 */
void ZoneManager_DeInit (int context )
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;
	//BadBlockInfo_Deinit(zonep->badblockinfo);
	Nand_ContinueFree(zonep->zonevalidinfo.wpages);
	deinit_free_node(zonep);
	deinit_used_node(zonep);
	free_zonemanager_memory(zonep);
	Nand_VirtualFree(zonep);
}

/**
 *	ZoneManager_RecyclezoneID - Get recycle zoneID
 *
 *	@context: global variable
 *	@lifetime: condition of lifetime need to find
 */
unsigned short ZoneManager_RecyclezoneID(int context,unsigned int lifetime)
{
	int flag = 0;
	int count = 0;
	unsigned int index ;
	SigZoneInfo *sigp = NULL;
	SigZoneInfo *sigpt = NULL;
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;
	short vaildpage =  conptr->vnand.PagePerBlock * BLOCKPERZONE(zonep->vnand);

	index = Hash_FindFirstLessLifeTime(zonep->useZone,lifetime,&sigp);
	if(index == -1){
		ndprint(ZONEMANAGER_ERROR,"Can't find the lifetime. Please give a new larger lifetime!! \n");
		return -1;
	}

	do {
		if(vaildpage > (short)sigp->validpage)
		{
			vaildpage = sigp->validpage;
			sigpt = sigp;
			if (!vaildpage)
				break;
		}
		else if(vaildpage == sigp->validpage)
			flag++;

		count++;

		index = Hash_FindNextLessLifeTime(zonep->useZone,index,&sigp);
	}
	while(index != -1);

	if (flag >= count - 1)
		Hash_FindFirstLessLifeTime(zonep->useZone,zonep->useZone->minlifetime + 1,&sigpt);
	if (sigpt >= zonep->sigzoneinfo + zonep->pt_zonenum || sigpt < zonep->sigzoneinfo){
		ndprint(ZONEMANAGER_ERROR,"%s %d sigpt:%p sigzoneinfo:%p zonenum:%d \n",__func__,__LINE__,sigpt,zonep->sigzoneinfo,zonep->pt_zonenum);
	}

	return (sigpt - zonep->sigzoneinfo);
}

/**
 *	ZoneManager_ForceRecyclezoneID - Get force recycle zoneID
 *
 *	@context: global variable
 *	@lifetime: condition of lifetime need to find
 */
unsigned short ZoneManager_ForceRecyclezoneID(int context,unsigned int lifetime)
{
	unsigned int index ;
	SigZoneInfo *sigp = NULL;
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;

	index = Hash_FindFirstLessLifeTime(zonep->useZone,lifetime,&sigp);
	if(index == -1)
		return -1;
	return sigp - zonep->sigzoneinfo;
}

/**
 *	L1Info_get - Get L1 info
 *
 *	@context: global variable
 *	@sectorID: id of sector
 */
unsigned int L1Info_get(int context ,unsigned int sectorID)
{
	Context *conptr = (Context *)context;
	unsigned int index = sectorID / (conptr->vnand.PagePerBlock *
						conptr->vnand.TotalBlocks *(conptr->vnand.BytePerPage/SECTOR_SIZE));
	return conptr->l1info->page[index];
}

/**
 *	L1Info_set - Set L1 info
 *
 *	@context: global variable
 *	@PageID: id of page
 */
void L1Info_set(int context,unsigned int sectorID,unsigned int PageID)
{
	Context *conptr = (Context *)context;
	unsigned int index = sectorID / (conptr->vnand.PagePerBlock *
			conptr->vnand.TotalBlocks *(conptr->vnand.BytePerPage/SECTOR_SIZE));
	NandMutex_Lock(&conptr->l1info->mutex);
	conptr->l1info->page[index] = PageID;
	NandMutex_Unlock(&conptr->l1info->mutex);
}

/**
 *	ZoneManager_AllocRecyclezone - Get a recycle zone
 *
 *	@context: global variable
 *	@zoneID: id of zone
 */
Zone *ZoneManager_AllocRecyclezone(int context ,unsigned short ZoneID)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;

	Zone *zoneptr = NULL;
	unsigned int i = 0;
	int ret = 0;

	zoneptr = alloc_zone(zonep);
	if(zoneptr == NULL)
	{
		ndprint(ZONEMANAGER_ERROR,"alloc Zone error func %s line %d\n",
					__FUNCTION__,__LINE__);
		return NULL;
	}
	if (ZoneID < 0 || ZoneID >= zonep->pt_zonenum)
		ndprint(ZONEMANAGER_ERROR,"%s %d zoneid: %d ERROR! total zonenum is %d\n",__func__,__LINE__,ZoneID,zonep->pt_zonenum);
	zoneptr->sigzoneinfo = zonep->sigzoneinfo + ZoneID;
	if (zoneptr->sigzoneinfo->pre_zoneid != 0xffff)
		zoneptr->prevzone = zonep->sigzoneinfo + zoneptr->sigzoneinfo->pre_zoneid;
	else
		zoneptr->prevzone = NULL;
	if (zoneptr->sigzoneinfo->next_zoneid != 0xffff)
		zoneptr->nextzone = zonep->sigzoneinfo + zoneptr->sigzoneinfo->next_zoneid;
	else
		zoneptr->nextzone = NULL;
	//zoneptr->startblockID = BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,ZoneID);
	zoneptr->startblockID = ZoneID * BLOCKPERZONE(zonep->vnand);
	zoneptr->top = zonep->sigzoneinfo;
	zoneptr->L1InfoLen = zonep->L1->len;
	zoneptr->L2InfoLen = zonep->l2infolen;
	zoneptr->L3InfoLen = zonep->l3infolen;
	zoneptr->L4InfoLen = zonep->l4infolen;

	for(i = 4 ; i < 8 ; i++)
	{
		if(zonep->memflag[i] == 0)
			break;
	}

	if(i == 8)
	{
		ndprint(ZONEMANAGER_ERROR,"PANIC ERROR func %s line %d \n",
					__FUNCTION__,__LINE__);
		goto err;
	}

	ret = delete_used_node(zonep,ZoneID);
	if(ret != 0)
	{
		ndprint(ZONEMANAGER_ERROR,"delete used node error ZoneID = %d func %s line %d\n",
					ZoneID, __FUNCTION__,__LINE__);
		goto err;
	}
	zoneptr->memflag = i;
	zoneptr->ZoneID = ZoneID;
	zoneptr->badblock = zoneptr->sigzoneinfo->badblock;
	zoneptr->validpage = zoneptr->sigzoneinfo->validpage;
	zoneptr->mem0 = zonep->mem0 + i * zonep->vnand->BytePerPage;
	zoneptr->L1Info = (unsigned char *)(zonep->L1->page);
	zoneptr->vnand = zonep->vnand;
	zoneptr->context = context;
	zoneptr->sumpage = BLOCKPERZONE(zonep->vnand) * zonep->vnand->PagePerBlock;

	ndprint(ZONEMANAGER_INFO, "zoneID: %d, free count %d used count %d func %s \n",
		ZoneID, zonep->freeZone->count,zonep->useZone->usezone_count, __FUNCTION__);
	return zoneptr;
err:
	free_zone(zonep,zoneptr);
	return NULL;
}

/**
 *	ZoneManager_FreeRecyclezone - free recycle zone
 *
 *	@context: global variable
 *	@zone: which to free
 */
void ZoneManager_FreeRecyclezone(int context,Zone *zone)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;

	zonep->memflag[zone->memflag] = 0;
	insert_free_node(zonep, zone->ZoneID);

	ndprint(ZONEMANAGER_INFO, "zoneID: %d, free count %d used count %d func %s \n",
		zone->ZoneID, zonep->freeZone->count,zonep->useZone->usezone_count, __FUNCTION__);

	free_zone(zonep,zone);
}

/**
 *	ZoneManager_Getminlifetime - get minimum lifetime of usezone
 *
 *	@context: global variable
 */
unsigned int ZoneManager_Getminlifetime(int context)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;

	return Hash_getminlifetime(zonep->useZone);
}

/**
 *	ZoneManager_Getmaxlifetime - get maximum lifetime of usezone
 *
 *	@context: global variable
 */
unsigned int ZoneManager_Getmaxlifetime(int context)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;
	return Hash_getmaxlifetime(zonep->useZone);
}

/**
 *	ZoneManager_Getusedcount - get count of usezone
 *
 *	@context: global variable
 */
unsigned int ZoneManager_Getusedcount(int context)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;
	return Hash_getcount(zonep->useZone);
}

/**
 *	ZoneManager_Getusedcount - get count of freezone
 *
 *	@context: global variable
 */
unsigned int ZoneManager_Getfreecount(int context)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;
	return HashNode_getcount(zonep->freeZone);
}

unsigned int ZoneManager_Getptzonenum(int context)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;
	return zonep->pt_zonenum;
}
/**
 *	ZoneManager_SetCurrentWriteZone - set current write zone
 *
 *	@context: global variable
 *	@zone: which to set
 */
void  ZoneManager_SetCurrentWriteZone(int context,Zone *zone)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;
	zonep->write_zone = zone;
}

/**
 *	ZoneManager_GetCurrentWriteZone - get current write zone
 *
 *	@context: global variable
 */
Zone *ZoneManager_GetCurrentWriteZone(int context)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;
	return zonep->write_zone;
}

/**
 *	ZoneManager_SetAheadZone - set ahead zone
 *
 *	@context: global variable
 *	@zone: which to set
 */
unsigned int ZoneManager_SetAheadZone(int context,Zone *zone)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;
	int i = 0;
	unsigned int count = 0;
	for(i = 0 ; i < 4 ; i++)
	{
		if(zonep->aheadflag[i] == 0)
		{
			zonep->ahead_zone[i] = zone;
			zonep->aheadflag[i] = 1;
			break;
		}
	}
	for(i = 0 ; i < 4 ; i++)
	{
		if(zonep->aheadflag[i] == 1)
		{
			count++;
		}
	}
	return count;
}

/**
 *	ZoneManager_GetAheadZone - get ahead zone
 *
 *	@context: global variable
 *	@zone: return ahead zone to caller
 */
unsigned int ZoneManager_GetAheadZone(int context,Zone **zone)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;
	int i = 0;
	unsigned int count = 0;

	*zone = zonep->ahead_zone[0];

	memcpy(&zonep->ahead_zone[0],&zonep->ahead_zone[1],3 * sizeof(unsigned int));
	memcpy(&zonep->aheadflag[0],&zonep->aheadflag[1],3 * sizeof(unsigned int));
	zonep->aheadflag[3] = 0;
	zonep->ahead_zone[3] = NULL;

	for(i= 0 ; i < 4 ; i++)
	{
		if(zonep->aheadflag[i] == 0)
		{
			count++;
		}
	}

	return count;
}

/**
 *	ZoneManager_GetAheadCount - get count of ahead zone
 *
 *	@context: global variable
 */
unsigned int ZoneManager_GetAheadCount(int context)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;
	int i = 0 ;
	unsigned int count = 0;

	for(i = 0 ; i < 4 ; i++)
	{
		if(zonep->aheadflag[i] == 1)
			count++;
	}

	return count ;
}

/**
 *	ZoneManager_SetPrevZone - set previous zone
 *
 *	@context: global variable
 *	@zone: which to set
 */
void ZoneManager_SetPrevZone(int context,Zone *zone)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;
	zonep->prev = zone->sigzoneinfo;
}

/**
 *	ZoneManager_GetPrevZone - get previous zone
 *
 *	@context: global variable
 */
SigZoneInfo *ZoneManager_GetPrevZone(int context)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;
	return zonep->prev;
}

/**
 *	ZoneManager_GetNextZone - get next zone
 *
 *	@context: global variable
 */
SigZoneInfo *ZoneManager_GetNextZone(int context)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;

	if(zonep->ahead_zone[0] != NULL)
		zonep->next = (zonep->ahead_zone[0])->sigzoneinfo;
	else
		zonep->next = NULL;

	return zonep->next;
}
int ZoneManager_convertPageToZone(int context,unsigned int pageid){
	Context *conptr = (Context *)context;
	//ZoneManager *zonep = conptr->zonep;
	int blockid = pageid / conptr->vnand.PagePerBlock;
	if (blockid < 0 || blockid >= conptr->vnand.TotalBlocks) {
		ndprint(ZONEMANAGER_ERROR, "ERROR: blockid = %d vnand.TotalBlocks = %d func: %s line: %d \n",
			blockid, conptr->vnand.TotalBlocks, __FUNCTION__, __LINE__);
		return -1;
	}

	//return BadBlockInfo_ConvertBlockToZoneID(zonep->badblockinfo,blockid);
	return blockid / BLOCKPERZONE(zonep->vnand);
}
void debug_zonemanagerinfo(int context)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonep = conptr->zonep;

	ndprint(ZONEMANAGER_DEBUG, "zonep->l2len %d\n",zonep->l2infolen);
	ndprint(ZONEMANAGER_DEBUG, "zonep->l3len %d \n",zonep->l3infolen);
	ndprint(ZONEMANAGER_DEBUG, "zonep->l4len %d \n",zonep->l4infolen);
	ndprint(ZONEMANAGER_DEBUG, "zonep->count %d \n",zonep->zoneIDcount);
	ndprint(ZONEMANAGER_DEBUG, "zonep->useZone %p \n",zonep->useZone);
}

