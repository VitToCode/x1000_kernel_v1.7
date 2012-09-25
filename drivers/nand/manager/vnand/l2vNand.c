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

void dump_availablebadblock(VNandManager *vm)
{
	int i=0,j=0;
	PPartition *pt=NULL;
	ndprint(VNAND_INFO,"================%s start =============\n",__func__);
	for(i=0;i<vm->pt->ptcount; i++){
		pt = &(vm->pt->ppt[i]);
		if(pt->mode == ONCE_MANAGER) {
                        break;
                }
		ndprint(VNAND_INFO,"\n pt = %p i = %d  bad block info:\n",pt ,i);
		for(j=0;j<64;j++){
			if(j%4==0)
				ndprint(VNAND_INFO,"\n %d: ",j);
			ndprint(VNAND_INFO,"%8d ",pt->pt_badblock_info[j]);
		}
		ndprint(VNAND_INFO,"\navailable block info: \n");
		for(j=0;j<64;j++){
			if(j%4==0)
				ndprint(VNAND_INFO,"\n%d: ",j);
			ndprint(VNAND_INFO,"%8d ",pt->pt_availableblockid[j]);
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
	return vnand->pt_availableblockid[blockid];
}

static int alloc_availableblock_info(PPartition *pt)
{
	pt->pt_availableblockid = (unsigned int *)Nand_ContinueAlloc(pt->totalblocks * sizeof(unsigned int));
	if(NULL == pt->pt_availableblockid) {
		ndprint(VNAND_ERROR, "alloc memoey fail func %s line %d \n",
			__FUNCTION__, __LINE__);
		return -1;
	}

	memset(pt->pt_availableblockid, 0xff, pt->totalblocks * sizeof(int));
	return 0;
}

static void free_availableblock_info(PPartition *pt)
{
	Nand_ContinueFree(pt->pt_availableblockid);
}

static  void PtAvailableBlockID_Init(VNandManager *vm)
{
	int badblock_number = 0;
	int blockid = 0;
	int pos,i=0,j=0;
	PPartition *pt = NULL;

	for(i=0; i<vm->pt->ptcount; i++){
		pt = &vm->pt->ppt[i];
		if(pt->mode == ONCE_MANAGER) {
                        break;
                }
		blockid = 0;
		badblock_number = 0;
		alloc_availableblock_info(pt);
		for(j=0; j< pt->byteperpage * BADBLOCKINFOSIZE/sizeof(unsigned int); j++){
			if(pt->pt_badblock_info[j]==0xffffffff)
				break;
			else
				badblock_number++;
		}
		for(pos=0; pos < pt->totalblocks-badblock_number; pos++){
			for(j=0; j<badblock_number; j++){
				if(pt->pt_badblock_info[j] == blockid){
					blockid++;
				}
			}
			pt->pt_availableblockid[pos] = blockid;
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

	UpdatePageList(vNand, pl);
	ret = __vNand_MultiPageWrite(vNand, pl);
	GoBackPageList(pl);

	return ret;
}

int vNand_MultiPageRead(VNandInfo* vNand, PageList* pl){
	int ret = 0;

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
/*
  The following functions is for partition manager
*/

int vNand_Init (VNandManager** vm)
{
	int ret;

	ret = __vNand_Init(vm);
	PtAvailableBlockID_Init(*vm);

	return ret;
}

void vNand_Deinit ( VNandManager** vm)
{
	int i;
	PPartition *pt = NULL;

	for(i = 0; i < (*vm)->pt->ptcount; i++){
		pt = &(*vm)->pt->ppt[i];
		if(pt->mode == ONCE_MANAGER) {
                        break;
                }
		free_availableblock_info(pt);
	}
	__vNand_Deinit(vm);

}
