#include "clib.h"
#include "simpleblockmanager.h"
#include "bufflistmanager.h"
#include "nanddebug.h"
#include "vNand.h"
#include "l2vNand.h"
#include "context.h"
#include "nandmanagerinterface.h"
#include "partitioninterface.h"

static BuffListManager *Blm;

/**
 *	get_unit_count_from_sl  - calculate the unit count
 *
 *	@sl_node: the sector list node
 *	@unit: the unit size in sector
 */
static inline int get_unit_count_from_sl(SectorList *snode, int unit)
{
	int sectors = (snode->startSector % unit) + snode->sectorCount;

	return (sectors + unit -1) / unit;
}

static inline int is_badblock(VNandInfo* vNand, int blockid) {
	int ret;

	while (1) {
		ret = vNand_IsBadBlock(vNand, blockid);
		if (ret != TIMEOUT)
			break;

		ndprint(SIGBLOCK_ERROR, "%s, warning, timeout!\n", __func__);
	}

	return ret;
}

static inline int mark_bakblock(int blm, VNandInfo* vNand, int blockid)
{
	BlockList *bl_top;

	bl_top = (BlockList *)BuffListManager_getTopNode(blm, sizeof(BlockList));
	if (!bl_top)
		ndprint(SIGBLOCK_ERROR, "%s, line:%d, alloc BlockList error!\n\n", __func__, __LINE__);

	bl_top->startBlock = blockid;
	bl_top->BlockCount = 1;

	if (vNand_MultiBlockErase(vNand, bl_top))
		ndprint(SIGBLOCK_ERROR, "%s, line:%d, erase blockid %d error!\n", __func__, __LINE__, bl_top->startBlock);

	BuffListManager_freeList(blm, (void **)&bl_top, (void *)bl_top, sizeof(BlockList));

	return vNand_MarkBadBlock(vNand, blockid);
}

static inline PageList *get_plnode(int blm, PageList **top, PageList *prev)
{
	PageList *refer = prev ? prev : (*top);

	if (!(*top)) {
		*top = (PageList *)BuffListManager_getTopNode(blm, sizeof(PageList));
		return (*top);
	} else {
		return (PageList *)BuffListManager_getNextNode(blm, (void *)refer, sizeof(PageList));
	}
}

static inline void free_pl(int blm, PageList **top)
{
	BuffListManager_freeAllList(blm, (void *)top, sizeof(PageList));
}

/**
 *	get_phy_block  - get physical block id
 *
 *	@conptr: a global variable
 *	@blockid: the logic block id
 */
static int get_phy_block(SmbContext *conptr, int blockid)
{
	int i = 0;
	PPartition *pt = (PPartition *)conptr->vnand.prData;

	ndprint(SIGBLOCK_DEBUG, "request block is: %d\n", blockid);
	ndprint(SIGBLOCK_DEBUG, "bad block is:");

	for(i = 0; i < blockid + 1; i++) {
		if(is_badblock(&conptr->vnand, i)) {
			blockid++;
			ndprint(SIGBLOCK_DEBUG, " %d ", i);
		}
		if(i > pt->totalblocks) {
			ndprint(SIGBLOCK_ERROR, "bad block overrun the partition\n");
			return ERROR_BADBLOCK_TANTO;
		}
	}

	ndprint(SIGBLOCK_DEBUG, "\nphy block is %d\n", blockid);

	return blockid;
}

/**
 *	sectornode_to_lpagelist  -  Convert one node of SectorList to a PageList
 *
 *	@conptr: global variable
 *	@snod: object need to calculate
 *	@pl: logical pagelist
 */
