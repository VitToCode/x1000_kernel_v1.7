#include "string.h"
#include "simpleblockmanager.h"
#include "bufflistmanager.h"
#include "nanddebug.h"
#include "vNand.h"
#include "context.h"
#include "nandmanagerinterface.h"
#include "partitioninterface.h"

static BuffListManager *Blm;

static int get_unit_count(SectorList *sl_node, int unit)
{
	int unitcnt = 0;
	int start = sl_node->startSector;
	int cnt = sl_node->sectorCount;

	if (start % unit + cnt <= unit)
		unitcnt = 1;
	else { 
		if ((start % unit + cnt) % unit == 0)
			unitcnt = (start % unit + cnt) / unit;
		else
			unitcnt = (start % unit + cnt) / unit + 1;
	}

	ndprint(SIGBLOCK_DEBUG,"need unit =%d, cnt = %d.\n",unit, unitcnt);
	return unitcnt;
}


static int get_operator_block(VNandInfo* vnand, int blockid)
{
	int i = 0;
	PPartition *pt = (PPartition *)vnand->prData;

	ndprint(SIGBLOCK_DEBUG,"request block is: %d ", blockid);
	ndprint(SIGBLOCK_DEBUG,"bad block is: ");
	for(i = 0; i < blockid + 1; i++) {
		if(vNand_IsBadBlock(vnand, i)) {
			blockid++;
			ndprint(SIGBLOCK_DEBUG," %d ", i);
		}
		if(i > pt->startblockID + pt->totalblocks) {
			ndprint(SIGBLOCK_ERROR,"bad block overrun the partition\n.");
			return -1;
		}
	}

	ndprint(SIGBLOCK_DEBUG,"---operator block is %d\n", blockid);

	return blockid;
}

/* *
 *	sectornode_to_pagelist  -  Convert one node of SectorList to a PageList
 *
 *	@context: global variable
 *	@sectornode: object need to calculate
 *	@pagelist: which created when Convert finished
*/
static int sectornode_to_pagelist(SmbContext *conptr, 
		SectorList *sectornode, PageList *pagelist)
{
	int i, offset = 0;
	PageList *pagenode = pagelist;
	int pagecount, sectorperpage = conptr->vnand->BytePerPage / SECTOR_SIZE;
	int pageperblock = conptr->vnand->PagePerBlock;
	int optblock = get_operator_block(conptr->vnand, conptr->reqblockid);
	int start = sectornode->startSector;
	int count = sectornode->sectorCount;
	int firstpagesectors = sectorperpage - start % sectorperpage;
	int lastpagesectors = ((count - firstpagesectors) % sectorperpage == 0) ?
		sectorperpage : (count - firstpagesectors) % sectorperpage;
	int offbytes = 0;

	if (optblock == -1) {
		return -1;
	}
	
	pagecount = get_unit_count(sectornode, sectorperpage);

	for (i = 0; i < pagecount; i++) {
		if(i == 0) {
			pagenode = pagelist;
		} else {
			pagenode = (PageList *)BuffListManager_getNextNode(
				(int)conptr->blm, (void *)pagelist,sizeof(PageList));
		}

		/* fill startPageID */
		pagenode->startPageID = optblock * pageperblock + 
			(start / sectorperpage + i) % pageperblock;

		/* fill Bytes */
		if (pagecount == 1)
			pagenode->Bytes = sectornode->sectorCount * SECTOR_SIZE;
		else {
			if (i == 0)
				pagenode->Bytes = firstpagesectors * SECTOR_SIZE;
			else if (i == pagecount - 1 && 
					lastpagesectors != sectorperpage) 
				pagenode->Bytes = lastpagesectors * SECTOR_SIZE;
			else
				pagenode->Bytes = conptr->vnand->BytePerPage;
		}

		/* fill OffsetBytes */
		if (i == 0)
			pagenode->OffsetBytes = start % sectorperpage * SECTOR_SIZE;
		else
			pagenode->OffsetBytes = 0;

		/* fill pData */
		if (i == 0)
			offset = 0;
		else
			offset += offbytes;

		offbytes = pagenode->Bytes;

		pagenode->pData  = (unsigned char *)sectornode->pData + offset;

		ndprint(SIGBLOCK_DEBUG,"start=%d,bytes=%d,offsetbytes=%d,offset=%d\n",
				pagenode->startPageID, pagenode->Bytes,
				pagenode->OffsetBytes,offset);
	}
	return 0;
}

