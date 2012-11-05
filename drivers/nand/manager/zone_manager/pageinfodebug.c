#include "pageinfodebug.h"
#include "cachemanager.h"
#include "context.h"
#include "nanddebug.h"
struct PageInfoDebug *Init_L2p_Debug(int context) {
	CacheManager *cachemanager = ((Context *)context)->cachemanager;
	struct PageInfoDebug *pdebug;
	pdebug = (struct PageInfoDebug *)Nand_VirtualAlloc(sizeof(struct PageInfoDebug));

#define InitCacheData(x)						\
	do{								\
		if(cachemanager->L##x##InfoLen){			\
			pdebug->l##x##cache = Nand_VirtualAlloc(cachemanager->L##x##InfoLen); \
				pdebug->l##x##len = cachemanager->L##x##InfoLen / 4; \
		}else{							\
			pdebug->l##x##cache = NULL;			\
				pdebug->l##x##len = 0;			\
		}							\
		pdebug->L##x##UnitLen = cachemanager->L##x##UnitLen;	\
	}while(0)

	InitCacheData(1);
	InitCacheData(2);
	InitCacheData(3);
	InitCacheData(4);
	ndprint(1,"cachemanager->L4InfoLen  = %d\n",cachemanager->L4InfoLen); 
	pdebug->diffdata = (unsigned int *)Nand_VirtualAlloc(2048 * 4);
	pdebug->pageid = -1;
	pdebug->sectorid = (unsigned int *)Nand_VirtualAlloc(1024 + 4);
#undef InitCacheData
	return pdebug;
}

void Deinit_L2p_Debug(struct PageInfoDebug *pdebug) {
	if(pdebug->l1cache)
		Nand_VirtualFree(pdebug->l1cache);
	if(pdebug->l2cache)
		Nand_VirtualFree(pdebug->l2cache);
	if(pdebug->l3cache)
		Nand_VirtualFree(pdebug->l3cache);
	if(pdebug->l4cache)
		Nand_VirtualFree(pdebug->l4cache);
	if(pdebug->diffdata)
		Nand_VirtualFree(pdebug->diffdata);
	if(pdebug->sectorid)
		Nand_VirtualFree(pdebug->sectorid);
}

void L2p_Debug_SaveCacheData(struct PageInfoDebug *pdebug,PageInfo *pi) {

#define savedata(x) do{							\
		if(pdebug->l##x##cache) {				\
			memcpy(pdebug->l##x##cache,pi->L##x##Info,pdebug->l##x##len * 4); \
		}							\
	}while(0)
	
	savedata(1);
	savedata(2);
	savedata(3);
	savedata(4);
#undef savedata
}

void L2p_Debug_SetstartPageid(struct PageInfoDebug *pdebug,int pageid) {
	pdebug->pageid = pageid;
}
static void dump_data(unsigned int *d,int count) {
	int i;
	for(i = 0;i < count;i++){
		if(i % 16 == 0)
			ndprint(1,"\n");
		ndprint(1,"%8d\t",d[i]);
	}
	ndprint(1,"\n");
}


void L2p_Debug_CheckData(struct PageInfoDebug *pdebug,PageInfo *pi,int count) {

	unsigned int *l1info,*l2info,*l3info,*l4info;
	int errorcount = 0;
	int n;
#define checkdata(x) do{						\
		int i;							\
		for(i = 0;i < pdebug->l##x##len;i++) {			\
			if(l##x##info[i] != pdebug->l##x##cache[i]){	\
				if(x == 4)				\
					pdebug->diffdata[errorcount] = l##x##info[i] / 4; \
					else				\
						pdebug->diffdata[errorcount] = l##x##info[i]; \
							errorcount++;	\
			}						\
		}							\
		pdebug->diffdata[errorcount] = 0;			\
	}while(0)

	l1info = (unsigned int *)pi->L1Info;
	l2info = (unsigned int *)pi->L2Info;
	l3info = (unsigned int *)pi->L3Info;
	l4info = (unsigned int *)pi->L4Info;


	checkdata(1);
	checkdata(2);
	checkdata(3);
	checkdata(4);

	for(n = 0;n < errorcount;n++) {
		if ((pdebug->diffdata[n] < pdebug->pageid) || (pdebug->diffdata[n] >= pdebug->pageid + count)) {
			ndprint(1,"count:%d diffdata[%d]:%d pdebug->pageid:%d errcount:%d\n",count,n,pdebug->diffdata[n],pdebug->pageid,errorcount);
			break;
		}
	}
	if(n < errorcount) {
		ndprint(1,"pageid = %d\n",pdebug->pageid);
		ndprint(1,"error: new pageinfo\n");
		if(pdebug->l1cache){
			ndprint(1,"l1 pageinfo:\n");
			dump_data(l1info,pdebug->l1len);
		}
		if(pdebug->l2cache){
			ndprint(1,"l2 pageinfo:\n");
			dump_data(l2info,pdebug->l2len);
		}
		if(pdebug->l3cache){
			ndprint(1,"l3 pageinfo:\n");
			dump_data(l3info,pdebug->l3len);
		}
		if(pdebug->l4cache){
			ndprint(1,"l4 pageinfo:\n");
			dump_data(l4info,pdebug->l4len);
		}
		ndprint(1,"error: pageinfo\n");
		if(pdebug->l1cache){
			ndprint(1,"l1 pageinfo:\n");
			dump_data(pdebug->l1cache,pdebug->l1len);
		}
		if(pdebug->l2cache){
			ndprint(1,"l2 pageinfo:\n");
			dump_data(pdebug->l2cache,pdebug->l2len);
		}
		if(pdebug->l3cache){
			ndprint(1,"l3 pageinfo:\n");
			dump_data(pdebug->l3cache,pdebug->l3len);
		}
		if(pdebug->l4cache){
			ndprint(1,"l4 pageinfo:\n");
			dump_data(pdebug->l4cache,pdebug->l4len);
		}
		ndprint(1,"error: diffpageinfo\n");

		dump_data(pdebug->diffdata,errorcount);
	}




