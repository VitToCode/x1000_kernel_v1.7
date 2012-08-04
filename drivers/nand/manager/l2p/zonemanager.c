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
#include "zonemanager.h"
#include "nandsigzoneinfo.h"
#include "nandzoneinfo.h"
#include "taskmanager.h"
#include "nmbitops.h"
#include "nandpageinfo.h"
#include "cachemanager.h"
#include "errhandle.h"

#define BLOCKPERZONE(context)   	8
#define FIRSTPAGEINFO(context)	   	3
#define ZONEPAGE1INFO(context)      1
#define ZONEPAGE2INFO(context)      2
#define ZONEMEMSIZE(vnand)      (vnand->BytePerPage * 4)

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
 *	calc_L2L3_len - Calculate length of L2 and L3
 *
 *	@zonep: operate object
 */
static void calc_L2L3_len(ZoneManager *zonep)
{
	unsigned int l2l3len = 0;
	unsigned int temp = 0;
	unsigned int value = 1;
	unsigned int i = 0;
	unsigned int sectornum;
	unsigned int totalsectornum;
	unsigned int l1_secnum;
	unsigned int l4_secnum;

	sectornum = zonep->vnand->BytePerPage / SECTOR_SIZE;
	totalsectornum = zonep->vnand->PagePerBlock * zonep->vnand->TotalBlocks * sectornum;
	l2l3len = (zonep->vnand->BytePerPage - (1024 + sizeof(NandPageInfo))) / sizeof(unsigned int);

	l1_secnum = zonep->vnand->BytePerPage / sizeof(unsigned int);
	l4_secnum = 1024 / sizeof(unsigned int);
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
	{
		zonep->l2infolen = 0;
	}
	else
	{
		zonep->l2infolen = value * sizeof(unsigned int);
	}

	zonep->l3infolen = (temp+value-1) / value * sizeof(unsigned int);

	/* if the capacity of nand is too small, l4infolen should not be 1024 */
	if (zonep->l3infolen || zonep->l2infolen)
		zonep->l4infolen = 1024;
	else
		zonep->l4infolen = zonep->vnand->TotalBlocks * zonep->vnand->PagePerBlock * 
			zonep->vnand->BytePerPage / (zonep->vnand->BytePerPage / sizeof(unsigned int)) / SECTOR_SIZE * sizeof(unsigned int);
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

	zonep->vnand = vnand;
	zonenum = zonep->pt_zonenum;

	zonep->zoneID = (unsigned short *)Nand_ContinueAlloc(zonenum * sizeof(unsigned short));
	if(zonep->zoneID == NULL)
	{
		ndprint(ZONEMANAGER_ERROR,"alloc memory fail func %s line %d \n",
			__FUNCTION__,__LINE__);
		
		return -1;
	}

	zonep->sigzoneinfo = (SigZoneInfo *)Nand_ContinueAlloc(zonenum * sizeof(SigZoneInfo));
	ndprint(ZONEMANAGER_INFO, "zonep->sigzoneinfo = %p\n",zonep->sigzoneinfo);
	if(zonep->sigzoneinfo == NULL)
	{
		ndprint(ZONEMANAGER_ERROR,"alloc memory fail func %s line %d \n",
			__FUNCTION__,__LINE__);
	
		ret = -1;
		goto err0;
	}

	(zonep->L1).page = (unsigned int *)Nand_ContinueAlloc(vnand->BytePerPage);
	if((zonep->L1).page == NULL)
	{
		ndprint(ZONEMANAGER_ERROR,"alloc memoey fail func %s line %d \n",
			__FUNCTION__,__LINE__);	

		ret = -1;
		goto err1; 	
	}

	zonep->mem0 = (unsigned char *)Nand_ContinueAlloc(2 * ZONEMEMSIZE(vnand));
	if(zonep->mem0 == NULL)
	{
		ndprint(ZONEMANAGER_ERROR,"alloc memoey fail func %s line %d \n",
			__FUNCTION__,__LINE__);	
		
		ret = -1;
		goto err2;
	}

	zonep->startblockID = (int *)Nand_ContinueAlloc(zonenum * sizeof(int));
	if(zonep->startblockID == NULL)
	{
		ndprint(ZONEMANAGER_ERROR,"alloc memoey fail func %s line %d \n",
			__FUNCTION__,__LINE__);	
		
		ret = -1;
		goto err3;
	}
	
	memset((void*)(zonep->sigzoneinfo) , 0xff, sizeof(SigZoneInfo) * zonenum);
	memset(zonep->memflag,0x0,sizeof(zonep->memflag));
	(zonep->L1).len = vnand->BytePerPage;
	zonep->last_zone = NULL;
	zonep->write_zone = NULL;
	zonep->prev = NULL;
	zonep->next = NULL;
	zonep->last_data_buf = NULL;
	zonep->last_pi = NULL;
	zonep->pl = NULL;
	zonep->need_write_last_data = 0;
	for(i = 0; i < 4 ; i++) {
		zonep->aheadflag[i] = 0;
		zonep->ahead_zone[i] = NULL;
	}
	
	InitNandMutex(&((zonep->L1).mutex));
	InitNandMutex(&zonep->HashMutex);

	calc_L2L3_len(zonep);
	zonep->zonemem =  ZoneMemory_Init(sizeof(Zone)); 
	zonep->zoneIDcount = zonenum;
	return 0;
err3:
	Nand_ContinueFree(zonep->mem0);
err2:
	Nand_ContinueFree((zonep->L1).page);
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
	Nand_ContinueFree(zonep->zoneID);
	Nand_ContinueFree(zonep->sigzoneinfo);
	Nand_ContinueFree(zonep->mem0);
	Nand_ContinueFree(zonep->startblockID);
	DeinitNandMutex(&((zonep->L1).mutex));
	DeinitNandMutex(&zonep->HashMutex);
	Nand_ContinueFree((zonep->L1).page);
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
	unsigned int startblockno = zonep->startblockID[zoneid];

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
 *	start_read_page0_error_handle - start to deal with the error when read page0
 *
 *	@zonep: operate object
 *	@zoneid: error zoneid
 */
static void start_read_page0_error_handle(ZoneManager *zonep, unsigned short zoneid)
{
	Message read_page0_error_msg;
	Context *conptr = (Context *)(zonep->context);
	ErrInfo errinfo;
	errinfo.context = zonep->context;
	errinfo.err_zoneid = zoneid;

	read_page0_error_msg.msgid = READ_PAGE0_ERROR_ID;
	read_page0_error_msg.prio = READ_PAGE0_ERROR_PRIO;
	read_page0_error_msg.data = (int)&errinfo;

	Message_Post(conptr->thandle, &read_page0_error_msg, NOWAIT);
}

/** 
 *	start_read_page1_error_handle - start to deal with the error when read page1
 *
 *	@zonep: operate object
 *	@zoneid: error zoneid
 */
static void start_read_page1_error_handle(ZoneManager *zonep, unsigned short zoneid)
{
	Message read_page1_error_msg;
	Context *conptr = (Context *)(zonep->context);
	ErrInfo errinfo;
	errinfo.context = zonep->context;
	errinfo.err_zoneid = zoneid;

	read_page1_error_msg.msgid = READ_PAGE1_ERROR_ID;
	read_page1_error_msg.prio = READ_PAGE1_ERROR_PRIO;
	read_page1_error_msg.data = (int)&errinfo;

	Message_Post(conptr->thandle, &read_page1_error_msg, NOWAIT);
}

/** 
 *	start_read_page2_error_handle - start to deal with the error when read page2
 *
 *	@zonep: operate object
 */
static void start_read_page2_error_handle(ZoneManager *zonep)
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
	Message_Recieve(conptr->thandle, msghandle);
}
/** 
 *	unpackage_page0_info - unpake information of page0
 *
 *	@zonep: operate object
 *	@zoneid: id of zone
 */