static int mRead (SmbContext *conptr, SectorList *sl_node)
{	
	int ret;
	PageList *pl_top;
	int sectorperpage = conptr->vnand->BytePerPage / SECTOR_SIZE;

	if (!sl_node)
		goto ERROR;

	pl_top = (PageList *)BuffListManager_getTopNode(
			(int)conptr->blm,sizeof(PageList));
	if (!(pl_top))
		goto ERROR;

	conptr->reqblockid = 
		sl_node->startSector / sectorperpage / conptr->vnand->PagePerBlock;

	sectornode_to_pagelist(conptr, sl_node, pl_top);
	ret = vNand_MultiPageRead(conptr->vnand, pl_top);
	if(ret < 0){
		ret = pl_top->retVal & 0xffff;
		ndprint(SIGBLOCK_ERROR," %s, %d retVal = %d\n",__FILE__,__LINE__,ret);
	}

	BuffListManager_freeAllList((int)conptr->blm, 
			(void **)&pl_top, sizeof(PageList));

	return ret;

ERROR:
	ndprint(SIGBLOCK_ERROR,"ERROR: fun %s line %d\n", __FUNCTION__, __LINE__);
	return -1;
}

static int erase_single_block(SmbContext *conptr, int blockid)
{
	int	ret;
	int statpage = blockid * conptr->vnand->PagePerBlock;
	int byteperpage = conptr->vnand->BytePerPage;
	BlockList *bl_top;
	unsigned char *buf = (unsigned char *)Nand_VirtualAlloc(
			sizeof(unsigned char) * conptr->vnand->BytePerPage);

	memset(buf, 0x0, sizeof(unsigned char)  * byteperpage);

	ret = vNand_PageRead(conptr->vnand, statpage, 0, byteperpage, buf); 
	if(ret < 0) {
		if(ret != -6){
			ndprint(SIGBLOCK_ERROR,
					" read first page err: %s, %d retVal = %d\n",
					__FILE__,__LINE__,ret);
			Nand_VirtualFree((void *)buf);
			return ret;
		}
	} else {
		bl_top = (BlockList *)BuffListManager_getTopNode(
				(int)conptr->blm,sizeof(BlockList));
		bl_top->startBlock = blockid;
		bl_top->BlockCount = 1;
		vNand_MultiBlockErase(conptr->vnand, bl_top );
		BuffListManager_freeAllList((int)conptr->blm, 
				(void **)&bl_top, sizeof(BlockList));
	}

	Nand_VirtualFree((void *)buf);
	return 0;
}

static int block_data_copy_to_next(SmbContext *conptr, int blockid, int stoppage)
{
	int i, z,ret, startpageid;
	int byteperpage = conptr->vnand->BytePerPage;
	int pageperblock = conptr->vnand->PagePerBlock;
	int pageperunit = VNANDCACHESIZE / byteperpage;
	int divide_exactly = !(stoppage % VNANDCACHESIZE);
	int cnt = divide_exactly ? (stoppage / VNANDCACHESIZE) 
		: (stoppage / VNANDCACHESIZE + 1); 
	int wpagecnt = 0;
	PageList *rpl, *wpl, *rpl_node, *wpl_node;

	ret = erase_single_block(conptr, blockid);
	if(ret < 0)
		return  ret;

	for(z = 0; z < cnt; z++) {
		rpl = (PageList *)BuffListManager_getTopNode(
				(int)conptr->blm,sizeof(PageList));
		wpl = (PageList *)BuffListManager_getTopNode(
				(int)conptr->blm,sizeof(PageList));

		startpageid = blockid * pageperblock + (z * pageperunit); 
		if ((z == cnt -1) && !divide_exactly) 
			wpagecnt = stoppage / pageperunit;
		else
			wpagecnt = pageperunit;

		for (i = 0; i < wpagecnt; i ++) {
			if (i == 0) {
				rpl_node = rpl;
				wpl_node = wpl;
			} else {
				rpl_node = (PageList *)BuffListManager_getNextNode(
						(int)conptr->blm, (void *)rpl,sizeof(PageList));
				wpl_node = (PageList *)BuffListManager_getNextNode(
						(int)conptr->blm, (void *)wpl,sizeof(PageList));
			}

			rpl_node->startPageID = startpageid + i;
			rpl_node->OffsetBytes = 0;
			rpl_node->Bytes = byteperpage;

			wpl_node->startPageID = startpageid + i + pageperblock;
			wpl_node->OffsetBytes = 0;
			wpl_node->Bytes = byteperpage;
		}

		ret = vNand_CopyData (conptr->vnand,rpl, wpl);

		BuffListManager_freeAllList((int)conptr->blm, 
				(void **)&rpl, sizeof(PageList));
		BuffListManager_freeAllList(
				(int)conptr->blm, (void **)&wpl, sizeof(PageList));

		if(ret < 0){
			return  wpl->retVal & 0xffff;
		}
	}

	return 0;
}

