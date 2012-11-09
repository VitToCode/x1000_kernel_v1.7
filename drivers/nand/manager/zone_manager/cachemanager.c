#include "clib.h"
#include "cachemanager.h"
#include "NandAlloc.h"
#include "nanddebug.h"
#include "pagelist.h"
#include "context.h"
#include "vNand.h"
#include "l2vNand.h"
#include "nandpageinfo.h"

#define L1_CACHEDATA_COUNT 1
#define L2_CACHEDATA_COUNT 4
#define L3_CACHEDATA_COUNT 32
#define L4_CACHEDATA_COUNT 32

#define calc_IndexID(start,ulen,len) (start / ((len >> 2) * ulen) * ((len >> 2) * ulen))

/**
 *	fill_infolen_unitlen - fill infolen and unitlen of cachemanager
 *
 *	@context: global variable
 */
static void fill_infolen_unitlen(int context)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonemanager = conptr->zonep;
	CacheManager *cachemanager = conptr->cachemanager;
	unsigned int lxlen;
	unsigned int ulxlen;
	cachemanager->L1InfoLen = zonemanager->L1->len;
	cachemanager->L2InfoLen = zonemanager->l2infolen;
	cachemanager->L3InfoLen = zonemanager->l3infolen;
	cachemanager->L4InfoLen = zonemanager->l4infolen;

	cachemanager->L4UnitLen = 1;
	lxlen = cachemanager->L4InfoLen;
	ulxlen = cachemanager->L4UnitLen;
	if (zonemanager->l3infolen){
		cachemanager->L3UnitLen = (lxlen / 4) * ulxlen;
		lxlen = cachemanager->L3InfoLen;
		ulxlen = cachemanager->L3UnitLen;
	}
	if (zonemanager->l2infolen){
		cachemanager->L2UnitLen = (lxlen / 4) * ulxlen;
		lxlen = cachemanager->L2InfoLen;
		ulxlen = cachemanager->L2UnitLen;
	}
	cachemanager->L1UnitLen = (lxlen / 4) * ulxlen;
	ndprint(CACHEMANAGER_INFO,"cachemanager->L1UnitLen = %d\n",cachemanager->L1UnitLen);
	ndprint(CACHEMANAGER_INFO,"cachemanager->L2UnitLen = %d\n",cachemanager->L2UnitLen);
	ndprint(CACHEMANAGER_INFO,"cachemanager->L3UnitLen = %d\n",cachemanager->L3UnitLen);
	ndprint(CACHEMANAGER_INFO,"cachemanager->L4UnitLen = %d\n",cachemanager->L4UnitLen);

	ndprint(CACHEMANAGER_INFO,"cachemanager->L1InfoLen = %d\n",cachemanager->L1InfoLen);
	ndprint(CACHEMANAGER_INFO,"cachemanager->L2InfoLen = %d\n",cachemanager->L2InfoLen);
	ndprint(CACHEMANAGER_INFO,"cachemanager->L3InfoLen = %d\n",cachemanager->L3InfoLen);
	ndprint(CACHEMANAGER_INFO,"cachemanager->L4InfoLen = %d\n",cachemanager->L4InfoLen);
}

/**
 *	newLXcache - initialize L1 cache
 *
 *	@context: global variable
 */
static CacheData *newLXcache(unsigned int *data,int infolen,int ulen)
{
	CacheData *cachedata;

	cachedata = Nand_VirtualAlloc(sizeof(CacheData));
	if(cachedata == NULL){
		ndprint(CACHEMANAGER_ERROR,"ERROR: fun %s line %d\n", __FUNCTION__, __LINE__);
		return 0;
	}

	cachedata->Index = data;
	cachedata->IndexID = 0;
	cachedata->IndexCount = infolen;
	cachedata->unitLen = ulen;
	cachedata->head.next = NULL;

	return cachedata;
}

static void deleteLXcache(CacheData *cd)
{
	Nand_VirtualFree(cd);
}

/**
 *	init_LXcache - initialize L4 cache
 *
 *	@context: global variable
 */