static PageList* sectornode_to_lpagelist(SmbContext *conptr,  SectorList *sl_node, PageList **pl)
{
	int i;
	unsigned int data_offset = 0;
	unsigned int totalpages;
	unsigned int startpage;
	PageList *pagenode = NULL;

	pagenode = *pl;
	startpage = sl_node->startSector / conptr->spp;
	totalpages = get_unit_count_from_sl(sl_node, conptr->spp);

	/*
	ndprint(SIGBLOCK_DEBUG,
			"slnode->lpl: startSector = %d, sectorCount = %d, startpage = %d, totalpages = %d\n",
			sl_node->startSector, sl_node->sectorCount, startpage, totalpages);
	*/
	for (i = 0; i < totalpages; i++) {
		pagenode = get_plnode((int)conptr->blm, pl, pagenode);
		if (!pagenode) {
			ndprint(SIGBLOCK_ERROR, "ERROR: %s, line:%d, get pagelist node error!\n", __func__, __LINE__);
			return NULL;
		}

		pagenode->startPageID = startpage + i;

		if ((i == 0) && (sl_node->startSector % conptr->spp)) {
			pagenode->Bytes = (conptr->spp - sl_node->startSector % conptr->spp) * SECTOR_SIZE;
			pagenode->OffsetBytes = (sl_node->startSector % conptr->spp) * SECTOR_SIZE;
		} else if (i == (totalpages - 1)) {
			pagenode->Bytes = ((sl_node->sectorCount -
							   (conptr->spp - sl_node->startSector % conptr->spp)) %
							   conptr->spp) * SECTOR_SIZE;
			if (pagenode->Bytes == 0)
				pagenode->Bytes = conptr->spp  * SECTOR_SIZE;
			pagenode->OffsetBytes = 0;
		} else {
			pagenode->Bytes = conptr->spp  * SECTOR_SIZE;
			pagenode->OffsetBytes = 0;
		}

		pagenode->retVal = 1;

		pagenode->pData = (unsigned char *)sl_node->pData + data_offset;
		data_offset += pagenode->Bytes;

		/*
		ndprint(SIGBLOCK_DEBUG,
				"slnode->lpl: startpageID = %d, offsetBytes = %d, Bytes = %d, pData = %p\n",
				pagenode->startPageID, pagenode->OffsetBytes, pagenode->Bytes, pagenode->pData);
		*/
	}
	if (pagenode)
		pagenode->retVal = 0;

	return pagenode;
}

/**
 *	sectorlist_to_pagelist  -  Convert one node of SectorList to a PageList
 *
 *	@conptr: global variable
 *	@snod: object need to calculate
 *	@pl: physical pagelist, which will align with block
 */
static void sectorlist_to_lpagelist(SmbContext *conptr, SectorList *sl, PageList **pl)
{
	struct singlelist *pos;
	SectorList *sl_node;
	PageList *pl_end = NULL;

	/* sectorlist to logical pagelist */
	singlelist_for_each(pos, &(sl->head)) {
		sl_node = singlelist_entry(pos, SectorList, head);

		ndprint(SIGBLOCK_DEBUG, "\nsimple_manager: startSector = %d, sectorCount = %d, pData = %p\n",
				sl_node->startSector, sl_node->sectorCount, sl_node->pData);

		if (0 == sl_node->sectorCount)
			continue;

		if (!(*pl))
			pl_end = sectornode_to_lpagelist(conptr, sl_node, pl);
		else
			pl_end = sectornode_to_lpagelist(conptr, sl_node, &pl_end);

		if (!pl_end) {
			ndprint(SIGBLOCK_ERROR, "%s, line:%d, convert sl to pl error!\n", __func__, __LINE__);
			goto error;
		}
	}

	return;
error:
	free_pl((int)conptr->blm, pl);
	*pl = NULL;
	return;
}

/**
 *	pl_get_first_block  -  get pagelist of the first
 *  block of the whole pagelist
 *
 *	@conptr: a global variable
 *	@pl: logical pagelist
 *  @return: part of pagelist which contained in one block
 */
static PageList* get_first_block_lpl(SmbContext *conptr, PageList **pl) {
	PageList *pl_top = NULL;
	PageList *pl_tmp, *pl_prev;
	struct singlelist *pos;

	conptr->lblockid = -1;
	singlelist_for_each(pos, &((*pl)->head)) {
		pl_tmp = singlelist_entry(pos, PageList, head);
		if (!pl_top) {
			pl_top = pl_tmp;
			conptr->poffb = pl_top->startPageID % conptr->ppb;
		}

		if ((pl_tmp->startPageID / conptr->ppb) != conptr->lblockid) {
			if (conptr->lblockid != -1) {
				pl_prev->head.next = NULL;
				*pl = pl_tmp;
				return pl_top;
			}
			conptr->lblockid = pl_tmp->startPageID / conptr->ppb;
		}
		pl_prev = pl_tmp;

		/*
		ndprint(SIGBLOCK_DEBUG,
				"plpl: startpageID = %d, offsetBytes = %d, Bytes = %d, pData = %p\n",
				pl_tmp->startPageID, pl_tmp->OffsetBytes, pl_tmp->Bytes, pl_tmp->pData);
		*/
	}

	*pl = NULL;
	return pl_top;
}