static int unpackage_page0_info(ZoneManager *zonep,unsigned short zoneid)
{
	unsigned int buflen = sizeof(NandSigZoneInfo);
	unsigned int i = 0;
	unsigned char *buf = zonep->mem0;
	NandSigZoneInfo *nandsigzoneinfo = (NandSigZoneInfo *)(zonep->mem0);
	SigZoneInfo *sigzoneinfo = zonep->sigzoneinfo + zoneid;

	while(((buf[i++] & 0xff) == 0xff) && i < buflen);	

	if(i == buflen)
	{
		memset(sigzoneinfo,0x0,sizeof(SigZoneInfo));
		return -1;
	}

	sigzoneinfo->badblock = nandsigzoneinfo->badblock;
	sigzoneinfo->lifetime = nandsigzoneinfo->lifetime;
	sigzoneinfo->validpage = zonep->vnand->PagePerBlock * BLOCKPERZONE(zonep->vnand) - 3;
	
	return 0;
}

/** 
 *	find_maxserialnumber 
 *
 *	@zonep: operate object
 *	@maxserial: maximum serial number
 *	@recordzone: record zone
 */
static  int find_maxserialnumber(ZoneManager *zonep,

			unsigned int *maxserial,unsigned short *recordzone)
{
	unsigned int buflen = sizeof(NandZoneInfo);
	unsigned int i = 0;						
	unsigned short zoneid;
	unsigned int max = 0;
	unsigned char *buf = zonep->mem0;
	NandZoneInfo *nandzoneinfo = (NandZoneInfo *)(zonep->mem0);
	SigZoneInfo *oldsigp = NULL;

	while(((buf[i++] & 0xff) == 0xff) && i < buflen);

	if(i == buflen)
		return -1;

	//update localzone info in zonep->sigzoneinfo
	zoneid = nandzoneinfo->localZone.ZoneID;
	if (zoneid >= 0 && zoneid < zonep->pt_zonenum) {
		oldsigp = (SigZoneInfo *)(zonep->sigzoneinfo + zoneid);
		CONV_ZI_SZ(&nandzoneinfo->localZone,oldsigp);
	}

	//update prevzone info in zonep->sigzoneinfo
	zoneid = nandzoneinfo->preZone.ZoneID;
	if (zoneid >= 0 && zoneid < zonep->pt_zonenum) {
		oldsigp = (SigZoneInfo *)(zonep->sigzoneinfo + zoneid);
		if (oldsigp->lifetime != 0xffffffff
			&& oldsigp->lifetime < nandzoneinfo->preZone.lifetime)
			CONV_ZI_SZ(&nandzoneinfo->preZone,oldsigp);
	}

	//update nextzone info in zonep->sigzoneinfo
	zoneid = nandzoneinfo->nextZone.ZoneID;
	if (zoneid >= 0 && zoneid < zonep->pt_zonenum) {
		oldsigp = (SigZoneInfo *)(zonep->sigzoneinfo + zoneid);
		if (oldsigp->lifetime != 0xffffffff
			&& oldsigp->lifetime < nandzoneinfo->nextZone.lifetime)
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

/** 
 *	scan_sigzoneinfo_fill_node - scan sigzoneinfo and fill it to hashtable
 *
 *	@zonep: operate object
 *	@pl: pagelist
 */
static void scan_sigzoneinfo_fill_node(ZoneManager *zonep,PageList *pl)
{
	unsigned int i = 0;
	SigZoneInfo *sigp = NULL;
	int ret = -1;
	unsigned int zonenum = zonep->pt_zonenum;

	for(i = 0 ; i < zonenum ; i++)
	{
		sigp = (SigZoneInfo *)(zonep->sigzoneinfo + i);

		if(sigp->lifetime == 0xffffffff)
		{
			read_zone_page0(zonep,i,pl);
			ret = pl->retVal;
			if (!ISNOWRITE(ret)) {
				if(ISERROR(ret))
				{
					start_read_page0_error_handle(zonep, i);		
				}
				unpackage_page0_info(zonep,i);
			}
			ret = insert_free_node(zonep,i);
			if(ret != 0)
			{
				ndprint(ZONEMANAGER_ERROR,"insert free node error func %s line %d \n",

							__FUNCTION__,__LINE__);
				/*todo*/			
			}
		}
		else
		{
			ret = insert_used_node(zonep,sigp);
			if(ret != 0)
			{
				ndprint(ZONEMANAGER_ERROR,"insert used node error func %s line %d \n",

							__FUNCTION__,__LINE__);
				/*todo*/
			}	
		}
	}
}

/** 
 *	get_pt_badblock_count - get bad block number of partition
 *
 *	@vnand: virtual nand
 */
static int get_pt_badblock_count(VNandInfo *vnand)
{
	int i;
	int count = 0;

	for (i = 0; i < vnand->BytePerPage >> 2; i++) {
		if (vnand->pt_badblock_info[i] >= vnand->startBlockID && 
			vnand->pt_badblock_info[i] < (vnand->startBlockID+vnand->TotalBlocks)){

			if (vnand->pt_badblock_info[i] == 0xffffffff)
				break;

			count++;
		}
	}

	return count;
}

/** 
 *	get_pt_zone_num - get zone count of partition
 *
 *	@vnand: virtual nand
 */
static int get_pt_zone_num(VNandInfo *vnand)
{
	int blocknum;
	blocknum = vnand->TotalBlocks - get_pt_badblock_count(vnand);
	if (blocknum == 0){
		ndprint(ZONEMANAGER_ERROR,"The partition's blocks are all bad blocks. This partition can't be used!!\n");
		return 0;
	}
	return (blocknum - 1) / BLOCKPERZONE(vnand) + 1;
}

/** 
 *	zonemanager_is_badblock - whether block is bad or not
 *
 *	@zonep: operate object
 *	@blockno: number of block
 */
static int zonemanager_is_badblock(ZoneManager *zonep, unsigned int blockno)
{
	int i;

	for (i = 0; i < zonep->vnand->BytePerPage >> 2; i++) {
		if (blockno != 0xffffffff && zonep->vnand->pt_badblock_info[i] == blockno)
			return 1;
	}

	return 0;
}

/** 
 *	get_first_startblockid - get first start blockid of the first zone of the partiton
 *
 *	@zonep: operate object
 */
static int get_first_startblockid(ZoneManager *zonep)
{
	int i;
	VNandInfo *vnand = zonep->vnand;

	for (i = vnand->startBlockID; i < vnand->TotalBlocks + vnand->startBlockID; i++) {
		if(!zonemanager_is_badblock(zonep, i))
			break;
	}

	return i;
}

/** 
 *	get_next_startblockid - get next start blockid of the partiton
 *
 *	@zonep: operate object
 *	@prev_startblockid: start blockid of previous zone
 */
static int get_next_startblockid(ZoneManager *zonep, int prev_startblockid)
{
	int i;
	int count = 0;
	VNandInfo *vnand = zonep->vnand;
	
	for (i = prev_startblockid + 1; i < vnand->TotalBlocks + vnand->startBlockID; i++) {
		if(zonemanager_is_badblock(zonep, i))
			continue;
		else
			count++;
		
		if (count == BLOCKPERZONE(vnand))
			break;
	}

	return i;
}

/** 
 *	fill_zone_start_blockid - fill start block id of each zone
 *
 *	@zonep: operate object
 */
static void fill_zone_start_blockid(ZoneManager *zonep)
{
	int i;
	unsigned int zonenum = zonep->pt_zonenum;

	for (i = 0; i < zonenum; i++) {
		if (i == 0)
			zonep->startblockID[i] = get_first_startblockid(zonep);
		else
			zonep->startblockID[i] = get_next_startblockid(zonep, zonep->startblockID[i - 1]);
	}
}

/** 
 *	scan_page_info - scan page info
 *
 *	@zonep: operate object
 */
static void scan_page_info(ZoneManager *zonep)
{
	PageList pagelist;
	PageList *plt = &pagelist;
	unsigned int max_serial = 0;
	unsigned short max_zoneid = 0;
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
		if (ISNOWRITE(ret))
			continue;
		else if (ISERROR(ret))
			start_read_page1_error_handle(zonep, i);
		else
			find_maxserialnumber(zonep,&max_serial,&max_zoneid);
	}
	
	zonep->maxserial = max_serial;
	zonep->last_zone_id = max_zoneid;

	read_zone_page2(zonep,max_zoneid,plt);
	ret = plt->retVal;
	if(ISERROR(ret) && !ISNOWRITE(ret))
	{
		start_read_page2_error_handle(zonep);			
	}

	memcpy((zonep->L1).page,zonep->mem0,zonep->vnand->BytePerPage);
	scan_sigzoneinfo_fill_node(zonep,plt);
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
	pageinfo->L1Info = (unsigned char *)(zonep->L1.page);
	
	if (zonep->l2infolen) {
		pageinfo->L2Info = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * zonep->l2infolen);
		if (!(pageinfo->L2Info)) {
			ndprint(ZONEMAAGER_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
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
	zoneptr->badblock = (zonep->sigzoneinfo + zoneid)->badblock;
	zoneptr->validpage= (zonep->sigzoneinfo + zoneid)->validpage;
	zoneptr->startblockID = zonep->startblockID[zoneid];
	zoneptr->top = zonep->sigzoneinfo;
	zoneptr->L1InfoLen = zonep->vnand->BytePerPage;
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
	zoneptr->maxserial = zonep->maxserial;
	zoneptr->L1Info = (unsigned char *)(zonep->L1).page;
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
	zoneptr->pageCursor = badblockcount * zoneptr->vnand->PagePerBlock + ZONEPAGE1INFO(zoneptr->vnand);
	zoneptr->allocPageCursor = zoneptr->pageCursor + 1;
	zoneptr->allocedpage = 3;

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
 *	get_last_pageinfo - get last pageinfo of maximum serial number zone
 *
 *	@zonep: operate object
 *	@pi: which need to filled
 */
static int get_last_pageinfo(ZoneManager *zonep, PageInfo **pi)
{
	int ret = 0;
	PageInfo pageinfo;
	Zone *zone = zonep->last_zone;
	PageInfo *prev_pi = &pageinfo;

	ret = alloc_pageinfo(prev_pi, zonep);
	if (ret != 0) {
		ndprint(ZONEMANAGER_ERROR,"alloc_pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}

	ret = Zone_FindFirstPageInfo(zone,prev_pi);
	if (ISERROR(ret) || zone->NextPageInfo == 0xffff)
		return -2;			

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
			copy_pageinfo(zonep, *pi, prev_pi);
			break;			
		}
		else
			copy_pageinfo(zonep, prev_pi, *pi);
	}

	free_pageinfo(prev_pi, zonep);
	zonep->last_pi = *pi;

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
		
	return pageid >= zonep->startblockID[zoneid] * ppb
		&& pageid < zonep->startblockID[zoneid + 1] * ppb;
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
	PageList *pl_top;
	PageList *pl_node;
	unsigned int spp = zonep->vnand->BytePerPage / SECTOR_SIZE;
	BuffListManager *blm = ((Context *)(zonep->context))->blm;
	unsigned short l4infolen = pi->L4InfoLen;
	unsigned int *l4info = (unsigned int *)(pi->L4Info);
	l4count = l4infolen >> 2;
	
	pl_top = (PageList *)BuffListManager_getTopNode((int)blm, sizeof(PageList));
	
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
	
		pl_node = (PageList *)BuffListManager_getNextNode((int)blm, (void *)pl_top, sizeof(PageList));
		pl_node->startPageID = tmp0;
		pl_node->OffsetBytes = l4info[i] % spp;
		pl_node->Bytes = k * SECTOR_SIZE;
		pl_node->retVal = 0;
		pl_node->pData = NULL;

		pagecount++;
	}

	zonep->pagecount = pagecount;
	BuffListManager_freeList((int)blm, (void *)&pl_top, (void *)pl_top, sizeof(PageList));

	ret = alloc_last_data_buf(zonep,pagecount);
	if (ret != 0) {
		ndprint(ZONEMANAGER_ERROR,"alloc_last_data_buf error func %s line %d \n",
					__FUNCTION__,__LINE__);
		return NULL;
	}

	pagecount = 0;
	singlelist_for_each(pos,&pl_top->head) {
		pl_node = singlelist_entry(pos,PageList,head);
		pl_node->pData = zonep->last_data_buf + (pagecount++) * zonep->vnand->BytePerPage;
	}

	return pl_top;
}

/** 
 *	analy_pagelist - analy pagelist to get pagecount
 *
 *	@pl: which need to analy
 */
static int analy_pagelist(PageList *pl)
{
	struct singlelist *pos;
	PageList *pl_nod;
	unsigned int last_right_pageid = 0;

	singlelist_for_each(pos,&pl->head) {
		pl_nod = singlelist_entry(pos,PageList,head);
		if (pl_nod->retVal >= 0)
			last_right_pageid = pl_nod->startPageID;
	}

	return last_right_pageid;
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

	return last_right_pageid - badblockcount * zonep->vnand->PagePerBlock - start_pageid + 1;
}

/** 
 *	get_current_write_zone_info - get maximun serial number zone info
 *
 *	@zonep: operate object
 */
static void get_current_write_zone_info(ZoneManager *zonep)
{
	int ret = 0;
	unsigned int last_right_pageid;
	Zone *zone = zonep->last_zone;
	
	zonep->pl = Create_read_pagelist(zonep, zonep->last_pi);
	if (!(zonep->pl) && zonep->pagecount == 0)
		last_right_pageid = zonep->last_pi->PageID;
	else {
		ret = Zone_RawMultiReadPage(zonep->last_zone, zonep->pl);
		if (ret != 0)
			zonep->need_write_last_data = 1;
		
		last_right_pageid = analy_pagelist(zonep->pl);
	}

	zone->pageCursor = last_right_pageid;
	zone->allocPageCursor = last_right_pageid;
	zone->allocedpage = calc_alloced_page(zonep, last_right_pageid);
}

/** 
 *	start_recycle - start boot recycle
 *
 *	@zonep: operate object
 */
static void start_recycle(ZoneManager *zonep)
{
#ifndef NO_ERROR
	Message boot_recycle_msg;
	int msghandle;
	Context *conptr = (Context *)(zonep->context);
	ndprint(ZONEMANAGER_INFO,"WARNNING: bootprepare find a error,Deal with it!\n");

	boot_recycle_msg.msgid = BOOT_RECYCLE_ID;
	boot_recycle_msg.prio = BOOT_RECYCLE_PRIO;
	boot_recycle_msg.data = zonep->context;

	msghandle = Message_Post(conptr->thandle, &boot_recycle_msg, WAIT);
	Message_Recieve(conptr->thandle, msghandle);
#endif
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
 *	update_l1l2l3l4_and_pagelist - update l1l2l3l4info and pagelist
 *
 *	@zonep: operate object
 *	@zone: which zone pagelist is on
 *	@pi: write pageinfo
 *	@pl: write pagelist
 */
static PageList *update_l1l2l3l4_and_pagelist(ZoneManager *zonep, Zone *zone, PageInfo *pi, PageList *pl)
{
	int i;
	PageList *pl_node = pl;
	PageList *pl_pre = pl;
	unsigned int spp = zonep->vnand->BytePerPage / SECTOR_SIZE;
	unsigned short l4infolen = pi->L4InfoLen;
	unsigned int *l1info = (unsigned int *)(pi->L1Info);
	unsigned int *l2info = (unsigned int *)(pi->L2Info);
	unsigned int *l3info = (unsigned int *)(pi->L3Info);
	unsigned int *l4info = (unsigned int *)(pi->L4Info);

	pi->PageID = Zone_AllocNextPage(zone);
	l1info[pi->L1Index] = pi->PageID;
	
	for (i = 0; i < l4infolen >> 2; i++) {
		if (l4info[i] / spp > pi->PageID && page_in_current_zone(zonep,zone->ZoneID,l4info[i] / spp)) {
			pl_pre = pl_node;
			pl_node->startPageID = Zone_AllocNextPage(zone);
			l4info[i] = pl_node->startPageID * spp;
			pl_node = (PageList *)(pl->head.next);
		}
	}

	if(pi->L3InfoLen != 0) {
		if(pi->L2InfoLen != 0) {
			l2info[pi->L2Index] = pi->PageID;
			l3info[pi->L3Index] = pi->PageID;
		}
		else
			l3info[pi->L3Index] = pi->PageID;
	}

	pl_pre->head.next = NULL;
	return pl_node;
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
	
#ifndef NO_ERROR
	Context *conptr = (Context *)context;
	VNandInfo *vnand = &conptr->vnand;
	Message force_recycle_msg;
	int msghandle;
	ForceRecycleInfo frinfo;
#endif
	count = ZoneManager_GetAheadCount(context);
	for (i = 0; i < 4 - count; i++){
		zone = ZoneManager_AllocZone(context);
		if (!zone) {
			ndprint(ZONEMANAGER_INFO,"ZoneManager,WARNING: There is not enough zone and start force recycle \n");
#ifndef NO_ERROR			
			/* force recycle */
			frinfo.context = context;
			frinfo.pagecount = vnand->PagePerBlock;
			frinfo.suggest_zoneid = -1;
			force_recycle_msg.msgid = FORCE_RECYCLE_ID;
			force_recycle_msg.prio = FORCE_RECYCLE_PRIO;
			force_recycle_msg.data = (int)&frinfo;
			
			msghandle = Message_Post(conptr->thandle, &force_recycle_msg, WAIT);
			Message_Recieve(conptr->thandle, msghandle);
			
			zone = ZoneManager_AllocZone(context);
#endif				
		}
		ZoneManager_SetAheadZone(context,zone);
	}	

	count = ZoneManager_GetAheadCount(context);
	if (count > 0 && count < 4) {
		ndprint(ZONEMANAGER_ERROR,"WARNING: can't alloc four zone beforehand, free zone count is %d \n", count);
		return 0;
	}
	else if (count == 0){
		ndprint(ZONEMANAGER_ERROR,"ERROR: There is no free zone exist!!!!!! \n");
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
 *	write_last_data - write last data
 *
 *	@zonep: operate object
 *	@pi: write pageinfo
 *	@pl: write pagelist
 */
static int write_last_data(ZoneManager *zonep, PageInfo *pi, PageList *pl)
{
	int ret = 0;
	unsigned int pagecount;
	unsigned int freepage_count;
	Zone *zone = zonep->last_zone;
	PageList *pl_next = NULL;

	pagecount = zonep->pagecount;
	freepage_count = Zone_GetFreePageCount(zone);

	if (freepage_count >= pagecount) {
		update_l1l2l3l4_and_pagelist(zonep,zone,pi,pl);
		ret = Zone_MultiWritePage(zone,pagecount,pl,pi);
		
		if (freepage_count == pagecount)
			alloc_new_zone_write(zonep->context,zone);
	}
	else {
		if(freepage_count == 1) {
			ret = Zone_MultiWritePage(zone,0,NULL,pi);
			if(ret != 0) {
				ndprint(ZONEMANAGER_ERROR,"vNand MultiPage Write error func %s line %d \n",
					__FUNCTION__,__LINE__);
				return -1;
			}
		}
		else if(freepage_count > 1) {
			pl_next = update_l1l2l3l4_and_pagelist(zonep,zone,pi,pl);
			ret = Zone_MultiWritePage(zone,pagecount,pl,pi);
			if(ret != 0) {
				ndprint(ZONEMANAGER_ERROR,"vNand MultiPage Write error func %s line %d \n",
					__FUNCTION__,__LINE__);
				return -1;
			}	
		}

		/* alloc a new zone to write */
		zone = alloc_new_zone_write (zone->context,zone);

		/* write left data to new zone */
		if (pl_next)
			pl = pl_next;
		
		update_l1l2l3l4_and_pagelist(zonep,zone,pi,pl);
		ret = Zone_MultiWritePage(zone,pagecount,pl,pi);
		if(ret != 0) {
			ndprint(ZONEMANAGER_ERROR,"vNand MultiPage Write error func %s line %d \n",
				__FUNCTION__,__LINE__);
			return -1;
		}		
	}
	
	return ret;	
}

/** 
 *	deal_data - read last data and then deal with it
 *
 *	@zonep: operate object
 *	@pi: write pageinfo
 */
static int deal_data(ZoneManager *zonep, PageInfo *pi)
{
	PageList *pl = zonep->pl;
	BuffListManager *blm = ((Context *)(zonep->context))->blm;

	if (!pl)
		return 0;
	
	if (zonep->need_write_last_data == 1)
		write_last_data(zonep, pi, pl);

	BuffListManager_freeAllList((int)blm, (void *)&pl, sizeof(PageList));

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
		ndprint(ZONEMANAGER_ERROR,"no zone used func %s line %d\n",
					__FUNCTION__,__LINE__);
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
	else if (ret == -2) {
		ndprint(ZONEMANAGER_INFO,"no pageinfo write in maximum serial number zone func %s line %d \n",
					__FUNCTION__,__LINE__);
		return 0;	
	}

	get_current_write_zone_info(zonep);

	if (pi->zoneID != 0xffff) {
		zonep->last_rzone_id = pi->zoneID;
		CacheManager_Init(zonep->context);
		start_recycle(zonep);
	}
	else
		ret = deal_data(zonep, pi);

	free_last_data_buf(zonep);

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
	Context * conptr = (Context *)context;
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
	zoneptr->startblockID = zonep->startblockID[zoneptr->ZoneID];
	zoneptr->L1InfoLen = zonep->vnand->BytePerPage;
	zoneptr->L2InfoLen = zonep->l2infolen;
	zoneptr->L3InfoLen = zonep->l3infolen;
	zoneptr->L4InfoLen = zonep->l4infolen;

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
	zoneptr->maxserial = zonep->maxserial;
	zoneptr->L1Info = (unsigned char *)(zonep->L1).page;
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
	ZoneManager *zonep = conptr->zonep;
	return &(zonep->L1);
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
	int pageperzone = conptr->vnand.PagePerBlock * BLOCKPERZONE(zone->vnand);
	ZoneManager *zonep = Nand_ContinueAlloc(sizeof(ZoneManager));
	if(zonep == NULL)
	{
		ndprint(ZONEMANAGER_ERROR,"alloc zonemanager fail func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;
	}

	conptr->zonep = zonep;
	zonep->context = context;
	zonep->vnand = &conptr->vnand;
#ifdef TEST
	memset(zonep->vnand->pt_badblock_info, 0xff, zonep->vnand->BytePerPage);
#endif
	zonep->pt_zonenum = get_pt_zone_num(zonep->vnand);
	if (zonep->pt_zonenum == 0){
		ndprint(ZONEMANAGER_ERROR,"The partition has no zone that can be used!\n");
		return -1;
	}

	zonep->zonevalidinfo.zoneid = -1;
	zonep->zonevalidinfo.current_count = -1;
	zonep->zonevalidinfo.wpages = (Wpages *)Nand_ContinueAlloc(sizeof(Wpages) * pageperzone);
	if (zonep->zonevalidinfo.wpages == NULL)
	{
		ndprint(ZONEMANAGER_ERROR,"alloc zonemanager fail func %s line %d \n",
					__FUNCTION__,__LINE__);
		return -1;

	}

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

	fill_zone_start_blockid(zonep);
	
	scan_page_info(zonep);

	conptr->l1info = &zonep->L1;

	ndprint(ZONEMANAGER_INFO, "free count %d used count %d maxserial zoneid %d maxserialnum %d \n", 
		zonep->freeZone->count, zonep->useZone->usezone_count, zonep->last_zone_id, zonep->maxserial);
	
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
	deinit_free_node(zonep);
	deinit_used_node(zonep);
	free_zonemanager_memory(zonep);
	Nand_ContinueFree(zonep);
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
	unsigned int vaildpage =  conptr->vnand.PagePerBlock * BLOCKPERZONE(zone->vnand);

	index = Hash_FindFirstLessLifeTime(zonep->useZone,lifetime,&sigp);
	if(index == -1){
		ndprint(ZONEMANAGER_ERROR,"Can't find the lifetime. Please give a new larger lifetime!! \n");
		return -1;
	}

	do {
		if(vaildpage > sigp->validpage)
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
	ZoneManager *zonep = conptr->zonep;
	unsigned int index = sectorID / (zonep->vnand->PagePerBlock * 
						zonep->vnand->TotalBlocks *(zonep->vnand->BytePerPage/SECTOR_SIZE));
	return (zonep->L1).page[index];					
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
	ZoneManager *zonep = conptr->zonep;
	unsigned int index = sectorID / (zonep->vnand->PagePerBlock * 
									 zonep->vnand->TotalBlocks *(zonep->vnand->BytePerPage/SECTOR_SIZE));
	NandMutex_Lock(&(zonep->L1).mutex);
	 (zonep->L1).page[index] = PageID;				
	NandMutex_Unlock(&(zonep->L1).mutex);
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

	zoneptr->sigzoneinfo = zonep->sigzoneinfo + ZoneID;
	zoneptr->startblockID = zonep->startblockID[ZoneID];
	zoneptr->top = zonep->sigzoneinfo;
	zoneptr->L1InfoLen = zonep->vnand->BytePerPage;
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
		ndprint(ZONEMANAGER_ERROR,"delete used node error func %s line %d\n",
					__FUNCTION__,__LINE__);
		goto err;			
	}
	zoneptr->memflag = i;
	zoneptr->ZoneID = ZoneID;
	zoneptr->mem0 = zonep->mem0 + i * zonep->vnand->BytePerPage;
	zoneptr->maxserial = zonep->maxserial;
	zoneptr->L1Info = (unsigned char *)(zonep->L1).page;
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
		if(zonep->aheadflag[i] == 0);
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

