#include "clib.h"
#include "pagelist.h"
#include "blocklist.h"
#include "ppartition.h"
#include "nandinterface.h"
#include "vnandinfo.h"
#include "nanddebug.h"

#define MAXALOWBADBLOCK (128*8*4 -1)
struct badblock {
	int blockid;
	int isbad;
};
static struct badblock bblocks[MAXALOWBADBLOCK];

struct Nand2K
{
    int PagePerBlock;
    int BytePerPage;
    int TotalBlocks;
	int MaxBadBlockCount;
} vNandChipInfo = {64,2048,128*8*4,20}; //512M


PPartition ppt[] = {{"x-boot",128*8*0,64,2048,128*8,20,512,0,64*128*8,1, NULL},{"kernel",128*8*1,64,2048,128*8,20,512,0,64*128*8,1, NULL},{"ubifs",128*8*2,64,2048,128*8,20,512,0,64*128*8,0, NULL},{"data",128*8*3,64,2048,128*8,20,512,0,64*128*8,1, NULL},{"error",128*8*4,64,2048,1,20,512,0,64,2, NULL}};
PPartArray partition={5,ppt};

struct vNand2K
{
	FILE *fp;
	int *pagebuf;
	struct Nand2K *nand;
};


static int em_vNand_InitNand (void *vd ){
	FILE *fp;
	int *buf;
	int i,n;
    	VNandManager* vNand = vd;
	char ptname[256];
	struct vNand2K *vNandChip;
	unsigned int spare = -1;
	
	vNand->info.PagePerBlock = vNandChipInfo.PagePerBlock; 
	vNand->info.BytePerPage = vNandChipInfo.BytePerPage;
	vNand->info.TotalBlocks = vNandChipInfo.TotalBlocks;
	vNand->info.MaxBadBlockCount = vNandChipInfo.MaxBadBlockCount;
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
	snprintf(ptname,256,"%s.bin","nand");
	fp = fopen(ptname,"rb");
	if(fp == NULL)
	{
		fp = fopen(ptname,"w");
		if(fp == NULL)
		{
			printf("open %s  error func %s line %d \n",ptname,__FUNCTION__,__LINE__);
			return -1;
		}
		for(i = 0;i < vNandChipInfo.PagePerBlock * vNandChipInfo.TotalBlocks;i++){
			fwrite(buf,1,vNandChipInfo.BytePerPage,fp);
			fwrite(&spare,4,1,fp);
		}
	}
	fclose(fp);
	vNandChip->fp = fopen(ptname,"r+");
	vNandChip->pagebuf = buf;

	vNandChip->nand = &vNandChipInfo;
		

	for(n = 0;n < partition.ptcount;n++){	
		partition.ppt[n].prData = vNandChip;		
	}
	
	printf("vNandChip %p\n",vNandChip);
	printf("vNandChip->nand %p\n",vNandChip->nand);
	
	for (i = 0; i < MAXALOWBADBLOCK; i++) {
		bblocks[i].blockid = i;
		bblocks[i].isbad = 0;
	}

	return 0;
}

static size_t page2offset(struct Nand2K *vNand,int pageid,int start)
{
	return pageid * (vNand->BytePerPage + 4) + start * (vNand->BytePerPage + 4) * vNand->PagePerBlock;
}

static size_t block2offset(struct Nand2K *vNand,int blockid,int start)
{
	return (blockid + start)* (vNand->BytePerPage + 4) * vNand->PagePerBlock;
}

static int em_vNand_PageRead (void *pt,int pageid, int offsetbyte, int bytecount, void * data ){

	struct vNand2K *p = (struct vNand2K *)PPARTITION(pt)->prData;
	int startblock = PPARTITION(pt)->startblockID;
	int val;
	unsigned int spare;
	fseek(p->fp,page2offset(p->nand,pageid,startblock) + offsetbyte,SEEK_SET);
	val = fread(data,1,bytecount,p->fp);
	fread(&spare,4,1,p->fp);
	if(spare != 0)
		return -6;
	return val;
	 
}

static int em_vNand_PageWrite (void *pt,int pageid, int offsetbyte, int bytecount, void* data ){
	struct vNand2K *p = (struct vNand2K *)PPARTITION(pt)->prData;
	int startblock = PPARTITION(pt)->startblockID;
	unsigned int spare = 0;
	int val;
	fseek(p->fp,page2offset(p->nand,pageid,startblock) + offsetbyte,SEEK_SET);
	val  = fwrite(data,1,bytecount,p->fp);
	fwrite(&spare,4,1,p->fp);
	return val;
}