/**
 *	pl_get_first_block  -  get pagelist of the first
 *  block of the whole pagelist
 *
 *	@conptr: a global variable
 *	@pl: logical pagelist
 *  @return: physical pagelist
 */
static PageList* lpl_to_ppl(SmbContext *conptr, PageList *pl) {
	struct singlelist *pos;
	PageList *pl_tmp;

	conptr->pblockid = get_phy_block(conptr, conptr->lblockid);
	if (conptr->pblockid < 0) {
		ndprint(SIGBLOCK_ERROR, "%s, line:%d, get physical blockid error!\n", __func__, __LINE__);
		return NULL;
	}

	singlelist_for_each(pos, &(pl->head)) {
		pl_tmp = singlelist_entry(pos, PageList, head);
		pl_tmp->startPageID = pl_tmp->startPageID + (conptr->pblockid - conptr->lblockid) * conptr->ppb;

		ndprint(SIGBLOCK_DEBUG, "plpl->ppl: startPageID = %d, OffsetBytes = %d, Bytes = %d, pData = %p\n",
				pl_tmp->startPageID, pl_tmp->OffsetBytes, pl_tmp->Bytes, pl_tmp->pData);
	}

	return pl;
}

/**
 *	block_data_copy_to_next - copy current block to
 *	next writeable block.
 */
static int block_data_copy_to_next(SmbContext *conptr, int srcblock, int dstblock, int endpage, int endpageBytes)
{
	int ret;
	unsigned int srcStartPageID = srcblock * conptr->ppb;
	unsigned int dstStartPageID = dstblock * conptr->ppb;
	PageList *rpl = NULL, *wpl = NULL, *rpl_node = NULL, *wpl_node = NULL;

	while (srcStartPageID <= endpage) {
		rpl_node = get_plnode((int)conptr->blm, &rpl, rpl_node);
		wpl_node = get_plnode((int)conptr->blm, &wpl, wpl_node);

		rpl_node->startPageID = srcStartPageID;
		wpl_node->startPageID = dstStartPageID;
		if (srcStartPageID == endpage) {
			rpl_node->Bytes = endpageBytes;
			wpl_node->Bytes = endpageBytes;
		} else {
			rpl_node->Bytes = conptr->bpp;
			wpl_node->Bytes = conptr->bpp;
		}
		rpl_node->OffsetBytes = 0;
		wpl_node->OffsetBytes = 0;
		rpl_node->pData = NULL;
		wpl_node->pData = NULL;

		/*
		ndprint(SIGBLOCK_DEBUG, "copy data: rpl, startPageID = %d, OffsetBytes = %d, Bytes = %d\n",
				rpl_node->startPageID, rpl_node->OffsetBytes, rpl_node->Bytes);
		ndprint(SIGBLOCK_DEBUG, "copy data: wpl, startPageID = %d, OffsetBytes = %d, Bytes = %d\n",
				wpl_node->startPageID, wpl_node->OffsetBytes, wpl_node->Bytes);
		*/

		srcStartPageID ++;
		dstStartPageID ++;
	}

	ret = vNand_CopyData(&conptr->vnand, rpl, wpl);

	free_pl((int)conptr->blm, &rpl);
	free_pl((int)conptr->blm, &wpl);

	return ret;
}

/**
 *	write_error_copydata  -  write error
 *
 *	@conptr: a global variable
 *	@retval: error flag
 */
