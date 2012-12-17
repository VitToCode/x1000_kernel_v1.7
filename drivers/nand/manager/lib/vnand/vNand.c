#include "os/clib.h"
#include "pagelist.h"
#include "vnandinfo.h"
#include "blocklist.h"
#include "context.h"
#include "os/NandAlloc.h"
#include "os/NandSemaphore.h"
#include "nanddebug.h"
#include "nandinterface.h"
#include "vNand.h"
#include "timeinterface.h"

static void dump_pagelist(PageList *pl)
{
	struct singlelist *pos;
	PageList *pl_node;
	ndprint(VNAND_INFO,"^^^^^^^ dump pl start ^^^^^^^\n");
	singlelist_for_each(pos,&pl->head){
		pl_node= singlelist_entry(pos,PageList,head);
		ndprint(VNAND_INFO,"pageid:%d pData:%p offset:%d bytes:%d ret:%d\n",pl_node->startPageID,pl_node->pData,pl_node->OffsetBytes,pl_node->Bytes,pl_node->retVal);
		if(pl_node->retVal == -6){
			ndprint(VNAND_INFO,"pData:%08x \n",((unsigned int *)(pl_node->pData))[0]);
		}
	}
	ndprint(VNAND_INFO,"^^^^^^^ dump pl end ^^^^^^^\n");
}

/*
  The following functions mutually exclusive call nand driver
*/
struct vnand_operater v_nand_ops={0};

static void vNandPage_To_NandPage(VNandInfo *vnand, int *pageid, int *offsetbyte)
{
	if (vnand->v2pp->_2kPerPage == 1)
		return;

	*offsetbyte += *pageid % vnand->v2pp->_2kPerPage * 2048;
	*pageid /= vnand->v2pp->_2kPerPage;
}

static void clear_pl_retval(PageList *pl)
{
	struct singlelist *pos = NULL;
	PageList *pl_node = NULL;

	singlelist_for_each(pos,&(pl->head)){
		pl_node = singlelist_entry(pos,PageList,head);
		pl_node->retVal = 0;
	}
}

static PageList* vNandPageList_To_NandPageList(VNandInfo *vnand, PageList *pl)
{
	struct singlelist *pos = NULL;
	PageList *pl_node = NULL;
	PageList *pl_next = NULL;
	PageList *npl = NULL;
	PageList *mpl = NULL;
	int bytecnt = 0;

	if (vnand->v2pp->_2kPerPage == 1 || vnand->mode != ZONE_MANAGER){
		clear_pl_retval(pl);
		return pl;
	}
	npl = (PageList *)BuffListManager_getTopNode(vnand->v2pp->blm, sizeof(PageList));
	mpl = npl;
	npl->Bytes = 0;
	while(1){
		pos = &pl->head;
		pl_node = singlelist_entry(pos,PageList,head);
		if(pos->next != NULL){
			pl_next = singlelist_entry(pos->next,PageList,head);
			pl = pl_next;
		}
		if(npl->Bytes == 0){
			npl->startPageID = pl_node->startPageID / vnand->v2pp->_2kPerPage;
			npl->OffsetBytes = pl_node->OffsetBytes + pl_node->startPageID % vnand->v2pp->_2kPerPage * 2048;
			npl->pData = pl_node->pData;
			npl->retVal = 0;
			bytecnt += pl_node->OffsetBytes;
		}
		npl->Bytes += pl_node->Bytes;
		bytecnt += pl_node->Bytes;

		if ((pl_node->retVal == 0 || npl->Bytes >= vnand->BytePerPage*vnand->v2pp->_2kPerPage ||
			bytecnt >= vnand->BytePerPage*vnand->v2pp->_2kPerPage ||
			 (pl_next && pl_next->startPageID-pl_node->startPageID != 1) ||
			 (pl_next && pl_next->startPageID % vnand->v2pp->_2kPerPage == 0)) &&
			(pos->next != NULL)){
			npl = (PageList *)BuffListManager_getNextNode(vnand->v2pp->blm, (void *)npl, sizeof(PageList));
			npl->Bytes = 0;
			bytecnt = 0;
			pl_node->retVal = 0; //set break node's retVal is 0,other is 1.
		}
		if(pos->next == NULL)
			break;
	}

	return mpl;
}