static int is_firstpage_of_block(int pageid, int pageperblock)
{
	return (pageid % pageperblock == 0) ? 1 : 0;
}

static int mWrite (SmbContext *conptr, SectorList *sl_node )
{
	int ret = 0, optblockid;
	PageList *pl_top;
	BuffListManager *blm = conptr->blm;
	PPartition *pt = (PPartition *)conptr->vnand->prData;
	int pageperblock = conptr->vnand->PagePerBlock;
	int sectorperpage = conptr->vnand->BytePerPage / SECTOR_SIZE;
	
	if (!sl_node) {
		ndprint(SIGBLOCK_ERROR,"ERROR: fun %s line %d\n", __FUNCTION__, __LINE__);
		goto error;
	}

	conptr->reqblockid = 
		sl_node->startSector / sectorperpage / conptr->vnand->PagePerBlock;
retry:

	if (is_firstpage_of_block(sl_node->startSector / sectorperpage, pageperblock)) {
		optblockid = get_operator_block(conptr->vnand, conptr->reqblockid);
		ret = erase_single_block(conptr, optblockid);
		if(ret < 0){
			goto error;
		}
	}

	pl_top = (PageList *)BuffListManager_getTopNode((int)blm,sizeof(PageList));
	if (!(pl_top)) {
		ndprint(SIGBLOCK_ERROR,"ERROR: fun %s line %d\n", __FUNCTION__, __LINE__);
		ret = -1;
		goto error;
	}


	if (sectornode_to_pagelist(conptr, sl_node, pl_top) < 0) {
		ndprint(SIGBLOCK_ERROR,"bad block overrun the partition\n"); 
		ret = -1;
		BuffListManager_freeAllList(
				(int)conptr->blm, (void **)&pl_top, sizeof(PageList));
		goto error;
	}

	ret = vNand_MultiPageWrite(conptr->vnand, pl_top);
	BuffListManager_freeAllList((int)conptr->blm, 
			(void **)&pl_top, sizeof(PageList));
	if(ret < 0){
		ret = pl_top->retVal & 0xffff;
		if (ret == -5) {
			optblockid = pl_top->startPageID / conptr->vnand->PagePerBlock;
			do {
				ret = block_data_copy_to_next(conptr, optblockid, 
						pl_top->startPageID % conptr->vnand->PagePerBlock - 1);
				vNand_MarkBadBlock(conptr->vnand, optblockid);
				if (optblockid++ > pt->startblockID + pt->totalblocks) {
					ndprint(SIGBLOCK_ERROR, " more bad block in this partition.\n");
					break;
				}
			} while (ret == -5);

			if (ret < 0)
				goto error;

			goto retry;
		} else {
			ndprint(SIGBLOCK_ERROR," %s, %d retVal = %d\n",__FILE__,__LINE__,ret);
			goto error;
		}
	}

error:
	return ret;
}

int SimpleBlockManager_Open(VNandInfo *vn, PPartition *pt)
{
	SmbContext *conptr;

	if (pt->mode != DIRECT_MANAGER) {
		ndprint(SIGBLOCK_ERROR,"%s mode error\n",__func__);
		return -1;
	}

	conptr = (SmbContext *)Nand_VirtualAlloc(sizeof(SmbContext));
	conptr->mode = DIRECT_MANAGER;
	//conptr->vnand = vn; 
	conptr->vnand = (VNandInfo *)Nand_VirtualAlloc(sizeof(VNandInfo)); 
	memcpy(conptr->vnand, vn, sizeof(VNandInfo));
	conptr->vnand->prData = (int *)pt;
	conptr->blm = Blm;

	return (int)conptr;
}

int SimpleBlockManager_Close(int handle)
{
	SmbContext * conptr = (SmbContext *)handle;
	if (conptr->mode != DIRECT_MANAGER) {
		ndprint(SIGBLOCK_ERROR,"%s mode error\n",__func__);
		return -1;
	}

	Nand_VirtualFree(conptr->vnand);
	Nand_VirtualFree(conptr);

	return 0;
}


