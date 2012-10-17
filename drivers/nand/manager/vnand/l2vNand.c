#include "clib.h"
#include "pagelist.h"
#include "vnandinfo.h"
#include "blocklist.h"
#include "context.h"
#include "NandAlloc.h"
#include "NandSemaphore.h"
#include "nanddebug.h"
#include "nandinterface.h"
#include "vNand.h"
#include "timeinterface.h"

//#define DBG_UP_PL

#ifdef DBG_UP_PL
#define VIRTUL_PAGESIZE 0x800
static void dump_uppl (PageList *pl) {
	struct singlelist *list = NULL;
	PageList *prev = NULL;
	PageList *this = NULL;

	this = pl;

	singlelist_for_each(list,&pl->head) {
		this = singlelist_entry(list,PageList,head);
		if (!prev) {
			prev = this;
			continue;
		}

		if (this) {

			if (((prev->retVal != 0) && (prev->retVal != 1))
                                        || ((this->retVal != 0) && (this->retVal != 1))) {
				ndprint(VNAND_ERROR, "ERROR: retVal is not inited:\n"
                                                "prev(%d)(%p)<%d>[%d][%d]:this(%d)(%p)<%d>[%d][%d]\n",
						prev->startPageID, prev->pData,
                                                prev->retVal, prev->OffsetBytes ,prev->Bytes,
						this->startPageID, this->pData,
                                                this->retVal, this->OffsetBytes, this->Bytes);
			}

			if (((this->pData - prev->pData) != prev->Bytes) && (prev->retVal == 1)) {
				ndprint(VNAND_ERROR, "ERROR: pData can not mergeable, but combineed:\n"
                                                "prev(%d)(%p)<%d>[%d][%d]:this(%d)(%p)<%d>[%d][%d]\n",
						prev->startPageID, prev->pData,
                                                prev->retVal, prev->OffsetBytes ,prev->Bytes,
						this->startPageID, this->pData,
                                                this->retVal, this->OffsetBytes, this->Bytes);
			}

			if (((this->pData - prev->pData) == prev->Bytes) && (prev->retVal == 0)) {
				ndprint(VNAND_ERROR, "ERROR: pData can mergeable, but not combineed:\n"
                                                "prev(%d)(%p)<%d>[%d][%d]:this(%d)(%p)<%d>[%d][%d]\n",
						prev->startPageID, prev->pData,
                                                prev->retVal, prev->OffsetBytes ,prev->Bytes,
						this->startPageID, this->pData,
                                                this->retVal, this->OffsetBytes, this->Bytes);
			}
/*
			if (((this->pData - prev->pData) == prev->Bytes) && (prev->retVal == 1)
                                        && ((this->startPageID - prev->startPageID != 0)
                                                 && (this->startPageID - prev->startPageID != 1))
                                        ) {
				ndprint(VNAND_ERROR, "ERROR: pageid can not mergeable:\n"
                                                "prev(%d)(%p)<%d>[%d][%d]:this(%d)(%p)<%d>[%d][%d]\n",
						prev->startPageID, prev->pData,
                                                prev->retVal, prev->OffsetBytes ,prev->Bytes,
						this->startPageID, this->pData,
                                                this->retVal, this->OffsetBytes, this->Bytes);
			}
*/		}

		prev = this;
	}
}

#endif

void dump_availablebadblock(VNandManager *vm)
{
	int i=0,j=0;
	PPartition *pt=NULL;
	ndprint(VNAND_INFO,"================%s start =============\n",__func__);
	for(i=0;i<vm->pt->ptcount; i++){
		pt = &(vm->pt->ppt[i]);
		if(pt->mode == ONCE_MANAGER)
			break;

		ndprint(VNAND_INFO,"\n pt->name = %s, pt = %p i = %d  bad block info:\n", pt->name, pt ,i);
		for(j=0; j<pt->byteperpage * BADBLOCKINFOSIZE; j++){
			if(j%4==0)
				ndprint(VNAND_INFO,"\n %d: ",j);
			ndprint(VNAND_INFO,"%8d ",pt->badblock->pt_badblock_info[j]);
			if ((j > 8) && (pt->badblock->pt_badblock_info[j] == -1))
				break;
		}
		ndprint(VNAND_INFO,"\navailable block info: pt->totalblocks = %d \n", pt->totalblocks);
		for(j=0; j < pt->totalblocks; j++) {
			if(j%4==0)
				ndprint(VNAND_INFO,"\n%d: ",j);
			ndprint(VNAND_INFO,"%8d ",pt->badblock->pt_availableblockid[j]);
		}
	}
	ndprint(VNAND_INFO,"\n");
	ndprint(VNAND_INFO,"================%s end =============\n",__func__);
}

