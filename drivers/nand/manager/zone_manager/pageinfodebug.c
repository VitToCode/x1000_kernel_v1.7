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
		if(cachemanager->L##x##UnitLen){			\
			pdebug->l##x##cache = Nand_VirtualAlloc(cachemanager->L##x##UnitLen * 4); \
				pdebug->l##x##len = cachemanager->L##x##UnitLen; \
		}else{							\
			pdebug->l##x##cache = NULL;			\
				pdebug->l##x##len = 0;			\
		}							\
	}while(0)

	InitCacheData(1);
	InitCacheData(2);
	InitCacheData(3);
	InitCacheData(4);
	pdebug->diffdata = (unsigned int *)Nand_VirtualAlloc(2048);
	pdebug->pageid = -1;
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
}

void L2p_Debug_SaveCacheData(struct PageInfoDebug *pdebug,PageInfo *pi) {

#define savedata(x) do{							\
		if(pdebug->l##x##cache) {				\
			memcpy(pdebug->l##x##cache,pi->L##x##Info,pdebug->l##x##len); \
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

	l1info = (unsigned int *)pi->L1Info;
	l2info = (unsigned int *)pi->L2Info;
	l3info = (unsigned int *)pi->L3Info;
	l4info = (unsigned int *)pi->L4Info;

#define checkdata(x) do{						\
		int i;							\
		for(i = 0;i < pdebug->l##x##len;i++) {			\
			if(l##x##info[i] != pdebug->l##x##cache[i]){	\
				pdebug->diffdata[errorcount] = l##x##info[i]; \
					errorcount++;			\
			}						\
		}							\
		pdebug->diffdata[errorcount] = 0;			\
	}while(0)

	checkdata(1);
	checkdata(2);
	checkdata(3);
	checkdata(4);

	for(n = 0;n < errorcount;n++) {
		if ((pdebug->diffdata[n] < pdebug->pageid) || (pdebug->diffdata[n] >= pdebug->pageid + count)) {
			break;
		}
	}
	if(n < errorcount) {
		ndprint(1,"pageid = %d\n",pdebug->pageid);
		ndprint(1,"error: old pageinfo\n");
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
		if(pdebug->l1cache){
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

	}




#define savedata(x) do{							\
		if(pdebug->l##x##cache) {				\
			memcpy(pdebug->l##x##cache,pi->L1Info,pdebug->l##x##len); \
		}							\
	}while(0)
	savedata(1);
	savedata(2);
	savedata(3);
	savedata(4);
#undef savedata
}
