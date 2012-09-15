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

struct vnand_operater{
	NandInterface *operator;
	unsigned char *vNand_buf;
	NandMutex mutex;
	void (*start_nand)(int);
	int context;
}v_nand_ops={0};

#define CHECK_OPERATOR(ops)						\
	do{								\
		if(v_nand_ops.operator && !v_nand_ops.operator->i##ops){ \
			ndprint(VNAND_INFO,"i%s isn't registed\n",#ops); \
			return -1;					\
		}							\
	}while(0)

#define VN_OPERATOR(ops,...)						\
	({								\
		int __ret;						\
		CHECK_OPERATOR(ops);					\
		NandMutex_Lock(&v_nand_ops.mutex);			\
		__ret = v_nand_ops.operator->i##ops (__VA_ARGS__);	\
			NandMutex_Unlock(&v_nand_ops.mutex);		\
			__ret;						\
	})

/*
  The following functions mutually exclusive call nand driver
*/

static void vNandPage_To_NandPage(VNandInfo *vnand, int *pageid, int *offsetbyte)
{
	if (vnand->_2kPerPage == 1)
		return;

	*offsetbyte += *pageid % vnand->_2kPerPage * 2048;
	*pageid /= vnand->_2kPerPage;
}

static PageList* vNandPageList_To_NandPageList(VNandInfo *vnand, PageList *pl)
{
	struct singlelist *pos = NULL;
	PageList *pl_node = NULL;
	PageList *npl = NULL;
	PageList *mpl = NULL;
	int *retVal =  vnand->retVal;
	int i;
	int flag = 1;

	if (vnand->_2kPerPage == 1)
		return pl;
	npl = (PageList *)BuffListManager_getTopNode(vnand->blm, sizeof(PageList));
	mpl = npl;
	npl->Bytes = 0;
	singlelist_for_each(pos,&pl->head) {
		pl_node = singlelist_entry(pos,PageList,head);
		if (flag){
			npl->startPageID = pl_node->startPageID / vnand->_2kPerPage;
			npl->OffsetBytes = pl_node->OffsetBytes + pl_node->startPageID % vnand->_2kPerPage * 2048;
			npl->pData = pl_node->pData;
			for (i=0; i<vnand->_2kPerPage; i++)
				*retVal++ = (int)pl_node;
			flag = 0;
		}
		npl->Bytes += pl_node->Bytes;
		if ((pl_node->retVal == 0 ||
			 (npl->Bytes + npl->OffsetBytes) >= vnand->BytePerPage*vnand->_2kPerPage) &&
			(pos->next != NULL)) {
			npl = (PageList *)BuffListManager_getNextNode(vnand->blm, (void *)npl, sizeof(PageList));
			npl->Bytes = 0;
			pl_node->retVal = 0;
			flag = 1;
		}
	}
	return mpl;
}

static void Fill_Pl_Retval(VNandInfo *vnand, PageList *alig_pl)
{
	struct singlelist *pos;
	PageList *newpl = NULL;
	int *retVal = NULL;
	int i;
	if (vnand->_2kPerPage == 1)
		return;
	retVal = vnand->retVal;
	singlelist_for_each(pos,&alig_pl->head){
		newpl = singlelist_entry(pos,PageList,head);
		if (newpl->retVal != 0){
			for(i=0; i< vnand->_2kPerPage; i++){
				((PageList*)(*retVal))->retVal = newpl->retVal;
				retVal++;
			}
		}else
			retVal+=vnand->_2kPerPage;
	}
}

static int vNand_InitNand (VNandManager *vm){
	return VN_OPERATOR(InitNand,vm);
}

int vNand_PageRead (VNandInfo* vNand,int pageid, int offsetbyte, int bytecount, void * data ){
	vNandPage_To_NandPage(vNand,&pageid,&offsetbyte);
	return VN_OPERATOR(PageRead,vNand->prData,pageid,offsetbyte,bytecount,data);
}

int vNand_PageWrite (VNandInfo* vNand,int pageid, int offsetbyte, int bytecount, void* data ){
	vNandPage_To_NandPage(vNand,&pageid,&offsetbyte);
	return VN_OPERATOR(PageWrite,vNand->prData,pageid,offsetbyte,bytecount,data);
}

int vNand_MultiPageRead (VNandInfo* vNand,PageList* pl ){
	int ret = 0;
	PageList *alig_pl = NULL;
#ifdef STATISTICS_DEBUG
	Get_StartTime(vNand->timebyte,0);
#endif
	alig_pl = vNandPageList_To_NandPageList(vNand,pl);
	vNand->align_rpl = alig_pl;
	ret = VN_OPERATOR(MultiPageRead,vNand->prData,alig_pl);
	Fill_Pl_Retval(vNand,alig_pl);
#ifdef STATISTICS_DEBUG
	Calc_Speed(vNand->timebyte, (void*)pl, 0, 0);
#endif
	return ret;
}

static int MultiPageRead (VNandInfo* vNand,PageList* pl )
{
	return VN_OPERATOR(MultiPageRead,vNand->prData,pl);
}

int vNand_MultiPageWrite (VNandInfo* vNand,PageList* pl ){
	int ret = 0;
	PageList *alig_pl = NULL;

#ifdef STATISTICS_DEBUG
	Get_StartTime(vNand->timebyte,1);
#endif
	alig_pl = vNandPageList_To_NandPageList(vNand,pl);
	ret = VN_OPERATOR(MultiPageWrite,vNand->prData,alig_pl);
	Fill_Pl_Retval(vNand,alig_pl);
	if (vNand->_2kPerPage != 1)
		BuffListManager_freeAllList(vNand->blm,(void **)&alig_pl,sizeof(PageList));
#ifdef STATISTICS_DEBUG
	Calc_Speed(vNand->timebyte, (void*)pl, 1, 0);
#endif

	return ret;
}

static int MultiPageWrite (VNandInfo* vNand,PageList* pl )
{
	return VN_OPERATOR(MultiPageWrite,vNand->prData,pl);
}

int vNand_CopyData (VNandInfo* vNand,PageList* rpl, PageList* wpl ){
	int ret = 0;
	unsigned int offset = 0;
	struct singlelist *pos = NULL;
	PageList *pl_node = NULL;
	PageList *pagelist = NULL;
	PageList *read_follow_pagelist = NULL;
	PageList *write_follow_pagelist = NULL;
	PageList *read_pagelist = NULL;
	PageList *write_pagelist = NULL;
	PageList *alig_rpl = NULL;
	PageList *alig_wpl = NULL;

	alig_rpl = vNandPageList_To_NandPageList(vNand,rpl);
	alig_wpl = vNandPageList_To_NandPageList(vNand,wpl);

	read_follow_pagelist = alig_rpl;
	write_follow_pagelist = alig_wpl;
	NandMutex_Lock(&v_nand_ops.mutex);
	while (1) {
		if (read_follow_pagelist == NULL || write_follow_pagelist == NULL)
			break;

		read_pagelist = read_follow_pagelist;
		write_pagelist = write_follow_pagelist;
		offset = 0;
		singlelist_for_each(pos, &read_follow_pagelist->head) {
			pl_node = singlelist_entry(pos, PageList, head);
			pl_node->pData = v_nand_ops.vNand_buf + offset;
			offset += pl_node->Bytes;
			if (offset > VNANDCACHESIZE)
				break;
			pagelist = pl_node;
		}
		if (pagelist->head.next) {
			read_follow_pagelist = singlelist_entry(pagelist->head.next,PageList,head);
			pagelist->head.next = NULL;
		}
		else
			read_follow_pagelist = NULL;

		offset = 0;
		singlelist_for_each(pos, &write_follow_pagelist->head) {
			pl_node = singlelist_entry(pos, PageList, head);
			pl_node->pData = v_nand_ops.vNand_buf + offset;
			offset += pl_node->Bytes;
			if (offset > VNANDCACHESIZE)
				break;
			pagelist = pl_node;
		}
		if (pagelist->head.next) {
			write_follow_pagelist = singlelist_entry(pagelist->head.next,PageList,head);
			pagelist->head.next = NULL;
		}
		else
			write_follow_pagelist = NULL;

		ret = v_nand_ops.operator->iMultiPageRead(vNand->prData, read_pagelist);
		if (ret != 0){
			ndprint(VNAND_ERROR,"MultiPagerRead failed! func: %s line: %d \n",
				__FUNCTION__, __LINE__);
			goto exit;
		}

		ret = v_nand_ops.operator->iMultiPageWrite(vNand->prData, write_pagelist);
		if (ret != 0) {
			ndprint(VNAND_ERROR,"MultiPageWrite failed! func: %s line: %d \n",
				__FUNCTION__, __LINE__);
			goto exit;
		}
	}

exit:
	NandMutex_Unlock(&v_nand_ops.mutex);
	BuffListManager_freeAllList(vNand->blm,(void **)&alig_rpl,sizeof(PageList));
	BuffListManager_freeAllList(vNand->blm,(void **)&alig_wpl,sizeof(PageList));
	return ret;
}

int vNand_MultiBlockErase (VNandInfo* vNand,BlockList* pl ){
	return VN_OPERATOR(MultiBlockErase,vNand->prData,pl);
}

int vNand_IsBadBlock (VNandInfo* vNand,int blockid ){
	return VN_OPERATOR(IsBadBlock,vNand->prData,blockid);
}

int vNand_MarkBadBlock (VNandInfo* vNand,unsigned int blockid ){
	return VN_OPERATOR(MarkBadBlock,vNand->prData,blockid);
}

static int vNand_DeInitNand (VNandManager* vNand){
	return  VN_OPERATOR(DeInitNand,vNand);
}

static int alloc_badblock_info(PPartition *pt)
{
	pt->pt_badblock_info = (unsigned int *)Nand_ContinueAlloc(pt->byteperpage * BADBLOCKINFOSIZE);
	if(NULL == pt->pt_badblock_info) {
		ndprint(VNAND_ERROR, "alloc memoey fail func %s line %d \n",
			__FUNCTION__, __LINE__);

		return -1;
	}

	memset(pt->pt_badblock_info, 0xff, pt->byteperpage * BADBLOCKINFOSIZE);

	return 0;
}

static void free_badblock_info(PPartition *pt)
{
	Nand_ContinueFree(pt->pt_badblock_info);
}

static int write_pt_badblock_info(VNandInfo *vnand, PageList *pl)
{
	return MultiPageWrite(vnand,pl);
}

static void scan_pt_badblock_info_write_to_nand(PPartition *pt, int pt_id, VNandInfo *evn, PageList *pl)
{
	int i,j = 0, ret = 0;
	unsigned int start_blockno;
	unsigned int end_blockno;
	VNandInfo vn;
	int size = pt->byteperpage * BADBLOCKINFOSIZE;

	if(pt->mode != ONCE_MANAGER){
		CONV_PT_VN(pt,&vn);
		start_blockno = vn.startBlockID;
		ndprint(VNAND_DEBUG, "vn.TotalBlocks = %d\n", vn.TotalBlocks);
		end_blockno = vn.startBlockID + vn.TotalBlocks;
		for (i = start_blockno; i < end_blockno; i++) {
			if (vNand_IsBadBlock(&vn, i) && j * 4 < size)
				pt->pt_badblock_info[j++] = i;
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
		}
		else
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
		pl_node->pData = (void *)((char *)pt->pt_badblock_info + offset);
		startpageid++;
		offset += pt->byteperpage;
	}
}

static void read_badblock_info_page(VNandManager *vm)
{
	int i, ret;
	int blmid;
	VNandInfo error_vn;
	PageList *pl = NULL;
	PPartition *pt = NULL;
	PPartition *lastpt = NULL;
	int startblock = 0, badcnt = 0,blkcnt = 0;
	vm->info.pt_badblock_info = NULL;

	if ((vm->pt->ptcount - 1) * BADBLOCKINFOSIZE > vm->info.PagePerBlock) {
		ndprint(VNAND_ERROR,"ERROR: BADBLOCKINFOSIZE = %d is too large,vnand->PagePerBlock = %d func %s line %d \n",
				BADBLOCKINFOSIZE,vm->info.PagePerBlock,__FUNCTION__,__LINE__);
		while(1);
	}

   	// find it which partition mode is ONCE_MANAGER
	for(i = 0;i < vm->pt->ptcount;i++){
		pt = &vm->pt->ppt[i];
		if(pt->mode == ONCE_MANAGER){
			if (i != (vm->pt->ptcount - 1)) {
				ndprint(VNAND_ERROR,"error block table partition position\n");
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
		if(vNand_IsBadBlock(&error_vn,startblock)) {
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
	lastpt->PageCount -= lastpt->totalblocks * pt->pageperblock;

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

	for(i = 0;i < vm->pt->ptcount - 1;i++){
		pt = &vm->pt->ppt[i];
		fill_pagelist(pt,pl,i);
		ret = MultiPageRead(&error_vn,pl);
		if (ret != 0 && ISNOWRITE(pl->retVal)) {
			ndprint(VNAND_INFO, "pt[%d] bad block table not creat\n", i);
			scan_pt_badblock_info_write_to_nand(pt,i,&error_vn,pl);
		}
	}
	BuffListManager_freeAllList(blmid,(void **)&pl,sizeof(PageList));
	BuffListManager_BuffList_DeInit(blmid);
}

int vNand_ScanBadBlocks (VNandManager* vm)
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
	int ret = 0;

	if(*vm){
		ndprint(VNAND_ERROR,"*vm should be null!\n");
		return -1;
	}

	*vm = Nand_VirtualAlloc(sizeof(VNandManager));
	if(*vm == NULL){
		ndprint(VNAND_ERROR,"*vm alloc failed!\n");
		return -1;
	}

	v_nand_ops.vNand_buf = (unsigned char *)Nand_ContinueAlloc(VNANDCACHESIZE);
	if(v_nand_ops.vNand_buf == NULL){
		ndprint(VNAND_ERROR,"alloc bad block info failed!\n");
		return -1;
	}

	InitNandMutex(&v_nand_ops.mutex);
	ret = vNand_InitNand(*vm);
	if (ret != 0) {
		ndprint(VNAND_ERROR,"driver init failed!\n");
		return -1;
	}
	(*vm)->info._2kPerPage = (*vm)->info.BytePerPage / 2048;

	ret = vNand_ScanBadBlocks(*vm);
	if(ret != 0){
		ndprint(VNAND_ERROR,"bad block scan failed!\n");
		return -1;
	}
	return 0;
}

void vNand_Deinit ( VNandManager** vm)
{
	int i;
	PPartition *pt = NULL;

	vNand_DeInitNand(*vm);
	DeinitNandMutex(&v_nand_ops.mutex);
	Nand_ContinueFree(v_nand_ops.vNand_buf);
	for(i = 0; i < (*vm)->pt->ptcount; i++){
		pt = &(*vm)->pt->ppt[i];
		free_badblock_info(pt);
	}
	Nand_VirtualFree(*vm);
	*vm = NULL;
}


void Register_StartNand(void *start,int context){
	v_nand_ops.start_nand = start;
	v_nand_ops.context = context;
}

void Register_NandDriver(NandInterface *ni){
	v_nand_ops.operator = ni;
	v_nand_ops.start_nand(v_nand_ops.context);
}

#ifdef DEBUG
void test_operator_vnand(VNandInfo *vnandptr)
{
	ndprint(VNAND_DEBUG,"iInit %p \n",v_nand_ops.operator->iInitNand);
	ndprint(VNAND_DEBUG,"iPageRaed %p \n",v_nand_ops.operator->iPageRead);
	ndprint(VNAND_DEBUG,"iPagewrite %p \n",v_nand_ops.operator->iPageWrite);
	ndprint(VNAND_DEBUG,"iMultiPageRead %p\n",v_nand_ops.operator->iMultiPageRead);
	ndprint(VNAND_DEBUG,"iMultiPageWrite %p\n",v_nand_ops.operator->iMultiPageWrite);
	ndprint(VNAND_DEBUG,"iMultiBlockErase %p\n",v_nand_ops.operator->iMultiBlockErase);
	ndprint(VNAND_DEBUG,"iMarkBadBlock %p \n",v_nand_ops.operator->iMarkBadBlock);

}
#endif