static int em_vNand_MultiPageRead (void *pt,PageList* pl ){
	struct vNand2K *p = (struct vNand2K *)PPARTITION(pt)->prData;
	int startblock = PPARTITION(pt)->startblockID;
	struct singlelist *sg;
	unsigned int spare;
   	do{
		if(pl->startPageID == -1)
			return -1;
		fseek(p->fp,page2offset(p->nand,pl->startPageID,startblock) + pl->OffsetBytes,SEEK_SET);
		pl->retVal = fread(pl->pData,1,pl->Bytes,p->fp);
		fread(&spare,4,1,p->fp);
		fsync((int)p->fp);
		if(pl->retVal <= 0)
		{
			perror("error::");
			return -1;
		}
		if(spare != 0)
		{
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
	struct vNand2K *p = (struct vNand2K *)PPARTITION(pt)->prData;
	struct singlelist *sg;
	unsigned int spare = 0;
	int startblock = PPARTITION(pt)->startblockID;
   	do{		
		fseek(p->fp,page2offset(p->nand,pl->startPageID,startblock) + pl->OffsetBytes,SEEK_SET);
		
		pl->retVal = fwrite(pl->pData,1,pl->Bytes,p->fp);
		fwrite(&spare,4,1,p->fp);
		fsync((int)p->fp);
		if(pl->retVal <= 0)
		{
			perror("error::");
			return -1;	
		}	
		sg = (pl->head).next;
		if(sg == NULL)
			break;

		pl = singlelist_entry(sg,PageList,head);
	}while(pl);
	return 0;
}

static int em_vNand_MultiBlockErase (void *pt,BlockList* pl ){
	struct vNand2K *p = (struct vNand2K *)PPARTITION(pt)->prData;
	int i,j;
	int ret = -1;
	struct singlelist *sg;
	int startblock = PPARTITION(pt)->startblockID;
	unsigned int spare = -1;
	memset(p->pagebuf,0xff,p->nand->BytePerPage);
	do{
		fseek(p->fp,block2offset(p->nand,pl->startBlock,startblock),SEEK_SET);
		for(i = 0;i < pl->BlockCount;i++){
			for(j = 0;j < p->nand->PagePerBlock;j++){
				ret = fwrite(p->pagebuf,1,p->nand->BytePerPage,p->fp);
				fwrite(&spare,4,1,p->fp);
				pl->retVal = 0;
				if(ret <= 0){
					pl->retVal = -1;
					break;
				}
			}
			if(pl->retVal < 0)
				return -1;
		}
		sg = (pl->head).next;
		if(sg == NULL)
			break;
		pl = singlelist_entry(sg,BlockList,head);
	}while(pl);
	
	return ret;
}

static int em_vNand_IsBadBlock (void *pt,int blockid ){
	
	if (blockid < MAXALOWBADBLOCK) {
		return bblocks[blockid].isbad;
	}

	return 0;

	struct vNand2K *p = (struct vNand2K *)PPARTITION(pt)->prData;
	int startblock = PPARTITION(pt)->startblockID;
	
	fseek(p->fp,block2offset(p->nand,blockid,startblock),SEEK_SET);
	if(fread(p->pagebuf,1,p->nand->BytePerPage,p->fp) <= 0)
		return -1;
	if(p->pagebuf[p->nand->BytePerPage / 4 - 1] == 0)
		return -1;
	return 0;
	
}

static int em_vNand_MarkBadBlock (void *pt,unsigned int blockid ){

	if (blockid < MAXALOWBADBLOCK) {
		bblocks[blockid].isbad = 1;
		ndprint(1,"mark badblock : %d\n", blockid);
	}

#if 0
	struct vNand2K *p = (struct vNand2K *)PPARTITION(pt)->prData;
	int startblock = PPARTITION(pt)->startblockID;
	
	fseek(p->fp,block2offset(p->nand,blockid,startblock),SEEK_SET);
	memset(p->pagebuf,0,p->nand->BytePerPage);
	if(fwrite(p->pagebuf,1,p->nand->BytePerPage,p->fp) <= 0)
		return -1;
#endif
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
	Register_NandDriver(&em_nand_ops);	
}
void ND_Exit(void){
}
