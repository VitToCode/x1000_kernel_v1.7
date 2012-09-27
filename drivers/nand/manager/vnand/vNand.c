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


#define L4INFOLEN 1024
// PAGENUMBER_PERPAGELIST is the max pages in transmit pagelist
#define PAGENUMBER_PERPAGELIST   (L4INFOLEN + 3*sizeof(int))
/*
struct vnand_operater{
	NandInterface *operator;
	unsigned char *vNand_buf;
	NandMutex mutex;
	void (*start_nand)(int);
	int context;
}v_nand_ops={0};

#define CHECK_OPERATOR(ops)											\
	do{																\
		if(v_nand_ops.operator && !v_nand_ops.operator->i##ops){	\
			ndprint(VNAND_INFO,"i%s isn't registed\n",#ops);		\
			return -1;												\
		}															\
	}while(0)

#define VN_OPERATOR(ops,...)								\
	({														\
		int __ret;											\
		CHECK_OPERATOR(ops);								\
		NandMutex_Lock(&v_nand_ops.mutex);					\
		__ret = v_nand_ops.operator->i##ops (__VA_ARGS__);	\
		NandMutex_Unlock(&v_nand_ops.mutex);				\
		__ret;												\
	})
*/
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
	PageList *npl = NULL;
	PageList *mpl = NULL;
	int *retVal =  vnand->v2pp->retVal;
	int i;
	int flag = 1;

	if (vnand->v2pp->_2kPerPage == 1 || vnand->mode == ONCE_MANAGER){
		clear_pl_retval(pl);
		return pl;
	}
	npl = (PageList *)BuffListManager_getTopNode(vnand->v2pp->blm, sizeof(PageList));
	mpl = npl;
	npl->Bytes = 0;
	singlelist_for_each(pos,&pl->head) {
		pl_node = singlelist_entry(pos,PageList,head);
		if (flag){
			npl->startPageID = pl_node->startPageID / vnand->v2pp->_2kPerPage;
			npl->OffsetBytes = pl_node->OffsetBytes + pl_node->startPageID % vnand->v2pp->_2kPerPage * 2048;
			npl->pData = pl_node->pData;
			npl->retVal = 0;
			for (i=0; i<vnand->v2pp->_2kPerPage; i++)
				*retVal++ = (int)pl_node;
			flag = 0;
		}
		npl->Bytes += pl_node->Bytes;
		if ((pl_node->retVal == 0 ||
			 (npl->Bytes + npl->OffsetBytes) >= vnand->BytePerPage*vnand->v2pp->_2kPerPage) &&
			(pos->next != NULL)) {
			npl = (PageList *)BuffListManager_getNextNode(vnand->v2pp->blm, (void *)npl, sizeof(PageList));
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

	if (vnand->v2pp->_2kPerPage == 1 || vnand->mode == ONCE_MANAGER)
		return;

	retVal = vnand->v2pp->retVal;
	singlelist_for_each(pos,&alig_pl->head){
		newpl = singlelist_entry(pos,PageList,head);
		if (newpl->retVal != 0){
			for(i=0; i< vnand->v2pp->_2kPerPage; i++){
				((PageList*)(*retVal))->retVal = newpl->retVal;
				retVal++;
			}
		}else
			retVal+=vnand->v2pp->_2kPerPage;
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

int __vNand_MultiPageRead (VNandInfo* vNand,PageList* pl ){
	int ret = 0;
	PageList *alig_pl = NULL;
#ifdef STATISTICS_DEBUG
	Get_StartTime(vNand->timebyte,0);
#endif
	alig_pl = vNandPageList_To_NandPageList(vNand,pl);
	ret = VN_OPERATOR(MultiPageRead,vNand->prData,alig_pl);
	Fill_Pl_Retval(vNand,alig_pl);
	if (vNand->v2pp->_2kPerPage != 1 && vNand->mode != ONCE_MANAGER)
		BuffListManager_freeAllList(vNand->v2pp->blm, (void **)&alig_pl, sizeof(PageList));
#ifdef STATISTICS_DEBUG
	Calc_Speed(vNand->timebyte, (void*)pl, 0, 0);
#endif
	return ret;
}

int __vNand_MultiPageWrite (VNandInfo* vNand,PageList* pl ){
	int ret = 0;
	PageList *alig_pl = NULL;

#ifdef STATISTICS_DEBUG
	Get_StartTime(vNand->timebyte,1);
#endif
	alig_pl = vNandPageList_To_NandPageList(vNand,pl);
	ret = VN_OPERATOR(MultiPageWrite,vNand->prData,alig_pl);
	Fill_Pl_Retval(vNand,alig_pl);
	if (vNand->v2pp->_2kPerPage != 1 && vNand->mode != ONCE_MANAGER)
		BuffListManager_freeAllList(vNand->v2pp->blm,(void **)&alig_pl,sizeof(PageList));
#ifdef STATISTICS_DEBUG
	Calc_Speed(vNand->timebyte, (void*)pl, 1, 0);
#endif

	return ret;
}

int __vNand_CopyData (VNandInfo* vNand,PageList* rpl, PageList* wpl ){
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
	if (vNand->v2pp->_2kPerPage != 1){
		BuffListManager_freeAllList(vNand->v2pp->blm,(void **)&alig_rpl,sizeof(PageList));
		BuffListManager_freeAllList(vNand->v2pp->blm,(void **)&alig_wpl,sizeof(PageList));
	}
	return ret;
}

int __vNand_MultiBlockErase (VNandInfo* vNand,BlockList* pl ){
	return VN_OPERATOR(MultiBlockErase,vNand->prData,pl);
}

int __vNand_IsBadBlock (VNandInfo* vNand,int blockid ){
	return VN_OPERATOR(IsBadBlock,vNand->prData,blockid);
}

int vNand_MarkBadBlock (VNandInfo* vNand,unsigned int blockid ){
	return VN_OPERATOR(MarkBadBlock,vNand->prData,blockid);
}

static int vNand_DeInitNand (VNandManager* vNand){
	return  VN_OPERATOR(DeInitNand,vNand);
}
static void virt2phyPage_DeInit(VNandManager *vm)
{
	PPartition *pt = &vm->pt->ppt[0];
	int i;

	BuffListManager_BuffList_DeInit(pt->v2pp->blm);
	for(i=0; i<vm->pt->ptcount; i++){
		pt = &vm->pt->ppt[i];
		if(pt->v2pp->_2kPerPage != 1){
			Nand_VirtualFree(pt->v2pp->retVal);
		}
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
		if(pt->v2pp->_2kPerPage != 1){
			pt->v2pp->retVal = (int*)Nand_VirtualAlloc(PAGENUMBER_PERPAGELIST);
			if(pt->v2pp->retVal == NULL){
				ndprint(VNAND_ERROR,"retVal alloc failed!\n");
				Nand_VirtualFree(pt->v2pp);
				return -1;
			}
		}
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