#define savedata(x) do{							\
		if(pdebug->l##x##cache) {				\
			memcpy(pdebug->l##x##cache,pi->L##x##Info,pdebug->l##x##len * 4); \
		}							\
	}while(0)
	savedata(1);
	savedata(2);
	savedata(3);
	savedata(4);
#undef savedata
}

void L2p_Debug_SaveSectorList(struct PageInfoDebug *pdebug,SectorList *sl) {
	struct singlelist *pos;
	SectorList *sl_node;
	int i;
	int count = 0;

	singlelist_for_each(pos, &(sl->head)) {
		sl_node = singlelist_entry(pos, SectorList, head);
		for(i = 0; i < sl_node->sectorCount / SECTOR_SIZE;i++) {
			pdebug->sectorid[count++] = sl_node->startSector + i * SECTOR_SIZE;
		}
	}
	for(;count < 256;count++)
		pdebug->sectorid[count] = -1;
}
#define checklxdata(x)					\
	({						\
		int i,n = -1;				\
		for(i = 0;i < pdebug->l##x##len;i++) {	     \
			if(l##x##info[i] != pdebug->l##x##cache[i]){	\
				if(x == 4)				\
					pdebug->diffdata[errorcount] = l##x##info[i] / 4; \
					else				\
						pdebug->diffdata[errorcount] = l##x##info[i]; \
							errorcount++;	\
							if(n == -1) n = i; \
			}else {						\
				if(n != -1) break;			\
			}						\
		}							\
		pdebug->diffdata[errorcount] = 0;			\
		n;							\
	})


void L2p_Debug_RemoveSectorID(struct PageInfoDebug *pdebug,PageInfo *pi){
	unsigned int *l1info,*l2info,*l3info,*l4info;
	int errorcount = 0;
	int pos = -1,n;
	int sectorid = 0;
	int c_sectorcount;
	l1info = (unsigned int *)pi->L1Info;
	l2info = (unsigned int *)pi->L2Info;
	l3info = (unsigned int *)pi->L3Info;
	l4info = (unsigned int *)pi->L4Info;
#define CalSectorid(x)						\
	do{							\
		if(pdebug->L##x##UnitLen) {				\
			errorcount = 0;					\
			pos = checklxdata(x);				\
			sectorid += pos * pdebug->L##x##UnitLen;	\
		}							\
	}while(0)

	CalSectorid(1);
	CalSectorid(2);
	CalSectorid(3);
	do {
		CalSectorid(4);
		if(pos != -1) {
			c_sectorcount = 0;
			while(pdebug->diffdata[c_sectorcount++]);

			for(n = 0;n < 256;n++) {
				if(pdebug->sectorid[n] <= pos &&  pdebug->sectorid[n] > pos + c_sectorcount) {
					pdebug->sectorid[n] = -1;
				}
			}
		}
	}while(pos != -1);
}

int L2p_Debug_CheckSectorID(struct PageInfoDebug *pdebug){
	int i;
	int ret = 0;
	for(i = 0;i < 256;i++) {
		if(pdebug->sectorid[i] != -1) {
			ndprint(1,"no write to nand %d\n",pdebug->sectorid[i]);
			ret = -1;
		}
	}
	return -1;
}
