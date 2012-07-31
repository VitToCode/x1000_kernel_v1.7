#include "clib.h"
#include "simpleblockmanager.h"
#include "bufflistmanager.h"
#include "nanddebug.h"
#include "vNand.h"
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

	ndprint(SIGBLOCK_DEBUG, "request block is: %d", blockid);
	ndprint(SIGBLOCK_DEBUG, "bad block is:");
	for(i = 0; i < blockid + 1; i++) {
		if(vNand_IsBadBlock(&conptr->vnand, i)) {
			blockid++;
			ndprint(SIGBLOCK_DEBUG, " %d ", i);
		}
		if(i > pt->startblockID + pt->totalblocks) {
			ndprint(SIGBLOCK_ERROR, "bad block overrun the partition\n");
			return ERROR_BADBLOCK_TANTO;
		}
	}

	ndprint(SIGBLOCK_DEBUG, "phy block is %d\n", blockid);

	return blockid;
}

/**
 *	get_pagenode  - get a page node from pagelist top
 *
 *	@conptr: a global variable
 *	@top: the pagelist top
*/
static inline PageList *get_pagenode(SmbContext *conptr, void *top)
{
	return (PageList *)BuffListManager_getNextNode(
			(int)conptr->blm, (void *)top,sizeof(PageList));
}

/**
 *	get_startpage  - calculate the pagelist StartPage
 *
 *	@conptr: a global variable
 *	@pagenum: current pagenode in pagelist number
*/
static inline int get_startpage(SmbContext *conptr, int pagenum)
{
	return conptr->pblockid * conptr->ppb + conptr->poffb + pagenum; 
}

/**
 *	get_bytes  - calculate the pagelist Bytes 
 *
 *	@conptr: a global variable
 *	@total: the pagelist contain pagenode total count
 *	@pagenum: current pagenode in pagelist number
 *	@sl: the sector list.
*/
static inline int get_bytes(SmbContext *conptr, int total,
		SectorList *sl, int pagenum)
{
	int firstpage = conptr->spp - sl->startSector % conptr->spp;
	int lastpage = ((sl->sectorCount - firstpage) % conptr->spp == 0) ?
		conptr->spp : (sl->sectorCount - firstpage) % conptr->spp;

	if (1 == total)
		return sl->sectorCount * SECTOR_SIZE;
	else {
		if (0 == pagenum) {
			return ((conptr->spp - sl->startSector % conptr->spp) 
					* SECTOR_SIZE);
		} else if ((total - 1) == pagenum && 
				lastpage != conptr->spp) {
			return (lastpage * SECTOR_SIZE);
		} else {
			return conptr->bpp;
		}
	}
}

/**
 *	get_offsetbytes  -  calculate pagelist OffsetBytes
 *
 *	@conptr:  a global variable
 *	@sl:	  sector node
 *	@pagenum: this pagenode in pagelist number
*/
static inline int get_offsetbytes(SmbContext *conptr, SectorList *sl, int pagenum)
{
	if (0 == pagenum) {
		return sl->startSector % conptr->spp * SECTOR_SIZE;
	} else {
		return 0;
	}
}

/* *
 *	sectornode_to_pagelist  -  Convert one node of SectorList to a PageList
 *
 *	@conptr: global variable
 *	@snod: object need to calculate
 *	@pl: which created when Convert finished
*/
static void sectornode_to_pagelist(SmbContext *conptr, 
		SectorList *snod, PageList *pl)
{
	int i, offset = 0, frontbytes = 0, totalpages;
	PageList *pagenode;
	
	totalpages = get_unit_count_from_sl(snod, conptr->spp);
	for (i = 0; i < totalpages; i++) {

		pagenode = (0 == i) ? pl : get_pagenode(conptr, (void *)pl);
		pagenode->startPageID = get_startpage(conptr, i);
		pagenode->Bytes = get_bytes(conptr, totalpages, snod, i);
		pagenode->OffsetBytes = get_offsetbytes(conptr, snod, i);

		offset = (0 == i) ? 0 : (offset + frontbytes);
		pagenode->pData  = (unsigned char *)snod->pData + offset;
		frontbytes = pagenode->Bytes;

		ndprint(SIGBLOCK_DEBUG, "start=%d,bytes=%d,offsetbytes=%d,offset=%d\n",
				pagenode->startPageID, pagenode->Bytes,
				pagenode->OffsetBytes, offset);
	}
}