static void Fill_Pl_Retval(VNandInfo *vnand, PageList *alig_pl, PageList *pl)
{
	struct singlelist *pos;
	struct singlelist *alig_pos;
	int cnt = 0;
	PageList *pl_node = NULL;

	if (vnand->v2pp->_2kPerPage == 1 || vnand->mode != ZONE_MANAGER)
		return;

	if (alig_pl == NULL || pl == NULL){
		ndprint(VNAND_INFO,"WARNING: Pagelist is null !!\n");
		return;
	}

	singlelist_for_each(pos,&pl->head){
		pl_node = singlelist_entry(pos,PageList,head);
		if (pl_node->retVal == 1)
			cnt ++;
		if ((pl_node->retVal == 0) ||
		   (pl_node->retVal == 1 && cnt >= vnand->v2pp->_2kPerPage)) {
			pl_node->retVal = alig_pl->retVal;
			alig_pos = alig_pl->head.next;
			if(alig_pos == NULL){
				if(pos->next == NULL)
					break;
				else
					ndprint(VNAND_ERROR,"ERROR:%s LINE:%d\n",__func__,__LINE__);
			}
			alig_pl = singlelist_entry(alig_pos,PageList,head);
			cnt = 0;
			continue;
		}
		pl_node->retVal = alig_pl->retVal;
	}
}

static int vNand_InitNand (VNandManager *vm){
	int ret;
	NandMutex_Lock(&v_nand_ops.mutex);
	ret = VN_OPERATOR(InitNand,vm);
	NandMutex_Unlock(&v_nand_ops.mutex);
	return ret;
}

int vNand_PageRead (VNandInfo* vNand,int pageid, int offsetbyte, int bytecount, void * data ){
	int ret;
	NandMutex_Lock(&v_nand_ops.mutex);
	vNandPage_To_NandPage(vNand,&pageid,&offsetbyte);
	ret = VN_OPERATOR(PageRead,vNand->prData,pageid,offsetbyte,bytecount,data);
	NandMutex_Unlock(&v_nand_ops.mutex);
	return ret;
}

int vNand_PageWrite (VNandInfo* vNand,int pageid, int offsetbyte, int bytecount, void* data ){
	int ret;
	NandMutex_Lock(&v_nand_ops.mutex);
	vNandPage_To_NandPage(vNand,&pageid,&offsetbyte);
	ret = VN_OPERATOR(PageWrite,vNand->prData,pageid,offsetbyte,bytecount,data);
	NandMutex_Unlock(&v_nand_ops.mutex);
	return ret;
}

int __vNand_MultiPageRead (VNandInfo* vNand,PageList* pl ){
	int ret = 0;
	PageList *alig_pl = NULL;

	NandMutex_Lock(&v_nand_ops.mutex);
	alig_pl = vNandPageList_To_NandPageList(vNand,pl);
#ifdef STATISTICS_DEBUG
	Get_StartTime(vNand->timebyte,0);
#endif
	ret = VN_OPERATOR(MultiPageRead,vNand->prData,alig_pl);
#ifdef STATISTICS_DEBUG
	Calc_Speed(vNand->timebyte, (void*)pl, 0, 0);
#endif
	Fill_Pl_Retval(vNand,alig_pl,pl);
	if (vNand->v2pp->_2kPerPage != 1 && vNand->mode == ZONE_MANAGER)
		BuffListManager_freeAllList(vNand->v2pp->blm, (void **)&alig_pl, sizeof(PageList));
	NandMutex_Unlock(&v_nand_ops.mutex);
	return ret;
}

int __vNand_MultiPageWrite (VNandInfo* vNand,PageList* pl ){
	int ret = 0;
	PageList *alig_pl = NULL;

	NandMutex_Lock(&v_nand_ops.mutex);
	alig_pl = vNandPageList_To_NandPageList(vNand,pl);
#ifdef STATISTICS_DEBUG
	Get_StartTime(vNand->timebyte,1);
#endif
	ret = VN_OPERATOR(MultiPageWrite,vNand->prData,alig_pl);
#if 1
	if(ISERROR(ret))
		dump_pagelist(alig_pl);
#endif
#ifdef STATISTICS_DEBUG
	Calc_Speed(vNand->timebyte, (void*)pl, 1, 0);
#endif
	Fill_Pl_Retval(vNand,alig_pl,pl);
	if (vNand->v2pp->_2kPerPage != 1 && vNand->mode == ZONE_MANAGER)
		BuffListManager_freeAllList(vNand->v2pp->blm,(void **)&alig_pl,sizeof(PageList));
	NandMutex_Unlock(&v_nand_ops.mutex);

	return ret;
}