static inline unsigned int L2PblockID(VNandInfo *vnand, unsigned int blockid)
{
	if(blockid >= vnand->TotalBlocks){
		ndprint(VNAND_ERROR,"ERROR: %s blockid is too large!!!!!",__func__);
		return -1;
	}
	return vnand->badblock->pt_availableblockid[blockid];
}

static int write_pt_badblock_info(VNandInfo *vnand, PageList *pl)
{
	return __vNand_MultiPageWrite(vnand,pl);
}

static void scan_pt_badblock_info_write_to_nand(PPartition *pt, int pt_id, VNandInfo *evn, PageList *pl)
{
	int i,j = 0, ret = 0;
	unsigned int start_blockno;
	unsigned int end_blockno;
	VNandInfo vn;
	int size = pt->byteperpage * BADBLOCKINFOSIZE;

	CONV_PT_VN(pt, &vn);
	start_blockno = vn.startBlockID;
	ndprint(VNAND_DEBUG, "vn.TotalBlocks = %d\n", vn.TotalBlocks);
	end_blockno = vn.startBlockID + vn.TotalBlocks;
	for (i = start_blockno; i < end_blockno; i++) {
		if (__vNand_IsBadBlock(&vn, i) && j * 4 < size){
			pt->badblock->pt_badblock_info[j++] = i;
		}
		else {
			if(j * 4 >= size){
				ndprint(VNAND_ERROR,"too many bad block in pt %d\n", pt_id);
				while(1);
			}
		}
	}

	ret = write_pt_badblock_info(evn, pl);
	if (ret != 0) {
		ndprint(VNAND_ERROR, "write pt %d badblock info error, ret =%d\n", pt_id, ret);
		while(1);
	}
}

static int alloc_badblock_info(PPartition *pt)
{
	pt->badblock = (struct badblockhandle *)Nand_ContinueAlloc(sizeof(struct badblockhandle));
	if(NULL == pt->badblock) {
		ndprint(VNAND_ERROR, "alloc memoey fail func %s line %d \n",
				__FUNCTION__, __LINE__);
		return -1;
	}

	pt->badblock->pt_badblock_info = (unsigned int *)Nand_ContinueAlloc(pt->byteperpage * BADBLOCKINFOSIZE);
	if(NULL == pt->badblock->pt_badblock_info) {
		ndprint(VNAND_ERROR, "alloc memoey fail func %s line %d \n",
				__FUNCTION__, __LINE__);
		goto err0;
	}
	memset(pt->badblock->pt_badblock_info, 0xff, pt->byteperpage * BADBLOCKINFOSIZE);

	pt->badblock->pt_availableblockid = (unsigned int *)Nand_ContinueAlloc(pt->totalblocks * sizeof(unsigned int));
	if(NULL == pt->badblock->pt_availableblockid) {
		ndprint(VNAND_ERROR, "alloc memoey fail func %s line %d \n",
				__FUNCTION__, __LINE__);
		goto err1;
	}
	memset(pt->badblock->pt_availableblockid, 0xff, pt->totalblocks * sizeof(int));

	return 0;

err1:
	Nand_ContinueFree(pt->badblock->pt_badblock_info);
err0:
	Nand_ContinueFree(pt->badblock);
	return -1;
}

static void free_badblock_info(PPartition *pt)
{
	Nand_ContinueFree(pt->badblock->pt_availableblockid);
	Nand_ContinueFree(pt->badblock->pt_badblock_info);
	Nand_ContinueFree(pt->badblock);
}