/**
 *	mRead  -  single block read
 *
 *	@conptr: a global variable
 *	@sl_node:   the physics sector node must be in a block
*/
static int mRead (SmbContext *conptr, SectorList *sl_node)
{
	int ret;
	PageList *pl_top;

	conptr->pblockid = get_phy_block(conptr, conptr->lblockid);
	if (conptr->pblockid < 0) {
		ret = ERROR_BADBLOCK_TANTO;
		goto rerror_badblock_tanto;
	}

	pl_top = (PageList *)BuffListManager_getTopNode(
			(int)conptr->blm, sizeof(PageList));
	if (NULL == pl_top) {
		ndprint(SIGBLOCK_ERROR, "ERROR: fun %s line %d\n", 
				__func__, __LINE__);
		ret = ERROR_NOMEM;
		goto rerror_get_pltop;
	}

	sectornode_to_pagelist(conptr, sl_node, pl_top);

	ret = vNand_MultiPageRead(&conptr->vnand, pl_top);
	if(ret < 0){
		ret = pl_top->retVal & 0xffff;
		ndprint(SIGBLOCK_ERROR, "%s,%d retVal = %d\n",
				__func__, __LINE__, ret);
	}

	BuffListManager_freeAllList((int)conptr->blm, 
			(void **)&pl_top, sizeof(PageList));

rerror_badblock_tanto:
rerror_get_pltop:

	return ret;
}

/**
 *	erase_single_block  -  erase a block
 *
 *	@conptr: a global variable
 *	@blockid: the block number
 *	@first : first page of the block or not
*/
static int erase_single_block(SmbContext *conptr, int blockid, int first)
{
	int ret, statpage = blockid * conptr->ppb;
	unsigned char *buf;
	BlockList *bl_top;

	if (BLOCK_FIRST_PAGE != first)
		return SUCCESS;

	buf = (unsigned char *)Nand_VirtualAlloc(
			sizeof(unsigned char) * conptr->bpp);
	memset(buf, 0x0, sizeof(unsigned char)  * conptr->bpp);

	ret = vNand_PageRead(&conptr->vnand, statpage, 0, conptr->bpp, buf); 
	if(ret < 0) {
		if(DATA_WRITED != ret){
			ndprint(SIGBLOCK_ERROR,
					"read first page err: %s, %d retVal = %d\n",
					__FILE__, __LINE__, ret);
			Nand_VirtualFree((void *)buf);
			return ret;
		}
	} else {
		bl_top = (BlockList *)BuffListManager_getTopNode(
				(int)conptr->blm,sizeof(BlockList));
		bl_top->startBlock = blockid;
		bl_top->BlockCount = 1;
		(bl_top->head).next = NULL;
		vNand_MultiBlockErase(&conptr->vnand, bl_top );
		BuffListManager_freeAllList((int)conptr->blm, 
				(void **)&bl_top, sizeof(BlockList));
	}

	Nand_VirtualFree((void *)buf);

	return SUCCESS;
}

static inline PageList *get_plnode_from_top(int blm, PageList **top)
{
	if (NULL == *top) {
		*top = (PageList *)BuffListManager_getTopNode(blm, sizeof(PageList));

		return (*top);
	} else {
		return (PageList *)BuffListManager_getNextNode(
				blm, (void *)(*top), sizeof(PageList));
	}
}

static inline void free_all_plnode(int blm, void **top)
{
	BuffListManager_freeAllList(blm, top, sizeof(PageList));
}

static inline void fill_fullpage_pagenode(PageList *pl, int startpageid, int bpp)
{
	pl->startPageID = startpageid;
	pl->OffsetBytes = 0;
	pl->Bytes = bpp;
	(pl->head).next = NULL;
}

/**
 *	get_unit_count_start_firstpage  -  get a unit count,
 *	start in first page of a block 
 *
 *	@count: total page number
 *	@unitsize: a unit page number
*/
static inline int get_unit_count_start_firstpage(int count, int unitsize)
{
	return (count + unitsize - 1) / unitsize;
}

