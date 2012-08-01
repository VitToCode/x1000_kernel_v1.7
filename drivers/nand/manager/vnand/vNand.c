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

static int vNand_InitNand (VNandManager *vm){
	return VN_OPERATOR(InitNand,vm);
}

int vNand_PageRead (VNandInfo* vNand,int pageid, int offsetbyte, int bytecount, void * data ){
	return VN_OPERATOR(PageRead,vNand->prData,pageid,offsetbyte,bytecount,data);
}

int vNand_PageWrite (VNandInfo* vNand,int pageid, int offsetbyte, int bytecount, void* data ){
	return VN_OPERATOR(PageWrite,vNand->prData,pageid,offsetbyte,bytecount,data);
}

int vNand_MultiPageRead (VNandInfo* vNand,PageList* pl ){
	return VN_OPERATOR(MultiPageRead,vNand->prData,pl);
}

int vNand_MultiPageWrite (VNandInfo* vNand,PageList* pl ){
	return VN_OPERATOR(MultiPageWrite,vNand->prData,pl);
}

int vNand_CopyData (VNandInfo* vNand,PageList* rpl, PageList* wpl ){
	int ret = 0;
	unsigned int offset = 0;
	struct singlelist *pos;
	PageList *pl_node;

	singlelist_for_each(pos, &rpl->head) {
		pl_node = singlelist_entry(pos, PageList, head);
		pl_node->pData = v_nand_ops.vNand_buf + offset;
		offset += pl_node->Bytes;
	}

	offset = 0;
	singlelist_for_each(pos, &wpl->head) {
		pl_node = singlelist_entry(pos, PageList, head);
		pl_node->pData = v_nand_ops.vNand_buf + offset;		
		offset += pl_node->Bytes;
	}

	NandMutex_Lock(&v_nand_ops.mutex);

	ret = v_nand_ops.operator->iMultiPageRead(vNand->prData, rpl);
	if (ret != 0){
		ndprint(VNAND_ERROR,"%s MultiPagerRead failed!\n",__FUNCTION__);
		return -1;
	}
	ret = v_nand_ops.operator->iMultiPageWrite(vNand->prData, wpl);

	NandMutex_Unlock(&v_nand_ops.mutex);

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
	pt->pt_badblock_info = (unsigned int *)Nand_VirtualAlloc(pt->byteperpage + 4);
	if(NULL == pt->pt_badblock_info) {
		ndprint(VNAND_ERROR, "alloc memoey fail func %s line %d \n",
			__FUNCTION__, __LINE__);	
		
		return -1;
	}

	memset(pt->pt_badblock_info, 0xff, pt->byteperpage + 4);

	return 0;
}

static int write_pt_badblock_info(VNandInfo *vnand, unsigned int pageid, PPartition *pt)
{
	return vNand_PageWrite(vnand, pageid, 0, vnand->BytePerPage, pt->pt_badblock_info);
}

static void scan_pt_badblock_info_write_to_nand(VNandManager *vm, int pageid, int size)
{
	int i,j = 0,n, ret = 0;
	unsigned int start_blockno;
	unsigned int end_blockno;
	PPartition *pt;
	VNandInfo vn;

	for(n = 0; n < vm->pt->ptcount;n++){
		pt = &vm->pt->ppt[n];
		if(pt->mode != ONCE_MANAGER){
			pt->pt_badblock_info[0] = n;
			j = 1;
			CONV_PT_VN(pt,&vn);
			start_blockno = vn.startBlockID;
			ndprint(VNAND_DEBUG, "vn.TotalBlocks = %d\n", vn.TotalBlocks);
			end_blockno = vn.startBlockID + vn.TotalBlocks;
			for (i = start_blockno; i < end_blockno; i++) {
				if (vNand_IsBadBlock(&vn, pt->startblockID + i) && j * 4 < size)
					pt->pt_badblock_info[j++] = i;
				else {
					if(j*4 >= size){
						ndprint(VNAND_ERROR,"too many bad block in pt %d\n", n);
						while(1);
					}
				}
			}

			ret = write_pt_badblock_info(&vn, pageid + n, pt);
			if (ret != vn.BytePerPage) {
				ndprint(VNAND_ERROR, "write pt %d badblock info error, ret =%d\n", n, ret);
				while(1);
			}
		}
	}
}

static void read_badblock_info_page(VNandManager *vm)
{
	unsigned int pageid;
	int i, ret;
	VNandInfo vn;
	PPartition *pt = NULL;
	PPartition *firpt = NULL;
	PPartition *lastpt = NULL;
	int startblock = 0, badcnt = 0;

	vm->info.pt_badblock_info = NULL;

   	// find it which partition mode is ONCE_MANAGER
	for(i = 0;i < vm->pt->ptcount;i++){
		pt = &vm->pt->ppt[i];
		if(0 == i) {
			firpt = pt;
		}
		if(pt->mode == ONCE_MANAGER){
			if (i != (vm->pt->ptcount - 1)) {
				ndprint(VNAND_ERROR,"error block table partition position\n");
				while(1);
			}
			break;
		}

		ret = alloc_badblock_info(pt);
		if(ret != 0) {
			ndprint(1,"alloc badblock info memory error func %s line %d \n",
					__FUNCTION__,__LINE__);
			while(1); //??
		}

		lastpt = pt;
	}

	if(i == vm->pt->ptcount){
		ndprint(VNAND_INFO, "INFO: not find badblock partition\n");
		return;
	}

	startblock = pt->startblockID - 1;
	CONV_PT_VN(pt,&vn);

	while(vNand_IsBadBlock(&vn,startblock) && startblock > 0) {
		startblock--;
		badcnt++;
	}

	lastpt->totalblocks -= badcnt;
	lastpt->PageCount -= badcnt * vn.PagePerBlock;
	if ((lastpt->totalblocks <= 0) || (lastpt->PageCount <= 0)) {
		ndprint(VNAND_ERROR, 
				"more bad blcoks,badcnt=%d,totalblocks=%d,PageCount = %d\n", 
				badcnt, lastpt->totalblocks, lastpt->PageCount);
		while(1);
	}

	pageid = startblock * vn.PagePerBlock;
	ndprint(VNAND_INFO, "Find bad block partition in block: %d\n", startblock);
	vNand_PageRead(&vn, pageid, 0, vn.BytePerPage, firpt->pt_badblock_info);

	if (firpt->pt_badblock_info[0] != 0x0) {
		ndprint(VNAND_INFO, "bad block table not creat\n");
		scan_pt_badblock_info_write_to_nand(vm, pageid, vn.BytePerPage - 4);
	}
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

	v_nand_ops.vNand_buf = (unsigned char *)Nand_VirtualAlloc(VNANDCACHESIZE);
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

	ret = vNand_ScanBadBlocks(*vm);
	if(ret != 0){
		ndprint(VNAND_ERROR,"bad block scan failed!\n");
		return -1;
	}
	return 0;
}

void vNand_Deinit ( VNandManager** vm)
{
	vNand_DeInitNand(*vm);
	DeinitNandMutex(&v_nand_ops.mutex);
	Nand_VirtualFree(v_nand_ops.vNand_buf);
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