static void PtAvailableBlockID_Init(VNandManager *vm)
{
	int badblock_number = 0;
	int blockid = 0;
	int pos,i=0,j=0;
	PPartition *pt = NULL;

	for(i=0; i<vm->pt->ptcount; i++){
		pt = &vm->pt->ppt[i];
		if(pt->mode == ONCE_MANAGER) {
			if (i != (vm->pt->ptcount - 1)) {
				ndprint(VNAND_ERROR,"%s: error block table partition position\n", __func__);
				while(1);
			}
			break;
		}

		blockid = 0;
		badblock_number = 0;
		for(j=0; j < (pt->byteperpage * BADBLOCKINFOSIZE) / sizeof(unsigned int); j++){
			if(pt->badblock->pt_badblock_info[j]==0xffffffff)
				break;
			else
				badblock_number++;
		}
		for(pos=0; pos < pt->totalblocks; pos++){
			for(j=0; j<badblock_number; j++){
				if(pt->badblock->pt_badblock_info[j] == blockid){
					blockid++;
				}
			}
			pt->badblock->pt_availableblockid[pos] = blockid;
			blockid++;
		}
	}
	//dump_availablebadblock(vm);
}

static void UpdatePageList(VNandInfo *vnand, PageList* pl)
{
	PageList *pagelist = NULL;
	struct singlelist *list = NULL;
	unsigned int blockid = 0;
	unsigned int pageoffset = 0;

	singlelist_for_each(list,&pl->head) {
		pagelist = singlelist_entry(list,PageList,head);
		pagelist->_startPageID = pagelist->startPageID;
		blockid = pagelist->startPageID / vnand->PagePerBlock;
		pageoffset = pagelist->startPageID % vnand->PagePerBlock;
		pagelist->startPageID = L2PblockID(vnand, blockid) * vnand->PagePerBlock + pageoffset;
                if(pagelist->startPageID < 0 ||
                                pagelist->startPageID >= vnand->PagePerBlock * vnand->TotalBlocks) {
                        ndprint(VNAND_ERROR,"%s: _pageid = %d pageid = %d totalblocks = %d \n"
                                        , ((PPartition *)vnand->prData)->name
                                        , pagelist->_startPageID
                                        , pagelist->startPageID
                                        , vnand->TotalBlocks
                                        );
                }
	}
}

static void GoBackPageList(PageList* pl)
{
	PageList *pagelist = NULL;
	struct singlelist *list = NULL;

	singlelist_for_each(list,&pl->head) {
		pagelist = singlelist_entry(list,PageList,head);
		pagelist->startPageID = pagelist->_startPageID;
	}
}

static void UpdateBlockList(VNandInfo *vnand, BlockList* bl)
{
	BlockList *blocklist = NULL;
	struct singlelist *list = NULL;

	singlelist_for_each(list,&bl->head) {
		blocklist = singlelist_entry(list,BlockList,head);
		blocklist->_startBlock = blocklist->startBlock;
		blocklist->startBlock = L2PblockID(vnand, blocklist->startBlock);
	}
}

static void GoBackBlockList(BlockList* bl)
{
	BlockList *blocklist = NULL;
	struct singlelist *list = NULL;
	blocklist = bl;

	singlelist_for_each(list,&bl->head) {
		blocklist = singlelist_entry(list,BlockList,head);
		blocklist->startBlock = blocklist->_startBlock;
	}
}

int vNand_MultiPageWrite(VNandInfo* vNand, PageList* pl){
	int ret = 0;

#ifdef DBG_UP_PL
	dump_uppl(pl);
#endif
	UpdatePageList(vNand, pl);
	ret = __vNand_MultiPageWrite(vNand, pl);
	GoBackPageList(pl);

	return ret;
}

int vNand_MultiPageRead(VNandInfo* vNand, PageList* pl){
	int ret = 0;

#ifdef DBG_UP_PL
	dump_uppl(pl);
#endif
	UpdatePageList(vNand, pl);
	ret = __vNand_MultiPageRead(vNand, pl);
	GoBackPageList(pl);
	return ret;
}

int vNand_MultiBlockErase(VNandInfo* vNand, BlockList* bl){
	int ret = 0;

	UpdateBlockList(vNand, bl);
	ret = __vNand_MultiBlockErase(vNand, bl);
	GoBackBlockList(bl);

	return ret;
}

int vNand_CopyData(VNandInfo* vNand,PageList* rpl, PageList* wpl){
	int ret = 0;

	UpdatePageList(vNand, rpl);
	UpdatePageList(vNand, wpl);
	ret = __vNand_CopyData(vNand, rpl, wpl);
	GoBackPageList(rpl);
	GoBackPageList(wpl);

	return ret;
}

