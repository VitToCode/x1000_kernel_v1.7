#include "clib.h"
#include "pagelist.h"
#include "blocklist.h"
#include "ppartition.h"
#include "nandinterface.h"
#include "vnandinfo.h"
#include "nanddebug.h"
#include "FileDesc.h"

struct Nand2K
{
    int PagePerBlock;
    int BytePerPage;
    int TotalBlocks;
	int MaxBadBlockCount;
} vNandChipInfo = {128,4096,128*4*2,20}; //512M


PPartition ppt[] = {{"ndsystem",64*2,128,4096,128*4*2,20,512,64*256,512*256,1, NULL, NULL}};
PPartArray partition={1,ppt};

struct vNand2K
{
	FILE *fp;
	int *pagebuf;
	struct Nand2K *nand;
};


static int em_vNand_InitNand (void *vd ){
	int *buf;
	int n;
	VNandManager* vNand = vd;
	char ptname[256];
	struct vNand2K *vNandChip;
	
	vNand->info.PagePerBlock = vNandChipInfo.PagePerBlock; 
	vNand->info.BytePerPage = vNandChipInfo.BytePerPage;
	vNand->info.TotalBlocks = vNandChipInfo.TotalBlocks;
	vNand->info.MaxBadBlockCount = vNandChipInfo.MaxBadBlockCount;
	printf("vNand->info.PagePerBlock = %d\n",vNand->info.PagePerBlock);
	printf("vNand->info.BytePerPage = %d\n",vNand->info.BytePerPage);
	printf("vNand->info.TotalBlocks = %d\n",vNand->info.TotalBlocks);
	vNand->pt = &partition;
	vNand->info.hwSector = 512;
	vNand->info.startBlockID = 0;

	vNandChip = (struct vNand2K *)malloc(sizeof(struct vNand2K));
	
	if(vNandChip == NULL){
		printf("alloc memory error func %s line %d \n",__FUNCTION__,__LINE__);
		return -1;
	}
	buf = (int *)malloc(vNandChipInfo.BytePerPage);
	memset(buf,0xff,vNandChipInfo.BytePerPage);
	if(buf == NULL)
	{
		printf("alloc memory error func %s line %d \n",__FUNCTION__,__LINE__);
		return -1;
	}	
	snprintf(ptname,256,"%s.bin",ppt[0].name);

	vNandChip->fp = fopen(ptname,"w+");
	vNandChip->pagebuf = buf;
	vNandChip->nand = &vNandChipInfo;
		

	for(n = 0;n < partition.ptcount;n++){	
		partition.ppt[n].prData = vNandChip;		
	}
	
	printf("vNandChip %p\n",vNandChip);
	printf("vNandChip->nand %p\n",vNandChip->nand);
	
	return 0;
}

static size_t page2offset(struct Nand2K *vNand,int pageid,int start)
{
	return pageid * vNand->BytePerPage + start * vNand->BytePerPage * vNand->PagePerBlock;
}

static size_t block2offset(struct Nand2K *vNand,int blockid,int start)
{
	return (blockid + start)* vNand->BytePerPage * vNand->PagePerBlock;
}

static int em_vNand_PageRead (void *pt,int pageid, int offsetbyte, int bytecount, void * data ){

	struct vNand2K *p = (struct vNand2K *)PPARTITION(pt)->prData;
	int startblock = PPARTITION(pt)->startblockID;
	
	fseek(p->fp,page2offset(p->nand,pageid,startblock) + offsetbyte,SEEK_SET);
	return fread(data,1,bytecount,p->fp);
	 
}

static int em_vNand_PageWrite (void *pt,int pageid, int offsetbyte, int bytecount, void* data ){
printf("%s(%s) %d\n",__FUNCTION__,__FILE__,__LINE__);

	struct vNand2K *p = (struct vNand2K *)PPARTITION(pt)->prData;
printf("%s(%s) %d\n",__FUNCTION__,__FILE__,__LINE__);

	int startblock = PPARTITION(pt)->startblockID;
printf("%s(%s) %d\n",__FUNCTION__,__FILE__,__LINE__);

	fseek(p->fp,page2offset(p->nand,pageid,startblock) + offsetbyte,SEEK_SET);
printf("%s(%s) %d\n",__FUNCTION__,__FILE__,__LINE__);

	return fwrite(data,1,bytecount,p->fp);
}

static int em_vNand_MultiPageRead (void *pt,PageList* pl ){
	struct vNand2K *p = (struct vNand2K *)PPARTITION(pt)->prData;
	int startblock = PPARTITION(pt)->startblockID;
	struct singlelist *sg;
   	do{
		if(pl->startPageID == -1)
			return -1;
		fseek(p->fp,page2offset(p->nand,pl->startPageID,startblock) + pl->OffsetBytes,SEEK_SET);
		pl->retVal = fread(pl->pData,1,pl->Bytes,p->fp);
		if(pl->retVal <= 0)
		{
			perror("error::");
			pl->retVal = -6;
			return -6;
		}	
		sg = (pl->head).next;
		if(sg == NULL)
			break;

		pl = singlelist_entry(sg,PageList,head);
	}while(pl);
	return 0;
}