static int write_error_copydata(SmbContext *conptr, int endpage, int endpageBytes, int retval)
{
	PPartition *pt = (PPartition *)conptr->vnand.prData;
	int srcblock = conptr->pblockid;
	int destblock, ret = retval;

	while (SUCCESS != ret) {
		conptr->pblockid++;
		if (conptr->pblockid > pt->startblockID + pt->totalblocks) {
			ndprint(SIGBLOCK_ERROR, "more bad block in this partition\n");
			ret = ERROR_BADBLOCK_TANTO;
			break;
		}

		if (is_badblock(&conptr->vnand, conptr->pblockid)) {
			continue;
		}

		if (BLOCK_FIRST_PAGE == conptr->poffb) {
			return mark_bakblock((int)conptr->blm, &conptr->vnand, srcblock);
		}

		destblock = conptr->pblockid;
		ret = block_data_copy_to_next(conptr, srcblock, destblock, endpage, endpageBytes);
		ndprint(SIGBLOCK_DEBUG, "copy data:(%d-->%d),ret = %d\n", srcblock, destblock, ret);
		if (SUCCESS != ret) {
			retval = mark_bakblock((int)conptr->blm, &conptr->vnand, destblock);
			if (retval != SUCCESS) {
				ret = retval;
				break;
			}
		}
	}

	if (SUCCESS == ret)
		return mark_bakblock((int)conptr->blm, &conptr->vnand, srcblock);

	return ret;
}

/**
 *	multi_block_rw  -  multi block read or write
 *
 *	@conptr: a global variable
 *	@pl:   the physics pagelist
 *	@rwflag: a flag of read of write
 */
static int signal_block_rw(SmbContext *conptr, PageList *pl, int rwflag)
{
	if (SIMP_WRITE == rwflag) {
		return vNand_MultiPageWrite(&conptr->vnand, pl);
	} else {
		return vNand_MultiPageRead(&conptr->vnand, pl);
	}
}

/**
 *	simpblock_rw  -  simp read or wirte
 *
 *	@conptr: a global variable
 *	@sl: physics sectorlist
 *	@rwflag: a flag of read of write
 */
static int simpblock_rw(SmbContext *conptr, SectorList *sl, int rwflag)
{
	int ret;
	PageList *lpl = NULL, *lpl_tmp, *ppl_tmp;

	/* convert sectorlist to logical pagelist */
	sectorlist_to_lpagelist(conptr, sl, &lpl);
	if (!lpl) {
		ndprint(SIGBLOCK_ERROR, "%s, line:%d, create pagelist error!\n", __func__, __LINE__);
		return -1;
	}

	/* split the whole logical pagelist align block and
	 convert it to physical pagelist */
	do {
		lpl_tmp = get_first_block_lpl(conptr, &lpl);
	rewrite:
		ppl_tmp = lpl_to_ppl(conptr, lpl_tmp);
		if (!ppl_tmp) {
			ndprint(SIGBLOCK_ERROR, "%s, line:%d, convert lpl to ppl error!\n", __func__, __LINE__);
			free_pl((int)conptr->blm, &lpl_tmp);
			return -1;
		}
		ret = signal_block_rw(conptr, ppl_tmp, rwflag);
		if (ret != SUCCESS) {
			ndprint(SIGBLOCK_ERROR, "%s, line:%d, warning: %s faild, ret = %d\n",
					__func__, __LINE__, (rwflag == SIMP_WRITE) ? "write" : "read", ret);
			if (rwflag == SIMP_WRITE) {
				int endpage = (ppl_tmp->OffsetBytes == 0) ? (ppl_tmp->startPageID - 1) : ppl_tmp->startPageID;
				int endpageBytes = (endpage == ppl_tmp->startPageID) ? ppl_tmp->OffsetBytes : conptr->bpp;
				ret = write_error_copydata(conptr, endpage, endpageBytes, ret);
				if (ret == SUCCESS)
					goto rewrite;
				else {
					ndprint(SIGBLOCK_ERROR, "%s, line:%d, mark bad block error!\n", __func__, __LINE__);
					free_pl((int)conptr->blm, &ppl_tmp);
					return -1;
				}
			}
		}
		free_pl((int)conptr->blm, &ppl_tmp);
	} while (lpl);

	return SUCCESS;
}

/**
 *	check_mode  - check mode DIRECT_MANAGER
 *
 *	@conptr: a global variable
 */
static inline int check_mode(SmbContext *conptr)
{
	return (DIRECT_MANAGER == conptr->mode) ?
		SUCCESS : MODE_ERROR;
}

/**
 *	SimpleBlockManager_Open  - open operation
 *
 *	@vnand: virtual nand
 *	@pt: physical partition
 *
 *	This return a handle to read, write and close
 */