int vNand_IsBadBlock (VNandInfo* vNand,int blockid )
{
	int ret = 0;

	L2PblockID(vNand, blockid);
	ret = __vNand_IsBadBlock(vNand, blockid);

	return ret;
}

static PageList *create_pagelist(PPartition *pt, int blmid)
{
	int i;
	PageList *pl = NULL;
	PageList *pl_node = NULL;

	for (i = 0; i < BADBLOCKINFOSIZE; i++) {
		if (pl == NULL) {
			pl = (PageList *)BuffListManager_getTopNode(blmid,sizeof(PageList));
			pl_node = pl;
		} else
			pl_node = (PageList *)BuffListManager_getNextNode(blmid,(void *)pl,sizeof(PageList));

		pl_node->Bytes = pt->byteperpage;
		pl_node->OffsetBytes = 0;
		pl_node->retVal = 0;
		pl_node->startPageID = -1;
		pl_node->pData = NULL;
	}

	return pl;
}

static void fill_pagelist(PPartition *pt, PageList *pl, int pt_id)
{
	int offset = 0;
	struct singlelist *pos = NULL;
	PageList *pl_node = NULL;
	unsigned int startpageid = pt_id * BADBLOCKINFOSIZE;

	singlelist_for_each(pos,&pl->head) {
		pl_node = singlelist_entry(pos,PageList,head);
		pl_node->startPageID = startpageid;
		pl_node->pData = (void *)((char *)pt->badblock->pt_badblock_info + offset);
		pl_node->retVal = 0;
		startpageid++;
		offset += pt->byteperpage;
	}
}