/**
 *	block_data_copy_to_next - copy current block to
 *	next writeable block.
*/
static int block_data_copy_to_next(SmbContext *conptr, 
		int srcblock, int destblock)
{
	int i, j, ret, startpageid;
	int pageperunit = VNANDCACHESIZE / conptr->bpp;
	int unitcnt = get_unit_count_start_firstpage(conptr->poffb, pageperunit);
	int wpagecnt = 0;
	PageList *rpl = NULL, *wpl = NULL, *rpl_node = NULL, *wpl_node = NULL;

	ret = erase_single_block(conptr, destblock, BLOCK_FIRST_PAGE);
	if(ret < 0)
		return  ret;

	for(j = 0; j < unitcnt; j++) {

		startpageid = srcblock * conptr->ppb + (j * pageperunit); 

		if ((j == unitcnt -1) && (0 != (conptr->poffb % VNANDCACHESIZE)))
			wpagecnt = conptr->poffb / pageperunit;
		else
			wpagecnt = pageperunit;

		for (i = 0; i < wpagecnt; i++) {
			rpl_node = get_plnode_from_top((int)conptr->blm, &rpl);
			wpl_node = get_plnode_from_top((int)conptr->blm, &wpl);

			fill_fullpage_pagenode(rpl_node, conptr->bpp, startpageid + i);
			fill_fullpage_pagenode(wpl_node, conptr->bpp, 
					startpageid + i + conptr->ppb * (destblock - srcblock));
		}
		ret = vNand_CopyData (&conptr->vnand,rpl, wpl);

		free_all_plnode((int)conptr->blm, (void **)&rpl);
		free_all_plnode((int)conptr->blm, (void **)&wpl);

		if(ret < 0){
			ndprint(SIGBLOCK_ERROR,	"%s:%d ret=%d\n",
					__func__, __LINE__, wpl->retVal & 0xffff);
			return  ret;
		}
	}

	return SUCCESS;
}

/**
 *	write_error_copydata  -  write error 
 *
 *	@conptr: a global variable
 *	@retval: error flag
*/
static int write_error_copydata(SmbContext *conptr, int retval)
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

		if (vNand_IsBadBlock(&conptr->vnand, conptr->pblockid)) {
			continue;
		}

		if (BLOCK_FIRST_PAGE == conptr->poffb) {
			return vNand_MarkBadBlock(&conptr->vnand, srcblock);
		}

		destblock = conptr->pblockid;
		ret = block_data_copy_to_next(conptr, srcblock, destblock);
		ndprint(SIGBLOCK_DEBUG, "copy data:(%d-->%d),ret = %d\n", 
				srcblock, destblock, ret);
		if (SUCCESS != ret) {
			retval = vNand_MarkBadBlock(&conptr->vnand, destblock);
			if (retval != SUCCESS) {
				ret = retval;
				break;
			}
		}
	}

	if (SUCCESS == ret)
		return vNand_MarkBadBlock(&conptr->vnand, srcblock);

	return ret;
}

/**
 *	mWrite  -  single block write
 *
 *	@conptr: a global variable
 *	@sl_node:   the physics sector node must be in a block
*/
static int mWrite (SmbContext *conptr, SectorList *sl_node )
{
	int ret = -1;
	PageList *pl_top;
	BuffListManager *blm = conptr->blm;

	conptr->pblockid = get_phy_block(conptr, conptr->lblockid);
	if (conptr->pblockid < 0) {
		ret = ERROR_BADBLOCK_TANTO;
		goto werror_badblock_tanto;
	}

retry:
	ret = erase_single_block(conptr, conptr->pblockid, conptr->poffb); 
	if(ret < 0){
		ndprint(SIGBLOCK_ERROR, "erase_single_block error, %s\n", __func__);
		goto werror_erase_block;
	}

	pl_top = (PageList *)BuffListManager_getTopNode((int)blm,sizeof(PageList));
	if (!pl_top) {
		ndprint(SIGBLOCK_ERROR, "ERROR: fun %s line %d\n", __func__, __LINE__);
		ret = ERROR_NOMEM;
		goto werror_nomem;
	}

	sectornode_to_pagelist(conptr, sl_node, pl_top);

	ret = vNand_MultiPageWrite(&conptr->vnand, pl_top);

	if(SUCCESS != ret) {
		ret = pl_top->retVal & 0xffff;
		switch (ret) {
		case ENAND:
		case DMA_AR:
		case IO_ERROR:
		case TIMEOUT:
		case ECC_ERROR:
			ndprint(SIGBLOCK_ERROR, "ERROR: ret = %d,fun %s line %d\n", 
					ret, __func__, __LINE__);
			break;
		default:
			break;
		}

		ret =  write_error_copydata(conptr, ret);
		if (SUCCESS == ret) {
			BuffListManager_freeAllList((int)conptr->blm, 
					(void **)&pl_top, sizeof(PageList));
			goto retry;
		} else {
			ndprint(SIGBLOCK_ERROR, "copydata failed line:%d,ret=%d\n",
					__LINE__, ret);
		}
	}

	BuffListManager_freeAllList((int)conptr->blm, 
			(void **)&pl_top, sizeof(PageList));
werror_badblock_tanto:
werror_erase_block:
werror_nomem:

	return ret;
}

