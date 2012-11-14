#include "errhandle.h"
#include "context.h"
#include "nandsigzoneinfo.h"
#include "nandzoneinfo.h"
#include "vNand.h"
#include "l2vNand.h"
#include "nanddebug.h"
#include "nmbitops.h"
//#include "badblockinfo.h"

#define ZONEPAGE1INFO(vnand)      ((vnand)->v2pp->_2kPerPage)
#define ZONEPAGE2INFO(vnand)      ((vnand)->v2pp->_2kPerPage + 1)

#if 1
static BlockList *create_blocklist(Context *conptr, int start_blockid, int total_blockcount)
{
	int i = 0;
	BlockList *bl = NULL;
	BlockList *bl_node = NULL;
	int blm = (int)conptr->blm;

	for (i = start_blockid; i < start_blockid + total_blockcount; i++) {
		if (bl == NULL) {
			bl = (BlockList *)BuffListManager_getTopNode(blm,sizeof(BlockList));
			bl_node = bl;
		} else
			bl_node = (BlockList *)BuffListManager_getNextNode(blm,(void *)bl,sizeof(BlockList));

		bl_node->startBlock = i;
		bl_node->BlockCount = 1;
	}

	return bl;
}
#else
static BlockList *create_blocklist(Context *conptr, int start_blockid, int total_blockcount)
{
	int i = 0;
	BlockList *bl = NULL;
	BlockList *bl_node = NULL;
	int blockcount = 0;
	VNandInfo *vnand = &conptr->vnand;
	int blm = (int)conptr->blm;

	for (i = start_blockid; i < start_blockid + total_blockcount; i++) {
		if (!vNand_IsBadBlock(vnand,i))//block is ok
			blockcount++;
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

static int erase_err_zone(int errinfo)
{
	int ret = 0;
	struct singlelist *pos;
	BlockList *bl;
	BlockList *bl_node = NULL;
	ErrInfo *einfo = (ErrInfo *)errinfo;
	unsigned short zoneid = einfo->err_zoneid;
	Context *conptr = (Context *)(einfo->context);
	VNandInfo *vnand = &conptr->vnand;
	ZoneManager *zonep = conptr->zonep;
	//int start_blockno = BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,zoneid);
	int start_blockno = zoneid * BLOCKPERZONE(zonep->vnand);
	int next_start_blockno = 0;
	int blockcount = 0;
	int blmid = (int)conptr->blm;

	if (zoneid == zonep->pt_zonenum - 1)
		blockcount = zonep->vnand->TotalBlocks - start_blockno;
	else {
		//next_start_blockno = BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,zoneid + 1);
		next_start_blockno = (zoneid + 1) * BLOCKPERZONE(zonep->vnand);
		blockcount = next_start_blockno - start_blockno;
	}

	bl = create_blocklist(conptr, start_blockno, blockcount);
	if (bl) {
		ret = vNand_MultiBlockErase(vnand,bl);
		if(ret < 0) {
			singlelist_for_each(pos,&bl->head){
				bl_node = singlelist_entry(pos, BlockList, head);
				if(bl_node->retVal == -1)
					nm_set_bit(bl_node->startBlock - start_blockno, (unsigned int *)&(conptr->top + zoneid)->badblock);
			}
		}

		BuffListManager_freeAllList(blmid,(void **)&bl,sizeof(BlockList));
	}

	return ret;
}

static int write_page0(int errinfo)
{
	int ret;
	PageList px;
	PageList *pl = &px;
	unsigned char *buf;
	SigZoneInfo *sigzoneinfo;
	NandSigZoneInfo *nandsigzoneinfo;
	ErrInfo *einfo = (ErrInfo *)errinfo;
	unsigned short zoneid = einfo->err_zoneid;
	Context *conptr = (Context *)(einfo->context);
	VNandInfo *vnand = &conptr->vnand;
	ZoneManager *zonep = conptr->zonep;
	//int start_blockno = BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,zoneid);
	int start_blockno = zoneid * BLOCKPERZONE(zonep->vnand);
	unsigned short badblockno = 0;
	int last_blockno = vnand->TotalBlocks - start_blockno;

	buf = (unsigned char *)Nand_ContinueAlloc(vnand->BytePerPage);
	if (!buf) {
		ndprint(1,"Nand_ContinueAlloc error func %s line %d \n"
					,__FUNCTION__,__LINE__);
		return -1;
	}

	sigzoneinfo = conptr->top + zoneid;
	nandsigzoneinfo = (NandSigZoneInfo *)buf;
	nandsigzoneinfo->ZoneID = zoneid;
	nandsigzoneinfo->lifetime = sigzoneinfo->lifetime + 1;
	nandsigzoneinfo->badblock = sigzoneinfo->badblock;
	while(nm_test_bit(badblockno,(unsigned int *)&((zonep->sigzoneinfo+zoneid)->badblock)) && (++badblockno));
	if(badblockno >= last_blockno){
		ndprint(1," %s %d badblockno:%d blocknum:%d",__func__,__LINE__,badblockno,last_blockno);
		return 0;
	}
	pl->startPageID = (start_blockno +badblockno)* vnand->PagePerBlock;
	pl->pData = (void *)buf;
	pl->Bytes = vnand->BytePerPage;
	pl->retVal = 0;
	(pl->head).next = NULL;
	pl->OffsetBytes = 0;

	ret = vNand_MultiPageWrite(vnand,pl);
	if(ret < 0) {
		ndprint(1,"vNand_MultiPageWrite error func %s line %d pageid:%d\n"
				,__FUNCTION__,__LINE__,pl->startPageID);
		return -1;
	}

	Nand_ContinueFree(buf);

	return ret;
}

int read_page0_err_handler(int errinfo)
{
	int ret;

	ret = erase_err_zone(errinfo);
	if(ret < 0)
		return -1;

	return write_page0(errinfo);
}

int read_page1_err_handler(int errinfo)
{
	return 0;
}

/**
 *	alloc_pageinfo  -  alloc L1Info, L2Info, L3Info and L4Info of pageinfo
 *
 *	@zonep: to konw whether L2InfoLen and L3InfoLen are 0 or not
 */
static PageInfo *alloc_pageinfo(ZoneManager *zonep)
{
	PageInfo *pi = NULL;

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

	pi->L1InfoLen = zonep->L1->len;
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
 *	@zonep: operate object
 *	@pageinfo: which need to free
 */
static void free_pageinfo(ZoneManager *zonep, PageInfo *pageinfo)
{
	if (!pageinfo)
		return;

	if (zonep->l2infolen)
		Nand_VirtualFree(pageinfo->L2Info);

	if (zonep->l3infolen)
		Nand_VirtualFree(pageinfo->L3Info);

	Nand_VirtualFree(pageinfo->L4Info);
	Nand_VirtualFree(pageinfo);
}

static int get_prev_zone(int errinfo, Zone **zone)
{
	int ret;
	unsigned char *buf;
	NandZoneInfo *nandzoneinfo;
	ErrInfo *einfo = (ErrInfo *)errinfo;
	unsigned short prev_zoneid;
	unsigned short last_zoneid = einfo->err_zoneid;
	int context = einfo->context;
	Context *conptr = (Context *)context;
	VNandInfo *vnand = &conptr->vnand;
	ZoneManager *zonep = conptr->zonep;
	unsigned short badblockno = 0;
	buf = (unsigned char *)Nand_ContinueAlloc(vnand->BytePerPage);
	if (!buf) {
		ndprint(1,"Nand_VirtualAlloc error func %s line %d \n"
					,__FUNCTION__,__LINE__);
		return -1;
	}

	/*ret = vNand_PageRead(vnand,BadBlockInfo_Get_Zone_startBlockID(zonep->badblockinfo,last_zoneid) *
	  vnand->PagePerBlock + ZONEPAGE1INFO(vnand),0,vnand->BytePerPage,buf);*/
	while(nm_test_bit(badblockno,(unsigned int *)&((zonep->sigzoneinfo+last_zoneid)->badblock)) && (++badblockno));

	ret = vNand_PageRead(vnand,(last_zoneid * BLOCKPERZONE(zonep->vnand) + badblockno) *
		vnand->PagePerBlock + ZONEPAGE1INFO(vnand),0,vnand->BytePerPage,buf);
	if(ret < 0) {
		ndprint(1,"vNand_MultiPageWrite error func %s line %d \n"
					,__FUNCTION__,__LINE__);
		Nand_ContinueFree(buf);
		*zone = NULL;
		return -1  ;
	}
	nandzoneinfo = (NandZoneInfo *)buf;
	prev_zoneid = nandzoneinfo->preZone.ZoneID;
	if (prev_zoneid == 0xffff) {
		Nand_ContinueFree(buf);
		*zone = NULL;
		return 0;
	}
	Nand_ContinueFree(buf);
	*zone = ZoneManager_Get_Used_Zone(zonep,prev_zoneid);
	return 0;
}

static int recover_L1info(int errinfo, Zone *zone)
{
	int ret;
	PageInfo *pi = NULL;
	ErrInfo *einfo = (ErrInfo *)errinfo;
	int context = einfo->context;
	Context *conptr = (Context *)context;
	VNandInfo *vnand = &conptr->vnand;
	ZoneManager *zonep = conptr->zonep;
	unsigned int *l1info = conptr->l1info->page;

	pi = alloc_pageinfo(zonep);
	if (!pi) {
		ndprint(1,"alloc_pageinfo error func %s line %d \n"
					,__FUNCTION__,__LINE__);
		return -1;
	}

	NandMutex_Lock(&conptr->l1info->mutex);

	ret = vNand_PageRead(vnand,zone->startblockID *
		vnand->PagePerBlock + ZONEPAGE2INFO(vnand),0,vnand->BytePerPage,l1info);
	if(ret < 0) {
		ndprint(1,"vNand_PageRead error func %s line %d \n"
					,__FUNCTION__,__LINE__);
		goto err;
	}

	ret = Zone_FindFirstPageInfo(zone,pi);
	if (ISERROR(ret) && !ISNOWRITE(ret))
		goto err;
	else if (ISNOWRITE(ret))
		goto exit;

	l1info[pi->L1Index] = pi->PageID;

	while (1) {
		if (zone->NextPageInfo == 0)
			break;
		else if (zone->NextPageInfo == 0xffff) {
			ndprint(1,"pageinfo data error func %s line %d \n",
						__FUNCTION__,__LINE__);
			goto err;
		}

		ret = Zone_FindNextPageInfo(zone,pi);
		if (ISERROR(ret) && !ISNOWRITE(ret))
			goto err;
		else if (ISNOWRITE(ret))
			goto exit;

		l1info[pi->L1Index] = pi->PageID;
	}

exit:
	NandMutex_Unlock(&conptr->l1info->mutex);
	free_pageinfo(zonep,pi);
	return 0;

err:
	NandMutex_Unlock(&conptr->l1info->mutex);
	free_pageinfo(zonep,pi);
	return -1;
}

static int zone_data_move_and_erase(int errinfo)
{
	int ret = 0;
	Zone *zone = NULL;
	SigZoneInfo *prev = NULL;
	SigZoneInfo *next = NULL;
	ForceRecycleInfo frinfo;
	ErrInfo *einfo = (ErrInfo *)errinfo;
	int context = einfo->context;
	unsigned short zoneid = einfo->err_zoneid;

	ZoneManager_GetAheadZone(context, &zone);
	if (!zone)
		zone = ZoneManager_AllocZone(context);
	ZoneManager_SetCurrentWriteZone(context,zone);
	prev = ZoneManager_GetPrevZone(context);
	next = ZoneManager_GetNextZone(context);
	Zone_Init(zone,prev,next);

	ret = CacheManager_Init(context);
	if (ret != 0) {
		ndprint(1,"ERROR:CacheManager_Init failed func %s line %d \n",
			__FUNCTION__, __LINE__);
		return -1;
	}

	frinfo.context = context;
	frinfo.pagecount = -1;
	frinfo.suggest_zoneid = zoneid;
	return Recycle_OnForceRecycle((int)&frinfo);
}

int read_page2_err_handler(int errinfo)
{
	int ret;
	Zone *zone;

	ret = get_prev_zone(errinfo, &zone);
	if(ret != 0)
		return -1;
	else if (zone) {
		ret = recover_L1info(errinfo, zone);
		if(ret < 0)
			return -1;
	}

	return zone_data_move_and_erase(errinfo);
}

int read_ecc_err_handler(int errinfo)
{
	ForceRecycleInfo frinfo;
	ErrInfo *einfo = (ErrInfo *)errinfo;
	int context = einfo->context;

	frinfo.context = context;
	frinfo.pagecount = -1;
	frinfo.suggest_zoneid = einfo->err_zoneid;
	return Recycle_OnForceRecycle((int)&frinfo);
}

int read_first_pageinfo_err_handler(int errinfo)
{
	int ret;
	ErrInfo *einfo = (ErrInfo *)errinfo;
	ZoneManager *zonep = ((Context *)(einfo->context))->zonep;

	ret = erase_err_zone(errinfo);
	if(ret < 0)
		return -1;

	ret = write_page0(errinfo);
	if(ret < 0)
		return -1;

	return ZoneManager_Move_UseZone_to_FreeZone(zonep, einfo->err_zoneid);
}