int SimpleBlockManager_Open(VNandInfo *vn, PPartition *pt)
{
	SmbContext *conptr = NULL;

	if (pt->mode != DIRECT_MANAGER) {
		ndprint(SIGBLOCK_ERROR, "%s mode error\n", __func__);
		return -1;
	}

	conptr = (SmbContext *)Nand_VirtualAlloc(sizeof(SmbContext));
	conptr->mode = DIRECT_MANAGER;
	CONV_PT_VN(pt, &conptr->vnand);
	conptr->blm = Blm;
	conptr->ppb = vn->PagePerBlock;
	conptr->spp = vn->BytePerPage / SECTOR_SIZE;
	conptr->spb = conptr->spp * conptr->ppb;
	conptr->bpp = vn->BytePerPage;

	return (int)conptr;
}

/**
 *	SiplBlockManager_Close  -  Close operation
 *
 *	@handle: return value of SimpleBlockManager_Open
 */
int SimpleBlockManager_Close(int handle)
{
	SmbContext *conptr = (SmbContext *)handle;

	if (MODE_ERROR == check_mode(conptr)) {
		ndprint(SIGBLOCK_ERROR,
				"%s mode error, request %d not DIRECT_MANAGER\n",
				__func__, conptr->mode);
		return MODE_ERROR;
	}

	Nand_VirtualFree(conptr);

	return 0;
}

/**
 *	SimpleBlockManager_Read  -  Read operation
 *
 *	@context: return value of SimpleBlockManager_Open
 *	@sl: which need to read
 *
 *	Transform SectorList to PageList directly,
 *	no use L2P cache, and don't go through NandManager.
 */
int SimpleBlockManager_Read(int context, SectorList *sl)
{
	SmbContext *conptr = (SmbContext *)context;

	if (MODE_ERROR == check_mode(conptr)) {
		ndprint(SIGBLOCK_ERROR,
				"%s mode error, request %d not DIRECT_MANAGER\n",
				__func__, conptr->mode);
		return MODE_ERROR;
	}

	if (NULL == sl) {
		ndprint(SIGBLOCK_ERROR, "%s: sl: NULL\n", __func__);
		return ENAND;
	}

	return simpblock_rw(conptr, sl, SIMP_READ);
}

/**
 *	SimpleBlockManager_Write  - simple write operation.
 *
 *	@context: return value of SimpleBlockManager_Open
 *	@sl: which need to write
 *
 *	Transform SectorList to PageList directly,
 *	no use L2P cache, and don't go through NandManager.
 */
int SimpleBlockManager_Write(int context, SectorList *sl)
{
	SmbContext *conptr = (SmbContext *)context;

	if (MODE_ERROR == check_mode(conptr)) {
		ndprint(SIGBLOCK_ERROR,
				"%s mode error, request %d not DIRECT_MANAGER\n",
				__func__, conptr->mode);
		return MODE_ERROR;
	}

	if (NULL == sl) {
		ndprint(SIGBLOCK_ERROR, "%s: sl: NULL\n", __func__);
		return ENAND;
	}

	return simpblock_rw(conptr, sl, SIMP_WRITE);
}

/**
 *	SimpleBlockManager_Ioctrl  -  IO command
 *
 *	@context: return value of SimpleBlockManager_Open
 *	@cmd: command
 *	@argv: argument of command
 */
int SimpleBlockManager_Ioctrl(int context, int cmd, int argv)
{
	SmbContext *conptr = (SmbContext *)context;

	if (MODE_ERROR == check_mode(conptr)) {
		ndprint(SIGBLOCK_ERROR,
				"%s mode error, request %d not DIRECT_MANAGER\n",
				__func__, conptr->mode);
		return MODE_ERROR;
	}

	return SUCCESS;
}

PartitionInterface smb_nand_ops = {
	.PartitionInterface_iOpen	= SimpleBlockManager_Open,
	.PartitionInterface_iClose	= SimpleBlockManager_Close,
	.PartitionInterface_Read	= SimpleBlockManager_Read,
	.PartitionInterface_Write	= SimpleBlockManager_Write,
	.PartitionInterface_Ioctrl	= SimpleBlockManager_Ioctrl,
};

int SimpleBlockManager_Init(PManager* pm)
{
	Blm = pm->bufferlist;
	return NandManger_Register_Manager((int)pm, DIRECT_MANAGER, &smb_nand_ops);
}

void SimpleBlockManager_Deinit(int handle)
{
	return;
}
