#include "os/clib.h"
#include "FileDesc.h"
#include "nandmanagerinterface.h"
#include "bufflistmanager.h"
#include "os/NandAlloc.h"
#include "vNand.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
struct argoption arg_option={0};

void help()
{
	printf("help");
}

static void start(int handle){
	LPartition* lp,*lpentry;
	struct singlelist *it;
	int pHandle;
	FILE *fp;
	unsigned char *buf;
	int rlen;
	int bl;
	SectorList *sl;
	int sectorid;
	int last_sectorcount;

	NandManger_getPartition(handle,&lp);
	singlelist_for_each(it,&lp->head){
		lpentry = singlelist_entry(it,LPartition,head);
		pHandle = NandManger_ptOpen(handle,lpentry->name,lpentry->mode);
		if(pHandle)
			break;
		else {
			printf("NandManger open failed!\n");
			exit(1);
		}
	}
	buf = (unsigned char*)malloc(256*512);
	if(buf == NULL){
		printf("malloc failed!\n");
		exit(1);
	}
	bl = BuffListManager_BuffList_Init();
	if(bl == 0){
		printf("BuffListManager Init failed!\n");
		exit(1);
	}
	fp = fopen(arg_option.file_desc.inname,"rb");
	if(fp == NULL){
		printf("read infile failed!\n");
		exit(1);
	}

	sectorid = 0;
	while(!feof(fp)){
		rlen = fread(buf,1,256*512,fp);
		if(rlen){
			sl = BuffListManager_getTopNode(bl,sizeof(SectorList));
			if(sl == 0){
				printf("Bufferlist request sectorlist failed!\n");
				exit(1);
			}
			sl->startSector = sectorid;
			sl->pData = buf;
			sl->sectorCount = (rlen + 511)/ 512;
			sectorid += (rlen + 511)/ 512;

			if(rlen < 256*512){
				memset(buf+ rlen, 0xff, 512*256 - rlen);
			}
			if(NandManger_ptWrite(pHandle,sl) < 0){
				printf("NandManger write failed!\n");
				exit(1);
			}
			last_sectorcount = sl->sectorCount;
			BuffListManager_freeAllList(bl,(void **)&sl,sizeof(SectorList));
		}


	}
	fclose(fp);
	printf("end write\n");

	if (arg_option.debug) {
		int sum_sectorid = sectorid - last_sectorcount;
		int i = 0;
		fp = NULL;
		FILE *w_fp;

		w_fp = fopen("debug.img", "w+");
		if( w_fp == NULL){
			printf("open debug.img failed!\n");
			exit(1);
		}

		bl = BuffListManager_BuffList_Init();
		if(bl == 0){
			printf("BuffListManager Init failed!\n");
			exit(1);
		}
		sectorid = 0;
		for(i=0; i < sum_sectorid; i+=256){

			sl = BuffListManager_getTopNode(bl,sizeof(SectorList));
			if(sl == 0){
				printf("Bufferlist request sectorlist failed!\n");
				exit(1);
			}

			sl->startSector = sectorid;
			sl->sectorCount = 256;
			sl->pData = buf;
			sectorid += 256;
			if(NandManger_ptRead(pHandle, sl) < 0){
				printf("NandManger read failed!\n");
				exit(1);
			}
			rlen = fwrite(buf,1,256*512,w_fp);
			if(i == sum_sectorid - 256){

					sl->pData = buf;
					sl->startSector = sectorid;

					sl->sectorCount = last_sectorcount;
					if(NandManger_ptRead(pHandle, sl) < 0){
						printf("NandManger read failed!\n");
						exit(1);
					}
					rlen = fwrite(buf, 1, last_sectorcount*512, w_fp);
					printf("last copy sectorCount rlen = %d\n", rlen);

			}
			BuffListManager_freeAllList(bl,(void **)&sl,sizeof(SectorList));
		}
		fclose(w_fp);
	}

	NandManger_ptClose(pHandle);
	free(buf);
	BuffListManager_BuffList_DeInit(bl);
	printf("end read\n");
}

/*
   1--64 info
   3--pageperblock
   8 block -- 1 zone
 */
static int caloutfilelen(int len){
	int blocklen = arg_option.file_desc.bytesperpage * arg_option.file_desc.pageperblock;
	int zonelen = blocklen * 8;
	int trimzoneinfolen = zonelen - 8 * 3 * arg_option.file_desc.bytesperpage;
	int realzonelen = trimzoneinfolen * 63 / 64;
	int l;
	l = (len + realzonelen - 1) / realzonelen * blocklen * 8;
	printf("l = %d\n",l);
	return l;
}
int main(int argc, char *argv[])
{
	int pHandle;
	int preblocks;

	arg_option.debug = 0;
	get_option(argc,argv);
	dumpconfig();
	if(arg_option.file_desc.length <= 0){
		printf("the len of inputfile shouldn't be zero!\n");
		return -1;
	}
	preblocks = (
		caloutfilelen(arg_option.file_desc.length) +
		arg_option.file_desc.bytesperpage * arg_option.file_desc.pageperblock - 1)
		/ (arg_option.file_desc.bytesperpage * arg_option.file_desc.pageperblock);
	if(preblocks >= arg_option.file_desc.blocks){
		printf("There isn't enough block for it!");
		exit(1);
	}
	if(arg_option.file_desc.blocks == 0){
		printf("Block count shouldn't be zero!");
		exit(1);
	}
	printf("%s(%s) %d arg_option.file_desc.blocks = %d\n",__FUNCTION__,__FILE__,__LINE__,arg_option.file_desc.blocks);

	pHandle = NandManger_Init();

	NandManger_startNotify(pHandle,start,pHandle);

	ND_probe(&arg_option.file_desc);
	NandManger_DeInit(pHandle);
    return 0;
}