static int em_vNand_MultiPageWrite (void *pt,PageList* pl ){
	printf("%s(%s) %d\n",__FUNCTION__,__FILE__,__LINE__);
	
	struct vNand2K *p = (struct vNand2K *)PPARTITION(pt)->prData;
	struct singlelist *sg;
	int startblock = PPARTITION(pt)->startblockID;
   	printf("%s(%s) %d\n",__FUNCTION__,__FILE__,__LINE__);
	
	do{		
		printf("%s(%s) %d\n",__FUNCTION__,__FILE__,__LINE__);
		
		fseek(p->fp,page2offset(p->nand,pl->startPageID,startblock) + pl->OffsetBytes,SEEK_SET);
		printf("%s(%s) %d %p %d\n",__FUNCTION__,__FILE__,__LINE__,pl->pData,pl->Bytes);
		
		pl->retVal = fwrite(pl->pData,1,pl->Bytes,p->fp);
		printf("%s(%s) %d\n",__FUNCTION__,__FILE__,__LINE__);
		fsync((int)p->fp);
		if(pl->retVal <= 0)
		{
			perror("error::");
			return -1;	
		}	
		printf("%s(%s) %d\n",__FUNCTION__,__FILE__,__LINE__);
		sg = (pl->head).next;
		if(sg == NULL)
			break;
		printf("%s(%s) %d\n",__FUNCTION__,__FILE__,__LINE__);
		pl = singlelist_entry(sg,PageList,head);
	}while(pl);
printf("%s(%s) %d\n",__FUNCTION__,__FILE__,__LINE__);

	return 0;
}

static int em_vNand_MultiBlockErase (void *pt,BlockList* pl ){
	struct vNand2K *p = (struct vNand2K *)PPARTITION(pt)->prData;
	int i;
	int ret = -1;
	struct singlelist *sg;
	int startblock = PPARTITION(pt)->startblockID;
	
	memset(p->pagebuf,0xff,p->nand->BytePerPage);
	do{
		fseek(p->fp,block2offset(p->nand,pl->startBlock,startblock),SEEK_SET);
		for(i = 0;i < pl->BlockCount;i++){
			pl->retVal = 0;
		}
		sg = (pl->head).next;
		if(sg == NULL)
			break;
		pl = singlelist_entry(sg,BlockList,head);
	}while(pl);
	
	return ret;
}

static int em_vNand_IsBadBlock (void *pt,int blockid ){
	printf("%s(%s) %d blockid = %d\n",__FUNCTION__,__FILE__,__LINE__,blockid);
	return 0;
	
}

static int em_vNand_MarkBadBlock (void *pt, int blockid ){
	return 0;
}

static int em_vNand_DeInitNand (void *vd){
	VNandManager* vNand = vd;
	struct vNand2K *p = (struct vNand2K *)PPARTITION(&vNand->pt->ppt[0])->prData;	
	fclose(p->fp);
	free(p->pagebuf);
	free(p);
	return 0;
}

NandInterface em_nand_ops = {
	.iInitNand = em_vNand_InitNand,
	.iPageRead = em_vNand_PageRead,
	.iPageWrite = em_vNand_PageWrite,
	.iMultiPageRead = em_vNand_MultiPageRead,
	.iMultiPageWrite = em_vNand_MultiPageWrite,
	.iMultiBlockErase = em_vNand_MultiBlockErase,
	.iIsBadBlock = em_vNand_IsBadBlock,
	.iDeInitNand = em_vNand_DeInitNand,
	.iMarkBadBlock = em_vNand_MarkBadBlock,
};


void ND_Init(void){

}
void ND_Exit(void){
}

void ND_probe(struct filedesc *file_desc){

	vNandChipInfo.PagePerBlock = file_desc->pageperblock;
	vNandChipInfo.BytePerPage = file_desc->bytesperpage;
	vNandChipInfo.TotalBlocks = file_desc->blocks;
	vNandChipInfo.MaxBadBlockCount = 0;
	printf("%s(%s) %d file_desc->blocks = %d vNandChipInfo.BytePerPage = %d\n",__FUNCTION__,__FILE__,__LINE__,file_desc->blocks,vNandChipInfo.BytePerPage);
	
	vNandChipInfo.MaxBadBlockCount = 0;
	ppt[0].name = file_desc->outname;
	ppt[0].startblockID = 0;
	ppt[0].pageperblock = vNandChipInfo.PagePerBlock;
	ppt[0].byteperpage = vNandChipInfo.BytePerPage;
	ppt[0].totalblocks = vNandChipInfo.TotalBlocks;
	ppt[0].badblockcount = vNandChipInfo.MaxBadBlockCount;
	ppt[0].hwsector = 512;
	ppt[0].startPage = 0 * vNandChipInfo.PagePerBlock;
	ppt[0].PageCount = vNandChipInfo.TotalBlocks * vNandChipInfo.PagePerBlock;
	ppt[0].mode = ZONE_MANAGER;
	ppt[0].prData = NULL;
	printf("ppt[0].pageperblock = %d\n",ppt[0].pageperblock);
	printf("ppt[0].byteperpage = %d\n",ppt[0].byteperpage);
	printf("ppt[0].totalblocks = %d\n",ppt[0].totalblocks);
	Register_NandDriver(&em_nand_ops);
}