/* *
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

static inline int check_mode(SmbContext *conptr)
{
	return (DIRECT_MANAGER == conptr->mode) ?
		SUCCESS : MODE_ERROR;
}


/**
 *	SimpleBlockManager_Close  -  Close operation
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
 *	single_block_rw  -  single block read or write
 *
 *	@conptr: a global variable
 *	@sl_node:   the physics sector node must be in a block
 *	@rwflag: a flag of read of write
*/
static inline int single_block_rw(SmbContext *conptr,
		SectorList *sl_node, int rwflag)
{
	int startpage = sl_node->startSector / conptr->spp;

	conptr->lblockid = startpage / conptr->ppb;
	conptr->poffb = startpage % conptr->ppb;

	if (SIMP_WRITE == rwflag) {
		return mWrite(conptr, sl_node);
	} else {
		return mRead(conptr, sl_node);
	}
}

/**
 *	split_sectornode_to_block_rw  -  split a sector node to 
 *	more if the sector is over a block.
 *
 *	@conptr: a global variable
 *	@node:   the physics sector node
 *	@rwflag: a flag of read of write
 *	@blockcnt: current sector node over block numbers
*/
static int split_sectornode_to_block_rw(SmbContext *conptr,
		SectorList *node, int rwflag, int blockcnt)
{
	SectorList rwnode;
	int start = node->startSector;
	int cnt = node->sectorCount;
	int i, ret, bufoffset = 0;
	int secoffset = node->startSector % conptr->spb;
	int sectors = secoffset + cnt;
	int rwblockid = node->startSector / conptr->spb;

	for (i = 0; i < blockcnt; i++, rwblockid++) {
		rwnode.startSector = (0 == i) ? start : (rwblockid * conptr->spb);

		rwnode.sectorCount = (0 == i) ? 
			(conptr->spb - secoffset) :
			((sectors % conptr->spb == 0) ? conptr->spb : (sectors % conptr->spb));

		rwnode.pData = (unsigned char *)(node->pData) + bufoffset;

		ndprint(SIGBLOCK_DEBUG, "%s:start Sec:%d,Count:%d,buffoffset in sector:%d\n",
				rwflag == 0 ? "write" : "read",
				rwnode.startSector, rwnode.sectorCount, 
				bufoffset / SECTOR_SIZE);

		bufoffset += rwnode.sectorCount * SECTOR_SIZE;

		ret = single_block_rw(conptr, &rwnode, rwflag);
		if (ret < 0) {
			return ret;
		}
	}

	return SUCCESS;
}

/**
 *	simpblock_rw  -  simple read or wirte
 *
 *	@conptr: a global variable
 *	@sl: physics sectorlist
 *	@rwflag: a flag of read of write
*/
static int simpblock_rw(SmbContext *conptr, SectorList *sl, int rwflag)
{
	struct singlelist *pos;
	SectorList *sl_node;
	int ret = 0;
	int blockcnt;

	singlelist_for_each(pos, &(sl->head)) {
		sl_node = singlelist_entry(pos, SectorList, head);
		if (0 == sl_node->sectorCount) {
			continue;
		}
		blockcnt = get_unit_count_from_sl(sl_node, conptr->spb);
		if (1 == blockcnt) {
			ret = single_block_rw(conptr, sl_node, rwflag);
		} else {
			ret = split_sectornode_to_block_rw(conptr, sl_node, 
					rwflag, blockcnt);
		}

		if (ret < 0) {
			return ret;
		}
	}

	return SUCCESS;
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

/* *
 *	SimpleBlockManager_Write  - Simple write operation.
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

}