int __vNand_CopyData (VNandInfo* vNand,PageList* rpl, PageList* wpl){
	int ret = 0;
	unsigned int read_offset = 0;
	unsigned int write_offset = 0;
	struct singlelist *pos;
	PageList *pl_node ;
	PageList *pagelist = NULL;
	PageList *read_follow_pagelist ;
	PageList *write_follow_pagelist ;
	PageList *read_pagelist ;
	PageList *write_pagelist ;
	PageList *alig_rpl ;
	PageList *alig_wpl ;

	NandMutex_Lock(&v_nand_ops.mutex);
	read_follow_pagelist = rpl;
	write_follow_pagelist = wpl;
	while (1) {
		if (read_follow_pagelist == NULL || write_follow_pagelist == NULL)
			break;

		read_pagelist = read_follow_pagelist;
		write_pagelist = write_follow_pagelist;
		if(write_follow_pagelist->pData)
			write_follow_pagelist = singlelist_entry(write_follow_pagelist->head.next, PageList, head);
		read_offset = 0;
		write_offset = 0;
		singlelist_for_each(pos, &read_follow_pagelist->head) {
			pl_node = singlelist_entry(pos, PageList, head);
			pl_node->pData = v_nand_ops.vNand_buf + read_offset;
			read_offset += pl_node->Bytes;
			if (read_offset > VNANDCACHESIZE)
				break;
			pagelist = pl_node;
		}
		if (pagelist->head.next) {
			read_follow_pagelist = singlelist_entry(pagelist->head.next,PageList,head);
			pagelist->head.next = NULL;
		}
		else
			read_follow_pagelist = NULL;

		singlelist_for_each(pos, &write_follow_pagelist->head){
			pl_node = singlelist_entry(pos, PageList, head);
			pl_node->pData = v_nand_ops.vNand_buf + write_offset;
			write_offset += pl_node->Bytes;
			if (write_offset > VNANDCACHESIZE)
				break;
			pagelist = pl_node;
		}
		//ndprint(VNAND_INFO,"vnand: readoffset = %d writeoffset = %d\n",read_offset,write_offset);
		if(read_offset != write_offset){
			ndprint(VNAND_ERROR,"VNAND ERROR: readoffset = %d write_offset = %d \n",read_offset,write_offset);
			while(1);
		}
		if (pagelist->head.next) {
			write_follow_pagelist = singlelist_entry(pagelist->head.next,PageList,head);
			pagelist->head.next = NULL;
		}
		else
			write_follow_pagelist = NULL;

		alig_rpl = vNandPageList_To_NandPageList(vNand,read_pagelist);
		ret = v_nand_ops.operator->iMultiPageRead(vNand->prData, alig_rpl);
		Fill_Pl_Retval(vNand,alig_rpl,read_pagelist);
		if (ret != 0){
			ndprint(VNAND_ERROR,"MultiPagerRead failed! func: %s line: %d ret=%d\n",
					__FUNCTION__, __LINE__,ret);
			//goto exit;
		}
		alig_wpl = vNandPageList_To_NandPageList(vNand,write_pagelist);
		ret = v_nand_ops.operator->iMultiPageWrite(vNand->prData, alig_wpl);
		Fill_Pl_Retval(vNand,alig_wpl,write_pagelist);
		if (ret != 0) {
			ndprint(VNAND_ERROR,"MultiPageWrite failed! func: %s line: %d ret=%d\n",
					__FUNCTION__, __LINE__,ret);
			goto exit;
		}
		if (vNand->v2pp->_2kPerPage != 1 && vNand->mode == ZONE_MANAGER){
			BuffListManager_freeAllList(vNand->v2pp->blm,(void **)&alig_rpl,sizeof(PageList));
			BuffListManager_freeAllList(vNand->v2pp->blm,(void **)&alig_wpl,sizeof(PageList));
		}
	}

exit:
	NandMutex_Unlock(&v_nand_ops.mutex);
	return ret;
}

int __vNand_MultiBlockErase (VNandInfo* vNand,BlockList* pl ){
	int ret;
	NandMutex_Lock(&v_nand_ops.mutex);
	ret = VN_OPERATOR(MultiBlockErase,vNand->prData,pl);
	NandMutex_Unlock(&v_nand_ops.mutex);
	return ret;
}

int __vNand_IsBadBlock (VNandInfo* vNand,int blockid ){
	int ret;
	NandMutex_Lock(&v_nand_ops.mutex);
	ret = VN_OPERATOR(IsBadBlock,vNand->prData,blockid);
	NandMutex_Unlock(&v_nand_ops.mutex);
	return ret;
}

int __vNand_MarkBadBlock (VNandInfo* vNand,int blockid ){
	int ret;
	NandMutex_Lock(&v_nand_ops.mutex);
	ret = VN_OPERATOR(MarkBadBlock,vNand->prData,blockid);
	NandMutex_Unlock(&v_nand_ops.mutex);
	return ret;
}