static void read_badblock_info_page(VNandManager *vm)
{
	int i, j, ret;
	int blmid;
	VNandInfo error_vn;
	PageList *pl = NULL;
	PPartition *pt = NULL;
	PPartition *lastpt = NULL;
	int startblock = 0, badcnt = 0,blkcnt = 0;
	int badblockcount = 0;

	if ((vm->pt->ptcount - 1) * BADBLOCKINFOSIZE > vm->info.PagePerBlock) {
		ndprint(VNAND_ERROR,"ERROR: BADBLOCKINFOSIZE = %d is too large,vnand->PagePerBlock = %d func %s line %d \n",
				BADBLOCKINFOSIZE,vm->info.PagePerBlock,__FUNCTION__,__LINE__);
		while(1);
	}

	// find it which partition mode is ONCE_MANAGER
	for(i = 0; i < vm->pt->ptcount; i++){
		pt = &vm->pt->ppt[i];
		if(pt->mode == ONCE_MANAGER){
			if (i != (vm->pt->ptcount - 1)) {
				ndprint(VNAND_ERROR,"%s: error block table partition position\n", __func__);
				while(1);
			}
			break;
		}

		ret = alloc_badblock_info(pt);
		if(ret != 0) {
			ndprint(VNAND_ERROR,"alloc badblock info memory error func %s line %d \n",
					__FUNCTION__,__LINE__);
			while(1);
		}

		lastpt = pt;
	}

	if(i == vm->pt->ptcount){
		ndprint(VNAND_INFO, "INFO: not find badblock partition\n");
		return;
	}

	startblock = 0;
	CONV_PT_VN(pt,&error_vn);
	//for error partblock bad block
	//for block number
	while(blkcnt < pt->totalblocks) {
		startblock--;
		if(__vNand_IsBadBlock(&error_vn,startblock)) {
			badcnt++;
			if (badcnt > pt->badblockcount) {
				ndprint(VNAND_ERROR,"too many badblocks, %s(line:%d) badcnt = %d,\n pt->badblockcount = %d\n",
						__func__, __LINE__, badcnt, pt->badblockcount);
				while(1);
			}
		}
		else
			blkcnt++;
	}
	//for error block self partition & badblock
	//error and last patition all spec is samed
	lastpt->totalblocks -= (badcnt + pt->totalblocks);
	lastpt->PageCount -= (badcnt + pt->totalblocks) * pt->pageperblock;

	//chanage error pt startblock for write & read
	pt->startblockID += startblock * pt->totalblocks;
	pt->startPage += startblock * pt->pageperblock;

	if ((lastpt->totalblocks <= 0) || (lastpt->PageCount <= 0)) {
		ndprint(VNAND_ERROR,
				"more bad blcoks,badcnt=%d,totalblocks=%d,PageCount = %d\n",
				badcnt, lastpt->totalblocks, lastpt->PageCount);
		while(1);
	}

	ndprint(VNAND_INFO, "Find bad block partition in block: %d, startblokdID = %d, pageid = %d\n",
			startblock, pt->startblockID, pt->startPage);

	blmid = BuffListManager_BuffList_Init();
	pl = create_pagelist(pt, blmid);
	if (!pl) {
		ndprint(VNAND_ERROR,"create_pagelist error func %s line %d \n",
				__FUNCTION__,__LINE__);
		while(1);
	}

	for(i = 0; i < vm->pt->ptcount; i++){
		pt = &vm->pt->ppt[i];
		if(pt->mode == ONCE_MANAGER){
			if (i != (vm->pt->ptcount - 1)) {
				ndprint(VNAND_ERROR,"%s: error block table partition position\n", __func__);
				while(1);
			}
			break;
		}

		fill_pagelist(pt, pl, i);
		ret = __vNand_MultiPageRead(&error_vn, pl);
		if (ret != 0) {
			if (ISNOWRITE(pl->retVal)) {
				ndprint(VNAND_INFO, "pt[%d] bad block table not creat\n", i);
				scan_pt_badblock_info_write_to_nand(pt,i,&error_vn,pl);
			} else {
				ndprint(VNAND_ERROR, "ERROR: pt[%d] bad bad block error!\n", i);
				while(1);
			}
		}
                /*
                 * pt->totalblocks -= pt's badblockcounts
                 * pt->PageCount -= pt's badblockcount * pt's pageperblock
                 * print (all pt)'s badblockinfo
                 **/
                ndprint(VNAND_INFO,"%s badblock info table\n",pt->name);
		for(j = 0; j < pt->totalblocks; j++) {
		        if(pt->badblock->pt_badblock_info[j] == -1)
		                break;
		        badblockcount++;
                        ndprint(VNAND_INFO,"%d ",pt->badblock->pt_badblock_info[j]);
		}
                ndprint(VNAND_INFO,"\n");
		if(badblockcount > 0) {
                        pt->totalblocks -= badblockcount;
			pt->PageCount -= badblockcount * pt->pageperblock;
                        badblockcount = 0;
		}
                ndprint(VNAND_INFO,"%s: totalblocks = %d PageCount = %d"
                                " badblockcount = %d\n"
                                ,pt->name
                                ,pt->totalblocks
                                ,pt->PageCount
                                ,badblockcount
                                );
	}
	BuffListManager_freeAllList(blmid,(void **)&pl,sizeof(PageList));
	BuffListManager_BuffList_DeInit(blmid);
}

static int vNand_ScanBadBlocks (VNandManager* vm)
{
	read_badblock_info_page(vm);

	ndprint(VNAND_INFO,"vNand_ScanBadBlocks finished! \n");

	return 0;
}

/*
  The following functions is for partition manager
*/
int vNand_Init (VNandManager** vm)
{
	int ret;

	ret = __vNand_Init(vm);
	if(ret != 0){
		ndprint(VNAND_ERROR,"vnand init failed!\n");
		return ret;
	}

	ret = vNand_ScanBadBlocks(*vm);
	if(ret != 0){
		ndprint(VNAND_ERROR,"bad block scan failed!\n");
		goto err;
	}

	PtAvailableBlockID_Init(*vm);

	return 0;

err:
	__vNand_Deinit(vm);
	return ret;
}

void vNand_Deinit ( VNandManager** vm)
{
	int i;
	PPartition *pt = NULL;

	for(i = 0; i < (*vm)->pt->ptcount; i++){
		pt = &(*vm)->pt->ppt[i];
		if(pt->mode == ONCE_MANAGER) {
			if (i != ((*vm)->pt->ptcount - 1)) {
				ndprint(VNAND_ERROR,"%s: error block table partition position\n", __func__);
				while(1);
			}
			break;
		}

		free_badblock_info(pt);
	}
	__vNand_Deinit(vm);
}