static CacheList *newLXcachelist(int count,int infolen,int ulen)
{
	int i;
	CacheList *cl;
	CacheData *cachedata;
	CacheData *pcachedata;

	cl = CacheList_Init();
	if(!cl){
		ndprint(CACHEMANAGER_ERROR,"ERROR: CacheList Init Failed!\n");
		return 0;
	}

	for (i = 0;i < count; i++) {
		cachedata = CacheData_Init(infolen,ulen);
		if (cachedata == NULL) {
			ndprint(CACHEMANAGER_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
			goto newLXcachelist_error;
		}

		CacheList_Insert(cl,cachedata);
	}

	return cl;

newLXcachelist_error:

	do{
		cachedata = CacheList_getTail(cl);
		pcachedata = cachedata;
		CacheData_DeInit(cachedata);
	}while(pcachedata);
	CacheList_DeInit(cl);
	return 0;
}

static void deleteLXcachelist(CacheList *cl)
{
	CacheData *cachedata;
	CacheData *pcachedata;
	do{
		cachedata = CacheList_getTail(cl);
		if (cachedata == NULL)
			break;
		pcachedata = cachedata;
		CacheData_DeInit(cachedata);
	}while(pcachedata);
	CacheList_DeInit(cl);
}

/**
 *	init_cache - initialize L1, L2 and L4 cache
 *
 *	@context: global variable
 */
static int init_L2L3L4cache(CacheManager *cm)
{
	cm->L2Info = NULL;
	cm->L3Info = NULL;
	cm->L4Info = NULL;

	if(cm->L2InfoLen){
		cm->L2Info = newLXcachelist(L2_CACHEDATA_COUNT,cm->L2InfoLen / UNIT_SIZE,cm->L2UnitLen);
		if (!cm->L2Info){
			ndprint(CACHEMANAGER_ERROR,"ERROR:l2info cache list init faild!\n");
			goto ERROR;
		}
	}
	if(cm->L3InfoLen){
		cm->L3Info = newLXcachelist(L3_CACHEDATA_COUNT,cm->L3InfoLen / UNIT_SIZE,cm->L3UnitLen);
		if (!cm->L3Info){
			ndprint(CACHEMANAGER_ERROR,"ERROR:l3info cache list init faild!\n");
			goto ERROR;
		}
	}
	cm->L4Info = newLXcachelist(L4_CACHEDATA_COUNT,cm->L4InfoLen / UNIT_SIZE,cm->L4UnitLen);
	if (!cm->L4Info){
		ndprint(CACHEMANAGER_ERROR,"ERROR:l4info cache list init faild!\n");
		goto ERROR;
	}

	return 0;
ERROR:
	if(cm->L2Info)
		deleteLXcachelist(cm->L2Info);
	if(cm->L3Info)
		deleteLXcachelist(cm->L3Info);
	if(cm->L4Info)
		deleteLXcachelist(cm->L4Info);
	return -1;
}

static void init_lct(CacheManager *cm)
{
	cm->lct.L1 = NULL;
	cm->lct.L2 = NULL;
	cm->lct.L3 = NULL;
	cm->lct.L4 = NULL;
	cm->lct.sectorid = -1;
}

/**
 *	CacheManager_Init  -  Initialize operation
 *
 *	@context: global variable
 *
 *	Apply for all cachedata and then initialize cachedata and cachelist.
 */
int CacheManager_Init ( int context )
{
	int ret;
	CacheManager *cm;
	Context *conptr = (Context *)context;

	if (conptr->cachemanager)
		return 0;

	cm = (CacheManager *)Nand_VirtualAlloc(sizeof(CacheManager));
	if (!cm) {
		ndprint(CACHEMANAGER_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		return -1;
	}
	memset(cm,0,sizeof(CacheManager));

	conptr->cachemanager = cm;

	cm->pagecache.pageinfobuf = Nand_ContinueAlloc(sizeof(unsigned char) * conptr->vnand.BytePerPage);
	if(!cm->pagecache.pageinfobuf){
		ndprint(CACHEMANAGER_ERROR,"ERROR:alloc pageinfo buf for cachemanager failed !\n");
		goto ERROR;
	}
	cm->pagecache.nandpageinfo = NULL;
	cm->pagecache.pageid = -1;
	cm->pagecache.vnand = &conptr->vnand;
	cm->pagecache.bufferlistid = (int)conptr->blm;

	fill_infolen_unitlen(context);

	cm->L1Info = NULL;
	cm->L1Info = newLXcache(conptr->l1info->page,cm->L1InfoLen / UNIT_SIZE,cm->L1UnitLen);
	if(!cm->L1Info){
		ndprint(CACHEMANAGER_ERROR,"ERROR:l1info cache list init faild!\n");
		goto ERROR;
	}

	/* init cache */
	ret = init_L2L3L4cache(cm);
	if (ret == -1)
		goto ERROR;
	/* init mutex */
	InitNandMutex(&cm->mutex);

	/* init lct */
	init_lct(cm);
	return 0;

ERROR:
	if(cm->pagecache.pageinfobuf)
		Nand_ContinueFree(cm->pagecache.pageinfobuf);
	if(cm->L1Info)
		deleteLXcache(cm->L1Info);

	Nand_VirtualFree(conptr->cachemanager);

	return -1;
}

/**
 *	CacheManager_DeInit  -  Deinit operation
 *
 *	@context: global variable
 */
void CacheManager_DeInit ( int context )
{
	Context *conptr = (Context *)context;
	CacheManager *cm = conptr->cachemanager;
	if(cm->L1Info)
		deleteLXcache(cm->L1Info);
	if(cm->L2Info)
		deleteLXcachelist(cm->L2Info);
	if(cm->L3Info)
		deleteLXcachelist(cm->L3Info);
	if(cm->L4Info)
		deleteLXcachelist(cm->L4Info);
	Nand_ContinueFree(cm->pagecache.pageinfobuf);

	DeinitNandMutex(&cm->mutex);
	Nand_VirtualFree(conptr->cachemanager);
}

#define NAND_LXOFFSET(x) ((unsigned int)&(((NandPageInfo*)0)->L##x##Info))
static unsigned char* readpageinfo(CacheManager *cm,unsigned int pageid,int lxoffset){
	unsigned char *data = NULL;
	PageList *pagelist;
	PageCache *pc = &cm->pagecache;
	VNandInfo *vnand = pc->vnand;
	unsigned char **dtmp;
	int ret = 0;
	const unsigned int lxoff[] ={NAND_LXOFFSET(2),NAND_LXOFFSET(3),NAND_LXOFFSET(4)};
	if(lxoffset < 1){
		ndprint(CACHEMANAGER_ERROR,"ERROR: lxoffset(%d) not less 1\n",lxoffset);
		while(1);
	}
	if(pc->pageid == pageid){   // hit pagecache;
		dtmp = (unsigned char **)((unsigned char *)pc->nandpageinfo + lxoff[lxoffset - 1]);
		data = *dtmp;
		return data;
	}

	pagelist = (PageList *)BuffListManager_getTopNode(pc->bufferlistid, sizeof(PageList));
	pagelist->startPageID = pageid;
	pagelist->OffsetBytes = 0;
	pagelist->Bytes = vnand->BytePerPage;
	pagelist->pData = (void*)pc->pageinfobuf;
	pagelist->retVal = 0;
	pagelist->head.next = NULL;

	ret = vNand_MultiPageRead(vnand, pagelist);
	if(ret < 0) {
		ndprint(CACHEMANAGER_ERROR,"vNand read pageinfo error func %s line %d ret = %d pageid = %d \n",
				__FUNCTION__,__LINE__,ret,pagelist->startPageID);
		pc->pageid = -1;
		goto err;
	}else{
		CONVERT_DATA_NANDPAGEINFO(pc->pageinfobuf,pc->nandpageinfo,
					  cm->L4InfoLen,cm->L3InfoLen,cm->L2InfoLen);

		if(((NandPageInfo *)pc->pageinfobuf)->MagicID != 0xaaaa)
			ndprint(ZONE_ERROR,"readpageinfo read nandpageinfo error pageid = %d MageicID = 0x%04X\n",pageid,
																			((NandPageInfo *)pc->pageinfobuf)->MagicID);
		pc->pageid = pageid;
		dtmp = (unsigned char **)((unsigned char *)pc->nandpageinfo + lxoff[lxoffset - 1]);
		data = *dtmp;
	}
err:
	BuffListManager_freeList(pc->bufferlistid, (void **)&pagelist,(void *)pagelist, sizeof(PageList));
	return data;
}

static CacheData * fillcache(CacheManager *cm,unsigned int sectorid,CacheData *src,CacheList *tar,int lxoffset){

	unsigned char *data;
	unsigned int pageid;
	unsigned int startid;
	unsigned int sectoralign;
	CacheData *cd;
	pageid = CacheData_get(src,sectorid);
	if(pageid == -1){
		return 0;
	}

	data = readpageinfo(cm,pageid,lxoffset);
	if(data == 0){
		ndprint(CACHEMANAGER_ERROR,"ERROR: read page info error! pageid = %d\n",pageid);
		pageid = -1;
		return 0;
	}
	cd = CacheList_getTail(tar);
	sectoralign = cd->IndexCount * cd->unitLen;
	startid = sectorid / sectoralign * sectoralign;
	CacheData_update(cd,startid,data);
	return cd;
}
#define GET_LX_OFFSET(cm,x) (((unsigned int)&cm->L##x##Info - (unsigned int)&cm->L1Info) / sizeof(unsigned int))
/**
 *	CacheManager_getPageID  -  Get a pageid when given a sectorid
 *
 *	@context: global variable
 *	@sectorid: number of sector given by caller
 */
#if 1
void dumpcachedate(CacheData *cd){
	int i;
	for(i = 0;i < cd->IndexCount;i++){
		if(i%8 == 0) ndprint(CACHEMANAGER_INFO,"\nI[%8d]:",cd->IndexID + i);
		ndprint(CACHEMANAGER_INFO,"%d  ",cd->Index[i]);
	}
	ndprint(CACHEMANAGER_INFO,"\n");
}
#endif
unsigned int CacheManager_getPageID ( int context, unsigned int sectorid )
{
	CacheManager *cachemanager = (CacheManager *)context;
	unsigned int pageid = -1;
	unsigned int l1page = 0;
	CacheData *cd,*ucd;
	CacheList *lx;
	int lxoffset;

	if (sectorid < 0 || sectorid > cachemanager->L1UnitLen * cachemanager->L1InfoLen >> 2) {
		ndprint(CACHEMANAGER_ERROR,"ERROR: sectorid = %d func %s line %d\n", sectorid, __FUNCTION__, __LINE__);
		return -1;
	}
	NandMutex_Lock(&(cachemanager->mutex));

	while (1) {
		pageid = -1;
		cd = CacheList_get(cachemanager->L4Info, sectorid);
		if (cd) {
			CacheList_Insert(cachemanager->L4Info, cd);
			pageid = CacheData_get(cd, sectorid);
			break;
		}
		lx = cachemanager->L4Info;
		lxoffset = GET_LX_OFFSET(cachemanager,4);
		if (cachemanager->L3InfoLen) {
			cd = CacheList_get(cachemanager->L3Info,sectorid);
			if(cd){
				ucd = fillcache(cachemanager,sectorid,cd,lx,lxoffset);
				CacheList_Insert(cachemanager->L3Info,cd);
				if(ucd){
					CacheList_Insert(lx,ucd);
					continue;
				}else break;
			}
			lx = cachemanager->L3Info;
			lxoffset = GET_LX_OFFSET(cachemanager,3);
		}
		if (cachemanager->L2InfoLen) {
			cd = CacheList_get(cachemanager->L2Info,sectorid);
			if(cd){
				ucd = fillcache(cachemanager,sectorid,cd,lx,lxoffset);
				CacheList_Insert(cachemanager->L2Info,cd);
				if(ucd){
					CacheList_Insert(lx,ucd);
					continue;
				}else break;
			}
			lx = cachemanager->L2Info;
			lxoffset = GET_LX_OFFSET(cachemanager,2);
		}
		l1page = CacheData_get(cachemanager->L1Info,sectorid);
		if (l1page != -1){
			ucd = fillcache(cachemanager,sectorid,cachemanager->L1Info,lx,lxoffset);
			if(ucd){
				CacheList_Insert(lx,ucd);
				continue;
			}else
				break;
		}

		ndprint(CACHEMANAGER_INFO,"INFO:L1Info not find this sector[%d]\n",sectorid);
		break;
	}

	NandMutex_Unlock(&(cachemanager->mutex));
	return pageid;
}
/**
 *	CacheManager_lockCache  -  lock cachedata
 *
 *	@context: global variable
 *	@sectorid: number of sector
 *	@pi: fill some information when lock operation finish
 *
 *	Get both a cachedate of L3 and L4 which the given sectorid is match it's index,
 *	and then lock these cachedata which other caller can't access.
 */
void CacheManager_lockCache ( int context, unsigned int sectorid, PageInfo **ppi )
{
	CacheManager *cachemanager = (CacheManager *)context;
	LockCacheDataTable *lct = &cachemanager->lct;
	PageInfo *pi;
	CacheList *lx;
	int lxoffset;
	CacheData **updatelx;

	pi = &cachemanager->pageinfo;
	if (sectorid < 0 || sectorid > cachemanager->L1UnitLen * cachemanager->L1InfoLen >> 2) {
		ndprint(CACHEMANAGER_ERROR,"ERROR: sectorid = %d func %s line %d\n", sectorid, __FUNCTION__, __LINE__);
		return;
	}

	NandMutex_Lock(&(cachemanager->mutex));

	lct->L1 = lct->L2 = lct->L3 = lct->L4 = NULL;
FINDDATACACHE:
	updatelx = &lct->L4;
	if(!*updatelx)
		*updatelx = CacheList_get(cachemanager->L4Info,sectorid);

	lx = cachemanager->L4Info;
	lxoffset = GET_LX_OFFSET(cachemanager,4);

	if(cachemanager->L3InfoLen){
		updatelx = &lct->L3;
		if(!*updatelx){
			*updatelx = CacheList_get(cachemanager->L3Info,sectorid);
		}
		if(*updatelx && (lct->L4 == NULL))
			lct->L4 = fillcache(cachemanager,sectorid,*updatelx,lx,lxoffset);

		lx = cachemanager->L3Info;
		lxoffset = GET_LX_OFFSET(cachemanager,3);
	}

	if(cachemanager->L2InfoLen){
		updatelx = &lct->L2;
		*updatelx = CacheList_get(cachemanager->L2Info,sectorid);
		if(*updatelx && (lct->L3 == NULL)){
			lct->L3 = fillcache(cachemanager,sectorid,*updatelx,lx,lxoffset);
			goto FINDDATACACHE;
		}
		lx = cachemanager->L2Info;
		lxoffset = GET_LX_OFFSET(cachemanager,2);
	}

	lct->L1 = cachemanager->L1Info;
	if(!*updatelx){
		*updatelx = fillcache(cachemanager,sectorid,lct->L1,lx,lxoffset);
		if(!*updatelx){
			ndprint(CACHEMANAGER_INFO,"INFO:L1Info not find sectorid[%d]\n",sectorid);
		}else
			goto FINDDATACACHE;

	}

	if(cachemanager->L2InfoLen){
		if(!lct->L2) {
			lct->L2 = CacheList_getTail(cachemanager->L2Info);
			memset(lct->L2->Index,0xff,lct->L2->IndexCount*4);
		}
		pi->L2Info = (unsigned char*)lct->L2->Index;
	}

	if(cachemanager->L3InfoLen){
		if(!lct->L3) {
			lct->L3 = CacheList_getTail(cachemanager->L3Info);
			memset(lct->L3->Index,0xff,lct->L3->IndexCount*4);
		}
		pi->L3Info = (unsigned char*)lct->L3->Index;
	}

	if(cachemanager->L4InfoLen){
		if(!lct->L4) {
			lct->L4 = CacheList_getTail(cachemanager->L4Info);
			memset(lct->L4->Index,0xff,lct->L4->IndexCount*4);
		}
		pi->L4Info = (unsigned char*)lct->L4->Index;
	}

	lct->sectorid = sectorid;

	pi->L1Info = (unsigned char*)lct->L1->Index;
	pi->L1InfoLen = cachemanager->L1InfoLen;
	pi->L2InfoLen = cachemanager->L2InfoLen;
	pi->L3InfoLen = cachemanager->L3InfoLen;
	pi->L4InfoLen = cachemanager->L4InfoLen;

	*ppi = pi;
}

/**
 *	CacheManager_unlockCache  - Unlock the cachedata
 *	which were locked in function of CacheManager_lockCache
 *
 *	@context: global variable
 *	@pi: which has L1, L2, L3 and L4 some information
 */
void CacheManager_unlockCache ( int context, PageInfo *pi )
{
	CacheManager *cachemanager = (CacheManager *)context;
	LockCacheDataTable *lct = &cachemanager->lct;


	lct->L1->IndexID = 0;
	if(cachemanager->L2InfoLen){
		lct->L2->IndexID = calc_IndexID(lct->sectorid,cachemanager->L2UnitLen,cachemanager->L2InfoLen);
		CacheList_Insert(cachemanager->L2Info,lct->L2);
	}
	if(cachemanager->L3InfoLen){
		lct->L3->IndexID = calc_IndexID(lct->sectorid,cachemanager->L3UnitLen,cachemanager->L3InfoLen);
		CacheList_Insert(cachemanager->L3Info,lct->L3);

	}

	lct->L4->IndexID = calc_IndexID(lct->sectorid,cachemanager->L4UnitLen,cachemanager->L4InfoLen);
	CacheList_Insert(cachemanager->L4Info,lct->L4);
/*
	if(lct->sectorid == 1){// < cachemanager->L1Info->unitLen){
		ndprint(CACHEMANAGER_INFO,"L1============================\n");
		dumpcachedate(cachemanager->L1Info);
		ndprint(CACHEMANAGER_INFO,"lct->L3=======================\n");
		dumpcachedate(lct->L3);
		ndprint(CACHEMANAGER_INFO,"lct->L4=======================\n");
		dumpcachedate(lct->L4);

	}
*/
	cachemanager->pagecache.pageid = -1;  //erase pagecache
	NandMutex_Unlock(&(cachemanager->mutex));
}
void CacheManager_DropCache ( int context, unsigned int *sectorid )
{
	CacheManager *cachemanager = (CacheManager *)context;
	unsigned int *data;
	LockCacheDataTable *lct = &cachemanager->lct;
	unsigned  int startsectorid;
	data = lct->L1->Index;
	data[lct->sectorid / cachemanager->L1UnitLen] = -1;
	if(cachemanager->L2InfoLen){
		data = lct->L2->Index;
		startsectorid = lct->sectorid - calc_IndexID(lct->sectorid,cachemanager->L2UnitLen,cachemanager->L2InfoLen);
		data[startsectorid / cachemanager->L2UnitLen] = -1;
	}
	if(cachemanager->L3InfoLen){
		data = lct->L3->Index;
		startsectorid = lct->sectorid - calc_IndexID(lct->sectorid,cachemanager->L3UnitLen,cachemanager->L3InfoLen);
		data[startsectorid / cachemanager->L3UnitLen] = -1;
	}

	startsectorid = calc_IndexID(lct->sectorid,cachemanager->L4UnitLen,cachemanager->L4InfoLen);
	data = lct->L4->Index;
	while(*sectorid != -1){
		data[*sectorid - startsectorid] = -1;
		sectorid++;
	}
}
static int checkldinfo(CacheData *ld,unsigned int startpageid,unsigned int count,int issector) {
	int i;
	unsigned int pageid;
	int ret = 0;
	for(i = 0;i < 512;i++) {
		if(ld->Index[i] == -1) continue;
		pageid = ld->Index[i];
		if(pageid >= startpageid * issector * 4 && pageid  < (startpageid + count) * issector * 4){
			ret = 1;
		}
	}
	return ret;
}
static int checklxinfo(CacheList *lx,unsigned int startpageid,unsigned int count,int issector) {
	struct list_head *pos;
	CacheData *cd;
	list_for_each(pos,&lx->top) {
		cd = list_entry(pos,CacheData,head);
		if(checkldinfo(cd,startpageid,count,issector))
			return 1;
	}
	return 0;
}
int CacheManager_CheckIsCacheMem ( int context,unsigned int startpageid,unsigned int count)
{
	CacheManager *cachemanager = (CacheManager *)context;
	int ret = 0;
	NandMutex_Lock(&(cachemanager->mutex));
	if(checkldinfo(cachemanager->L1Info,startpageid,count,0)){
		ret = 1;
		goto exitCheck;
	}
	if(cachemanager->L2InfoLen  && checklxinfo(cachemanager->L2Info,startpageid,count,0)) {
		ret = 1;
		goto exitCheck;
	}

	if(cachemanager->L3InfoLen  && checklxinfo(cachemanager->L3Info,startpageid,count,0)) {
		ret = 1;
		goto exitCheck;
	}
	if(cachemanager->L4InfoLen  && checklxinfo(cachemanager->L4Info,startpageid,count,1)) {
		ret = 1;
		goto exitCheck;
	}
exitCheck:
	NandMutex_Unlock(&(cachemanager->mutex));
	return ret;
}
static int checkdatainfo(unsigned int *d,int size,unsigned int startpageid,unsigned int count) {
	int i;

	for(i = 0;i < size;i++) {
		if(d[i] / 4 >= startpageid  && d[i] / 4 < startpageid + count){
			return 1;
		}
	}
	return 0;
}
static int checkreaddatainfo(CacheManager *cm,unsigned int pageid,unsigned int startpageid,unsigned int count,int layer) {
	int i;
	unsigned int *ldata;
	unsigned int *data;
	int ret = 0;
	int len = 0;

	if(layer == 1)
		len = cm->L2InfoLen;
	if(layer == 2)
		len = cm->L3InfoLen;
	if(layer == 3)
		len = cm->L4InfoLen;
	data = (unsigned int *)readpageinfo(cm,pageid,layer);
	if(data){
		if(layer == 3){
			if(checkdatainfo(data,len / 4,startpageid,count))
				return 1;
		}
		return 0;
	 }else return 0;

	ldata = Nand_VirtualAlloc(len);
	for( i = 0;i < len/4;i++)
	{
		if(data[i] == -1) continue;
		if(data[i] >= startpageid && data[i] < startpageid + count){
			ret = 1;
			break;
		}else{

			if(checkreaddatainfo(cm,pageid,startpageid,count,layer++)){
				ret = 1;
				break;			}
		}
	}
	Nand_VirtualFree(ldata);
	return ret;
}
int CacheManager_CheckCacheAll ( int context,unsigned int startpageid,unsigned int count)
{
	CacheManager *cachemanager = (CacheManager *)context;
	CacheData *ld;
	int i;
	int ret = 0;
	unsigned int pageid;
	NandMutex_Lock(&(cachemanager->mutex));

	ld = cachemanager->L1Info;
	for(i = 0;i < 512;i++) {
		if(ld->Index[i] == -1) continue;
		pageid = ld->Index[i];
		if(pageid >= startpageid && pageid < startpageid + count){
			ret = 1;
			goto exit;
		}
		if(cachemanager->L2InfoLen && checkreaddatainfo(cachemanager,pageid,startpageid,count,1)) {
			ret = 1;
			goto exit;
		}else if(cachemanager->L3InfoLen && checkreaddatainfo(cachemanager,pageid,startpageid,count,2)) {
			ret = 1;
			goto exit;
		} else if(cachemanager->L4InfoLen &&  checkreaddatainfo(cachemanager,pageid,startpageid,count,3)){
			ret = 1;
			goto exit;
		}

	}

exit:
	NandMutex_Unlock(&(cachemanager->mutex));
	return ret;
}