int singleblock_transfer(int context, SectorList *sl, int rwflag)
{
	SmbContext *conptr = (SmbContext *)context;
	struct singlelist *pos;
	SectorList *sl_node;
	SectorList sl_opnode;
	VNandInfo* vnand = conptr->vnand;
	int pageperblock = vnand->PagePerBlock;
	int sectorperpage = vnand->BytePerPage / SECTOR_SIZE;
	int sectorperblock = sectorperpage * pageperblock;
	int ret = 0, opblockid, i;
	int start, cnt, bufoffset,secoffset,blockcnt;

	singlelist_for_each(pos, &(sl->head)) {
		sl_node = singlelist_entry(pos, SectorList, head);
		blockcnt = get_unit_count(sl_node, sectorperblock);
		if (blockcnt == 1) {
			if (rwflag == 0) {
				ret = mWrite(conptr, sl_node);
			} else {
				ret = mRead(conptr, sl_node);
			}
			if (ret < 0)
				return ret;
		} else {
			start = sl_node->startSector;
			cnt = sl_node->sectorCount;
			bufoffset = 0;
			secoffset = sl_node->startSector % sectorperblock;
			opblockid = sl_node->startSector / sectorperblock;

			for (i = 0; i < blockcnt; i++, opblockid++) {
				if (i == 0) {
					sl_opnode.startSector = start;
					sl_opnode.sectorCount = sectorperblock - secoffset;
				} else {
					bufoffset += sl_opnode.sectorCount * SECTOR_SIZE;
					sl_opnode.startSector = opblockid * sectorperblock;
					if ((i < blockcnt - 1) || 
							((secoffset + cnt) % sectorperblock == 0))
						sl_opnode.sectorCount = sectorperblock;
					else 
						sl_opnode.sectorCount = (secoffset + cnt) % sectorperblock;

				}

				ndprint(SIGBLOCK_DEBUG,"%s:start Sec:%d,Count:%d,buffoffset:%d.\n",
						rwflag == 0 ? "write" : "read",
						sl_opnode.startSector, sl_opnode.sectorCount, 
						bufoffset / SECTOR_SIZE);
				sl_opnode.pData = (unsigned char *)(sl_node->pData) + bufoffset;
				if (rwflag == 0) {
					ret = mWrite(conptr, &sl_opnode);
				} else {
					ret = mRead(conptr, &sl_opnode);
				}
				if (ret < 0)
					return ret;
			}
		}
	}

	return ret;
}

/* *
 *	SimpleBlockManager_Read  -  Simple read operation
 *
 *	@context: current operator handle
 *	@sl: which need to read
 *
 *	Transform SectorList to PageList directly, 
 *	no use L2P cache, and don't go through NandManager.
*/
int SimpleBlockManager_Read(int context, SectorList *sl)
{
	SmbContext *conptr = (SmbContext *)context;
	int rwflag = 1;

	if (conptr->mode != DIRECT_MANAGER) {
		ndprint(SIGBLOCK_ERROR,"%s mode error\n",__func__);
		return -1;
	}

	if ( conptr->vnand == NULL) {
		ndprint(SIGBLOCK_ERROR,"%s,conptr->vnand == NULL.\n",__func__);
		return -1;
	}

	return singleblock_transfer(context, sl, rwflag);
}

/* *
 *	SimpleBlockManager_Write  - Simple write operation.
 *
 *	@context: current operator handle
 *	@sl: which need to write
 *
 *	Transform SectorList to PageList directly, 
 *	no use L2P cache, and don't go through NandManager.
*/
int SimpleBlockManager_Write(int context, SectorList *sl)
{
	SmbContext *conptr = (SmbContext *)context;
	int rwflag = 0;

	if (conptr->mode != DIRECT_MANAGER) {
		ndprint(SIGBLOCK_ERROR,"%s mode error\n",__func__);
		return -1;
	}
	if ( conptr->vnand == NULL) {
		ndprint(SIGBLOCK_ERROR,"%s,conptr->vnand == NULL.\n",__func__);
		return -1;
	}

	return singleblock_transfer(context, sl, rwflag);
}

int SimpleBlockManager_Ioctrl(int context, int cmd, int argv)
{
	SmbContext *conptr = (SmbContext *)context;

	if (conptr->mode != DIRECT_MANAGER) {
		ndprint(SIGBLOCK_ERROR,"%s mode error\n",__func__);
		return -1;
	}
	if ( conptr->vnand == NULL) {
		ndprint(SIGBLOCK_ERROR,"%s,conptr->vnand == NULL.\n",__func__);
		return -1;
	}

	return 0;
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