static int vNand_DeInitNand (VNandManager* vNand){
	int ret;
	NandMutex_Lock(&v_nand_ops.mutex);
	ret = VN_OPERATOR(DeInitNand,vNand);
	NandMutex_Unlock(&v_nand_ops.mutex);
	return ret;
}
static void virt2phyPage_DeInit(VNandManager *vm)
{
	PPartition *pt = &vm->pt->ppt[0];
	int i;

	BuffListManager_BuffList_DeInit(pt->v2pp->blm);
	for(i=0; i<vm->pt->ptcount; i++){
		pt = &vm->pt->ppt[i];
		Nand_VirtualFree(pt->v2pp);
	}
}

static int virt2phyPage_Init(VNandManager *vm)
{
	int i;
	PPartition *pt = NULL;
	int blm = BuffListManager_BuffList_Init();

	for(i=0; i<vm->pt->ptcount; i++){
		pt = &vm->pt->ppt[i];
		pt->v2pp = (struct virt2phy_page *)Nand_VirtualAlloc(sizeof(struct virt2phy_page));
		if(pt->v2pp == NULL){
			ndprint(VNAND_ERROR,"v2pp alloc failed!\n");
			return -1;
		}
		pt->v2pp->_2kPerPage = pt->byteperpage / 2048; // 2048 = 2k ,virtual page size is 2k
		pt->v2pp->blm = blm;
	}
	return 0;
}
/*
  The following functions is for partition manager
*/

int __vNand_Init (VNandManager** vm)
{
	int ret = 0;

	if(*vm){
		ndprint(VNAND_ERROR,"*vm should be null!\n");
		return -1;
	}

	*vm = Nand_VirtualAlloc(sizeof(VNandManager));
	if(*vm == NULL){
		ndprint(VNAND_ERROR,"*vm alloc failed!\n");
		goto err0;
	}

	v_nand_ops.vNand_buf = (unsigned char *)Nand_ContinueAlloc(VNANDCACHESIZE);
	if(v_nand_ops.vNand_buf == NULL){
		ndprint(VNAND_ERROR,"alloc bad block info failed!\n");
		goto err1;
	}

	InitNandMutex(&v_nand_ops.mutex);
	ret = vNand_InitNand(*vm);
	if (ret != 0) {
		ndprint(VNAND_ERROR,"driver init failed!\n");
		goto err1;
	}

	ret = virt2phyPage_Init(*vm);
    if (ret != 0)
		goto err1;

	return 0;

err1:
	Nand_VirtualFree(v_nand_ops.vNand_buf);
err0:
	Nand_VirtualFree(*vm);
	return ret;
}

void __vNand_Deinit ( VNandManager** vm)
{
	virt2phyPage_DeInit(*vm);
	vNand_DeInitNand(*vm);
	DeinitNandMutex(&v_nand_ops.mutex);
	Nand_ContinueFree(v_nand_ops.vNand_buf);
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

int vNand_DirectPageRead(PPartition *pt, int pageid, int off_t, int bytes, void * data)
{
	int ret;
	NandMutex_Lock(&v_nand_ops.mutex);
	ret = VN_OPERATOR(PageRead, pt, pageid, off_t, bytes, data);
	NandMutex_Unlock(&v_nand_ops.mutex);
	return ret;
}

int vNand_DirectPageWrite(PPartition *pt, int pageid, int off_t, int bytes, void* data)
{
	int ret;
	NandMutex_Lock(&v_nand_ops.mutex);
	ret = VN_OPERATOR(PageWrite, pt, pageid, off_t, bytes, data);
	NandMutex_Unlock(&v_nand_ops.mutex);
	return ret;
}

int vNand_DirectMultiBlockErase(PPartition *pt, BlockList* bl)
{
	int ret;
	NandMutex_Lock(&v_nand_ops.mutex);
	ret = VN_OPERATOR(MultiBlockErase, pt, bl);
	NandMutex_Unlock(&v_nand_ops.mutex);
	return ret;
}

int vNand_DirectIsBadBlock(PPartition *pt, int blockid)
{
	int ret;
	NandMutex_Lock(&v_nand_ops.mutex);
	ret = VN_OPERATOR(IsBadBlock, pt, blockid);
	NandMutex_Unlock(&v_nand_ops.mutex);
	return ret;
}

int vNand_DirectMarkBadBlock(PPartition *pt, int blockid)
{
	int ret;
	NandMutex_Lock(&v_nand_ops.mutex);
	ret = VN_OPERATOR(MarkBadBlock, pt, blockid);
	NandMutex_Unlock(&v_nand_ops.mutex);
	return ret;
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
