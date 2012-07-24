#include <string.h>
#include "cachemanager.h"
#include "NandAlloc.h"
#include "nanddebug.h"
#include "pagelist.h"
#include "context.h"
#include "vNand.h"
#include "nandpageinfo.h"

#define L1_CACHEDATA_COUNT 1
#define L2_CACHEDATA_COUNT 1
#define L3_CACHEDATA_COUNT 32
#define L4_CACHEDATA_COUNT 32

/**
 *	fill_infolen_unitlen - fill infolen and unitlen of cachemanager
 *
 *	@context: global variable
 */
static void fill_infolen_unitlen(int context)
{
	Context *conptr = (Context *)context;
	ZoneManager *zonemanager = conptr->zonep;
	VNandInfo *vnand = &conptr->vnand;
	CacheManager *cachemanager = conptr->cachemanager;
	
	cachemanager->L1InfoLen = vnand->BytePerPage;
	cachemanager->L2InfoLen = zonemanager->l2infolen;
	cachemanager->L3InfoLen = zonemanager->l3infolen;
	cachemanager->L4InfoLen = zonemanager->l4infolen;
	cachemanager->L1UnitLen = vnand->TotalBlocks * vnand->PagePerBlock * vnand->BytePerPage
		/ (vnand->BytePerPage / UNIT_SIZE) / SECTOR_SIZE;

	if (zonemanager->l2infolen)
		cachemanager->L2UnitLen = cachemanager->L1UnitLen 
		/ (zonemanager->l2infolen / UNIT_SIZE);
	else
		cachemanager->L2UnitLen = 0;
	
	if (zonemanager->l3infolen) {
		if (zonemanager->l2infolen)
			cachemanager->L3UnitLen = cachemanager->L2UnitLen 
			/ (zonemanager->l3infolen / UNIT_SIZE);
		else
			cachemanager->L3UnitLen = cachemanager->L1UnitLen 
			/ (zonemanager->l3infolen / UNIT_SIZE);
	}
	else
		cachemanager->L3UnitLen = 0;
	
	cachemanager->L4UnitLen = 1;
}

/**
 *	init_L1cache - initialize L1 cache
 *
 *	@context: global variable
 */
static void init_L1cache(int context)
{
	Context *conptr = (Context *)context;
	CacheData *cachedata = conptr->cachemanager->cachedata;
	
	cachedata[0].Index = conptr->l1info->page;
	cachedata[0].IndexID = 0;
	cachedata[0].IndexCount = conptr->cachemanager->L1InfoLen / UNIT_SIZE;
	cachedata[0].unitLen = conptr->cachemanager->L1UnitLen;
	cachedata[0].head.next = NULL;
	conptr->cachemanager->L1Info = &cachedata[0];
}

/**
 *	init_L2cache - initialize L2 cache
 *
 *	@context: global variable
 */
static int init_L2cache(int context)
{
	int ret;
	Context *conptr = (Context *)context;
	CacheData *cachedata = conptr->cachemanager->cachedata;
	
	ret = CacheData_Init(&cachedata[1], conptr->cachemanager->L2InfoLen / UNIT_SIZE * 
	conptr->cachemanager->L1InfoLen / UNIT_SIZE, conptr->cachemanager->L2UnitLen);
	if (ret == -1) {
		ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		return -1;
	}

	cachedata[1].IndexID = 0;
	conptr->cachemanager->L2Info = &cachedata[1];

	return 0;	
}

/**
 *	init_L3cache - initialize L3 cache
 *
 *	@context: global variable
 */
static int init_L3cache(int context)
{
	int i, ret;
	Context *conptr = (Context *)context;
	CacheData *cachedata = conptr->cachemanager->cachedata;
	
	for (i = L1_CACHEDATA_COUNT + L2_CACHEDATA_COUNT; 
	i < L1_CACHEDATA_COUNT + L2_CACHEDATA_COUNT + L3_CACHEDATA_COUNT; i++) {
		ret = CacheData_Init(&cachedata[i], conptr->cachemanager->L3InfoLen / UNIT_SIZE, 
			conptr->cachemanager->L3UnitLen);
		if (ret == -1) {
			ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
			return -1;
		}
		
		CacheList_Insert(conptr->cachemanager->L3Info,&cachedata[i]);
	}

	return 0;	
}

/**
 *	init_L4cache - initialize L4 cache
 *
 *	@context: global variable
 */
static int init_L4cache(int context)
{
	int i, ret;
	Context *conptr = (Context *)context;
	CacheData *cachedata = conptr->cachemanager->cachedata;
	
	for (i = L1_CACHEDATA_COUNT + L2_CACHEDATA_COUNT + L3_CACHEDATA_COUNT; 
	i < L1_CACHEDATA_COUNT + L2_CACHEDATA_COUNT + 
		L3_CACHEDATA_COUNT + L4_CACHEDATA_COUNT; i++) {
		ret = CacheData_Init(&cachedata[i], conptr->cachemanager->L4InfoLen / UNIT_SIZE, 
			conptr->cachemanager->L4UnitLen);
		if (ret == -1) {
			ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
			return -1;
		}
		CacheList_Insert(conptr->cachemanager->L4Info,&cachedata[i]);
	}

	return 0;	
}

/**
 *	init_cache - initialize L1, L2 and L4 cache
 *
 *	@context: global variable
 */
static int init_cache(int context)
{
	int ret = 0;
	
	init_L1cache(context);

	ret = init_L2cache(context);
	if (ret == -1)
		goto ERROR;
	
	ret = init_L3cache(context);
	if (ret == -1)
		goto ERROR;
	
	ret = init_L4cache(context);
	if (ret == -1)
		goto ERROR;

ERROR:
	return ret;	
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
	CacheData *cachedata;
	Context *conptr = (Context *)context;

	conptr->cachemanager = (CacheManager *)Nand_VirtualAlloc(sizeof(CacheManager));
	if (!(conptr->cachemanager )) {
		ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		return -1;
	}

	conptr->cachemanager->cachedata = (CacheData *)Nand_VirtualAlloc(sizeof(CacheData) * 
		(L1_CACHEDATA_COUNT + L2_CACHEDATA_COUNT + 
		L3_CACHEDATA_COUNT + L4_CACHEDATA_COUNT));
	cachedata = conptr->cachemanager->cachedata;
	if (!cachedata) {
		ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		Nand_VirtualFree(conptr->cachemanager);
		return -1;
	}

	fill_infolen_unitlen(context);

	if (CacheList_Init(&(conptr->cachemanager->L3Info)) == -1) {
		ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		goto ERROR;
	}

	if (CacheList_Init(&(conptr->cachemanager->L4Info)) == -1) {
		ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		goto ERROR;
	}

	/* init cache */
	ret = init_cache(context);
	if (ret == -1)
		goto ERROR;

	/* init mutex */
	InitNandMutex(&(conptr->cachemanager->mutex));

	conptr->cachemanager->locked_data = NULL;
	
	return 0;
ERROR:
	Nand_VirtualFree(cachedata);
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
	int i;
	Context *conptr = (Context *)context;

	for (i = 0; i < L1_CACHEDATA_COUNT + L2_CACHEDATA_COUNT + 
		L3_CACHEDATA_COUNT + L4_CACHEDATA_COUNT; i++)
		CacheData_DeInit(&(conptr->cachemanager->cachedata)[i]);

	CacheList_DeInit(&conptr->cachemanager->L3Info);
	CacheList_DeInit(&conptr->cachemanager->L4Info);
	
	Nand_VirtualFree(conptr->cachemanager->cachedata);
	Nand_VirtualFree(conptr->cachemanager);
}

/**
 *	CacheManager_updateL3Cache  -  Update L3 cache when given sectorid and pi
 *
 *	@cachemanager: operate object
 *	@sectorid: number of sector
 *	@pi: pageinfo
 */
static void CacheManager_updateL3Cache ( CacheManager *cachemanager, unsigned int sectorid, PageInfo *pi )
{
	CacheData *cachedata;

	cachedata = CacheList_get(cachemanager->L3Info, sectorid);
	if (!cachedata) {
		cachedata = CacheList_getTail(cachemanager->L3Info);

		if (cachemanager->L2InfoLen)
			cachedata->IndexID = sectorid / cachemanager->L2UnitLen * cachemanager->L2UnitLen;
		else
			cachedata->IndexID = sectorid / cachemanager->L1UnitLen * cachemanager->L1UnitLen;
	}

	memcpy(cachedata->Index, pi->L3Info, cachemanager->L3InfoLen);
			
	CacheList_Insert(cachemanager->L3Info, cachedata);
}

/**
 *	CacheManager_updateCache  -  Update L4 cache when given sectorid and pageid
 *
 *	@context: global variable
 *	@sectorid: number of sector
 *	@pageid: number of page
 */
void CacheManager_updateCache (int context, unsigned int sectorid, unsigned int pageid )
{
	unsigned int l4indexid;
	CacheData *cachedata;
	Context *conptr = (Context *)context;
	CacheManager *cachemanager = conptr->cachemanager;

	cachedata = CacheList_get(cachemanager->L4Info, sectorid);
	if (!cachedata) {
		cachedata = CacheList_getTail(cachemanager->L4Info);
		memset(cachedata->Index, 0xff, cachemanager->L4InfoLen);
	}

	if (cachemanager->L3InfoLen) {
		cachedata->IndexID = sectorid / cachemanager->L3UnitLen * cachemanager->L3UnitLen;
		l4indexid = sectorid % cachemanager->L3UnitLen;
	}
	else if (cachemanager->L2InfoLen) {
		cachedata->IndexID = sectorid / cachemanager->L2UnitLen * cachemanager->L2UnitLen;
		l4indexid = sectorid % cachemanager->L2UnitLen;
	}
	else {
		cachedata->IndexID = sectorid / cachemanager->L1UnitLen * cachemanager->L1UnitLen;
		l4indexid = sectorid % cachemanager->L1UnitLen;
	}

	CacheData_set(cachedata, l4indexid, pageid);

	CacheList_Insert(cachemanager->L4Info, cachedata);
}

/**
 *	CacheManager_updateL4Cache  -  Update L4 cache when given sectorid and pi
 *
 *	@cachemanager: operate object
 *	@sectorid: number of sector
 *	@pi: pageinfo
 */
static void CacheManager_updateL4Cache ( CacheManager *cachemanager, unsigned int sectorid, PageInfo *pi )
{
	CacheData *cachedata;

	cachedata = CacheList_get(cachemanager->L4Info, sectorid);
	if (!cachedata) {
		cachedata = CacheList_getTail(cachemanager->L4Info);

		if (cachemanager->L3InfoLen)
			cachedata->IndexID = sectorid / cachemanager->L3UnitLen * cachemanager->L3UnitLen;
		else if (cachemanager->L2InfoLen)
			cachedata->IndexID = sectorid / cachemanager->L2UnitLen * cachemanager->L2UnitLen;
		else
			cachedata->IndexID = sectorid / cachemanager->L1UnitLen * cachemanager->L1UnitLen;
	}

	memcpy(cachedata->Index, pi->L4Info, cachemanager->L4InfoLen);

	CacheList_Insert(cachemanager->L4Info, cachedata);
}

/**
 *	read_infopage  -  Read infopage into pi at pageid
 *
 *	@context: global variable
 *	@pageid: physical address
 *	@pi: fill some information when read operation finish
 */
static int read_infopage( int context, unsigned int pageid , PageInfo *pi )
{
	int ret = -1;
	unsigned char *buf = NULL;
	NandPageInfo *nandpageinfo;
	PageList *pagelist = NULL;
	Context *conptr = (Context *)context;
	CacheManager *cachemanager = conptr->cachemanager;
	VNandInfo *vnand = &conptr->vnand;

	buf = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * vnand->BytePerPage);
	if (!buf) {
		ndprint(1,"error func %s line %d \n", __FUNCTION__,__LINE__);
		return -1;
	}
	nandpageinfo = (NandPageInfo *)buf;
	memset(buf,0xff,vnand->BytePerPage);

	pagelist = (PageList *)BuffListManager_getTopNode((int)(conptr->blm), sizeof(PageList));
	pagelist->startPageID = pageid;
	pagelist->OffsetBytes = 0;
	pagelist->Bytes = vnand->BytePerPage;
	pagelist->pData = (void*)buf;
	pagelist->retVal = 0;
	(pagelist->head).next = NULL;

	pi->L1InfoLen = cachemanager->L1InfoLen;
	pi->L2InfoLen = cachemanager->L2InfoLen;
	pi->L3InfoLen = cachemanager->L3InfoLen;
	pi->L4InfoLen = cachemanager->L4InfoLen;
	pi->PageID = pageid;
	
	ret = vNand_MultiPageRead(vnand, pagelist);

	if(ret != 0) {
		ndprint(1,"vNand read pageinfo error func %s line %d \n",
					__FUNCTION__,__LINE__);
		ret = -1;
		goto err;
	}	

	/* analyse pi */
	pi->L1Index = nandpageinfo->L1Index;
	nandpageinfo->L4Info = buf + sizeof(NandPageInfo);
	memcpy(pi->L4Info, nandpageinfo->L4Info, pi->L4InfoLen);
	
	if(pi->L3InfoLen != 0) {
		if(pi->L2InfoLen == 0) {
			pi->L3Index = nandpageinfo->L3Index;
			nandpageinfo->L3Info = buf + sizeof(NandPageInfo) + pi->L4InfoLen; 
			memcpy(pi->L3Info,nandpageinfo->L3Info,pi->L3InfoLen);
		}
		else {
			pi->L2Index = nandpageinfo->L2Index;
			nandpageinfo->L2Info = buf + sizeof(NandPageInfo) + pi->L4InfoLen;
			memcpy(pi->L2Info,nandpageinfo->L2Info,pi->L2InfoLen);
			
			pi->L3Index = nandpageinfo->L3Index;
			nandpageinfo->L3Info = buf + sizeof(NandPageInfo) + pi->L2InfoLen + pi->L4InfoLen; 
			memcpy(pi->L3Info,nandpageinfo->L3Info,pi->L3InfoLen);
		}	
	}
	
err:	
	BuffListManager_freeList((int)(conptr->blm), (void **)&pagelist,(void *)pagelist, sizeof(PageList));
	return ret;
}

/**
 *	alloc_pageinfo  -  alloc L1Info, L2Info, L3Info and L4Info of pageinfo
 *
 *	@pageinfo: which need to alloc
 *	@cachemanager: to konw whether L2InfoLen and L3InfoLen are 0 or not
 */
static int alloc_pageinfo(PageInfo *pageinfo, CacheManager *cachemanager)
{
	pageinfo->L1Info = (unsigned char *)(cachemanager->L1Info->Index);
	
	if (cachemanager->L2InfoLen) {
		pageinfo->L2Info = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * cachemanager->L2InfoLen);
		if (!(pageinfo->L2Info)) {
			ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
			goto ERROR1;
		}
	}
	
	if (cachemanager->L3InfoLen) {
		pageinfo->L3Info = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * cachemanager->L3InfoLen);
		if (!(pageinfo->L3Info)) {
			ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
			goto ERROR2;
		}
	}
	
	pageinfo->L4Info = (unsigned char *)Nand_VirtualAlloc(sizeof(unsigned char) * cachemanager->L4InfoLen);
	if (!(pageinfo->L4Info)) {
		ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		goto ERROR3;
	}

	return 0;

ERROR3:
	if (cachemanager->L3InfoLen)
		Nand_VirtualFree(pageinfo->L3Info);
ERROR2:
	if (cachemanager->L2InfoLen)
		Nand_VirtualFree(pageinfo->L2Info);
ERROR1:
	return -1;
}

/**
 *	free_pageinfo  -  free L1Info, L2Info, L3Info and L4Info of pageinfo
 *
 *	@pageinfo: which need to free
 *	@cachemanager: to konw whether L2InfoLen and L3InfoLen are 0 or not
 */
static void free_pageinfo(PageInfo *pageinfo, CacheManager *cachemanager)
{
	if (cachemanager->L2InfoLen)
		Nand_VirtualFree(pageinfo->L2Info);
	
	if (cachemanager->L3InfoLen)
		Nand_VirtualFree(pageinfo->L3Info);

	Nand_VirtualFree(pageinfo->L4Info);
}

/**
 *	is_L3L4samepage - whether L3 and L4 in the same page
 *
 *	@pi: which has L1, L2, L3 and L4 some information
 *	@pageid: number of page
 */
static int is_L3L4samepage(PageInfo *pi, unsigned int pageid)
{
	if (pageid == pi->PageID)
		return 1;
	else
		return 0;
}

/**
 *	access_l4cache  -  access l4cache to find pageid
 *
 *	@context: global variable
 *	@sectorid: number of sector given by caller
 *	@pageid: if cache hit, then save it in pageid to caller
 */
static void access_l4cache(int context, unsigned int sectorid, unsigned int *pageid)
{
	unsigned int page_id = -1;
	unsigned int l4indexid;
	CacheData *cachedata;
	Context *conptr = (Context *)context;
	CacheManager *cachemanager = conptr->cachemanager;

	if (cachemanager->L3InfoLen)
		l4indexid = sectorid % cachemanager->L3UnitLen;
	else if (cachemanager->L2InfoLen)
		l4indexid = sectorid % cachemanager->L2UnitLen;
	else
		l4indexid = sectorid % cachemanager->L1UnitLen;

	cachedata = CacheList_get(cachemanager->L4Info, sectorid);
	if (cachedata) {
		CacheList_Insert(cachemanager->L4Info, cachedata);

		page_id  = CacheData_get(cachedata, l4indexid);
	}

	*pageid = page_id;
}

/**
 *	access_l3cache  -  access l3cache to find pageid
 *
 *	@context: global variable
 *	@sectorid: number of sector given by caller
 *	@pageid: if cache hit, then save it in pageid to caller
 */
static int access_l3cache(int context, unsigned int sectorid, unsigned int *pageid)
{
	int ret = 0;
	unsigned int page_id = -1;
	unsigned int l3indexid;
	CacheData *cachedata;
	PageInfo pi;
	Context *conptr = (Context *)context;
	CacheManager *cachemanager = conptr->cachemanager;

	if (cachemanager->L2InfoLen)
		l3indexid = sectorid % cachemanager->L2UnitLen;
	else
		l3indexid = sectorid % cachemanager->L1UnitLen;
	
	cachedata = CacheList_get(cachemanager->L3Info, sectorid);
	if (cachedata) {
		CacheList_Insert(cachemanager->L3Info, cachedata);

		page_id  = CacheData_get(cachedata, l3indexid);

		if (page_id  != -1) { // cache hit
			ret = alloc_pageinfo(&pi, cachemanager);
			if (ret == -1) {
				ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
				return -1;
			}

			read_infopage(context, page_id , &pi);

			page_id  = ((unsigned int *)(pi.L4Info))[sectorid % cachemanager->L3UnitLen 
			/ cachemanager->L4UnitLen];

			if ((int)page_id  < 0) {
				ndprint(1,"WARNING: cache miss, sectorid = %d, func %s line %d\n", 
					sectorid, __FUNCTION__, __LINE__);
				free_pageinfo(&pi, cachemanager);
				return -1;
			}
			
			CacheManager_updateL4Cache(cachemanager, sectorid, &pi);
			
			free_pageinfo(&pi, cachemanager);
		}
	}

	*pageid = page_id;

	return ret;
}

/**
 *	access_l2l1cache  -  access l2cache or l1cache to find pageid
 *
 *	@context: global variable
 *	@sectorid: number of sector given by caller
 *	@pageid: if cache hit, then save it in pageid to caller
 */
static int access_l2l1cache(int context, unsigned int sectorid, unsigned int *pageid)
{
	int ret = 0;
	unsigned page_id = -1;
	PageInfo pi;
	Context *conptr = (Context *)context;
	CacheManager *cachemanager = conptr->cachemanager;

	if (cachemanager->L2InfoLen)
		page_id = (cachemanager->L2Info->Index)[sectorid % cachemanager->L1UnitLen 
		/ cachemanager->L2UnitLen + sectorid / cachemanager->L1UnitLen * (cachemanager->L2InfoLen / 4)];
	else
		page_id = CacheData_get(cachemanager->L1Info, sectorid);

	ret = alloc_pageinfo(&pi, cachemanager);
	if (ret == -1) {
		ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		return -1;
	}
			
	read_infopage(context, page_id, &pi);

	/* update L3 cache */
	if (cachemanager->L3InfoLen) {
		if (cachemanager->L2InfoLen)
			page_id = ((unsigned int *)(pi.L3Info))[sectorid % cachemanager->L2UnitLen 
			/ cachemanager->L3UnitLen];
		else
			page_id = ((unsigned int *)(pi.L3Info))[sectorid % cachemanager->L1UnitLen 
			/ cachemanager->L3UnitLen];
		
		if ((int)page_id < 0) {
			ndprint(1,"WARNING: cache miss, sectorid = %d, func %s line %d\n", 
				sectorid, __FUNCTION__, __LINE__);
			free_pageinfo(&pi, cachemanager);
			return -1;
		}
		
		CacheManager_updateL3Cache(cachemanager, sectorid, &pi);

		/* whether L3 and L4 in the same page, if not, then read again */
		if (!is_L3L4samepage(&pi, page_id))
			read_infopage(context, page_id, &pi);

	}

	/* update L4 cache */
	if (cachemanager->L3InfoLen)
		page_id = ((unsigned int *)(pi.L4Info))[sectorid % cachemanager->L3UnitLen / cachemanager->L4UnitLen];
	else if (cachemanager->L2InfoLen)
		page_id = ((unsigned int *)(pi.L4Info))[sectorid % cachemanager->L2UnitLen / cachemanager->L4UnitLen];
	else
		page_id = ((unsigned int *)(pi.L4Info))[sectorid % cachemanager->L1UnitLen / cachemanager->L4UnitLen];

	if ((int)page_id < 0) {
		ndprint(1,"WARNING: cache miss func %s line %d\n", __FUNCTION__, __LINE__);
		free_pageinfo(&pi, cachemanager);
		return -1;
	}
	
	CacheManager_updateL4Cache(cachemanager, sectorid, &pi);

	free_pageinfo(&pi, cachemanager);

	*pageid = page_id;

	return ret;
}

/**
 *	CacheManager_getPageID  -  Get a pageid when given a sectorid
 *
 *	@context: global variable
 *	@sectorid: number of sector given by caller
 */
unsigned int CacheManager_getPageID ( int context, unsigned int sectorid )
{	
	int ret;
	unsigned int pageid;
	Context *conptr = (Context *)context;
	CacheManager *cachemanager = conptr->cachemanager;

	if (sectorid < 0 || sectorid > cachemanager->L1UnitLen * cachemanager->L1InfoLen / 4) {
		ndprint(1,"ERROR: sectorid = %d func %s line %d\n", sectorid, __FUNCTION__, __LINE__);
		return -1;
	}

	access_l4cache(context, sectorid, &pageid);
	if (pageid != -1) // cache hit
		return pageid;

	/* l4cache miss, if l3cache exist, then access l3cache, else access l2cache */
	if (cachemanager->L3InfoLen) {
		ret = access_l3cache(context, sectorid, &pageid);
		if(ret == -1)
			return -1;
		if (pageid != -1) // cache hit
			return pageid;
	}

	/* l3cache miss, if l2cache exist, then access l2cache, else access l1cache */
	ret = access_l2l1cache(context, sectorid, &pageid);
	if(ret == -1)
		return -1;

	return pageid;
}

/**
 *	get_pageinfo  -  get pageinfo
 *
 *	@pi: which need to alloc
 *	@cachemanager: apply infolen
 */
static int get_pageinfo(PageInfo **pi, CacheManager *cachemanager)
{
	int ret = 0;
	
	ret = alloc_pageinfo(*pi, cachemanager);
	if (ret == -1) {
		ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		return -1;
	}

	(*pi)->L1InfoLen = cachemanager->L1InfoLen;
	(*pi)->L2InfoLen = cachemanager->L2InfoLen;
	(*pi)->L3InfoLen = cachemanager->L3InfoLen;
	(*pi)->L4InfoLen = cachemanager->L4InfoLen;

	return ret;
}

/**
 *	get_L3cachedata  -  get L3cachedata
 *
 *	@context: global variable
 *	@sectorid: number of sector
 *	@cachedata: if get L3cachedata success, then save it in cachedata to caller
 */
static int get_L3cachedata(int context, unsigned int sectorid, CacheData **cachedata)
{
	int ret = 0;
	unsigned int pageid;
	PageInfo pageinfo;
	Context *conptr = (Context *)context;
	CacheManager *cachemanager = conptr->cachemanager;
	
	*cachedata = CacheList_get(cachemanager->L3Info, sectorid);
	if (!(*cachedata)) {
		*cachedata = CacheList_getTail(cachemanager->L3Info);

		ret = alloc_pageinfo(&pageinfo, cachemanager);
		if (ret == -1) {
			ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
			return -1;
		}
		
		if (cachemanager->L2InfoLen)
			pageid = CacheData_get(cachemanager->L2Info, sectorid % cachemanager->L1UnitLen);
		else
			pageid = CacheData_get(cachemanager->L1Info, sectorid);

		if ((int)pageid < 0)
			memset(pageinfo.L3Info, 0xff, cachemanager->L3InfoLen);
		else {
			ret = read_infopage(context, pageid, &pageinfo);
			if (ret == -1) {
				ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
				return -1;
			}
		}	

		memcpy((*cachedata)->Index, pageinfo.L3Info, cachemanager->L3InfoLen);

		/* fill IndexID */
		if (cachemanager->L2InfoLen)
			(*cachedata)->IndexID = sectorid / cachemanager->L2UnitLen * cachemanager->L2UnitLen;
		else
			(*cachedata)->IndexID = sectorid / cachemanager->L1UnitLen * cachemanager->L1UnitLen;

		free_pageinfo(&pageinfo, cachemanager);
	}

	return ret;
}

/**
 *	get_L4cachedata  -  get L4cachedata
 *
 *	@context: global variable
 *	@sectorid: number of sector
 *	@L3_cachedata: which get in func get_L4cachedata
 *	@cachedata: if get L4cachedata success, then save it in cachedata to caller	
 */
static int get_L4cachedata(int context, unsigned int sectorid, CacheData *L3_cachedata, CacheData **cachedata)
{
	int ret = 0;
	unsigned int pageid;
	unsigned int l4indexid;
	PageInfo pageinfo;
	Context *conptr = (Context *)context;
	CacheManager *cachemanager = conptr->cachemanager;

	*cachedata = CacheList_get(cachemanager->L4Info,sectorid);
	if (!(*cachedata)) {
		*cachedata = CacheList_getTail(cachemanager->L4Info);

		ret = alloc_pageinfo(&pageinfo, cachemanager);
		if (ret == -1) {
			ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
			return -1;
		}

		if (cachemanager->L3InfoLen) {
			if (cachemanager->L2InfoLen)
				l4indexid = sectorid % cachemanager->L2UnitLen;
			else
				l4indexid = sectorid % cachemanager->L1UnitLen;

			pageid = CacheData_get(L3_cachedata, l4indexid);
		}
		else if (cachemanager->L2InfoLen) {
			l4indexid = sectorid % cachemanager->L1UnitLen;
			pageid = CacheData_get(cachemanager->L2Info, l4indexid);
		}
		else {
			l4indexid = sectorid;
			pageid = CacheData_get(cachemanager->L1Info, l4indexid);
		}
		
		if ((int)pageid < 0)
			memset(pageinfo.L4Info, 0xff, cachemanager->L4InfoLen);
		else {
			ret = read_infopage(context, pageid, &pageinfo);
			if (ret == -1) {
				ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
				return -1;
			}
		}

		memcpy((*cachedata)->Index, pageinfo.L4Info, cachemanager->L4InfoLen);

		/* fill IndexID */
		if (cachemanager->L3InfoLen)
			(*cachedata)->IndexID = sectorid / cachemanager->L3UnitLen * cachemanager->L3UnitLen;
		else if (cachemanager->L2InfoLen)
			(*cachedata)->IndexID = sectorid / cachemanager->L2UnitLen * cachemanager->L2UnitLen;
		else
			(*cachedata)->IndexID = sectorid / cachemanager->L1UnitLen * cachemanager->L1UnitLen;

		free_pageinfo(&pageinfo, cachemanager);
	}

	return ret;
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
void CacheManager_lockCache ( int context, unsigned int sectorid, PageInfo **pi )
{
	int ret = -1;
	CacheData *L1_cachedata = NULL;
	CacheData *L2_cachedata = NULL;
	CacheData *L3_cachedata = NULL;
	CacheData *L4_cachedata = NULL;
	Context *conptr = (Context *)context;
	CacheManager *cachemanager = conptr->cachemanager;

	NandMutex_Lock(&(cachemanager->mutex));

	*pi = (PageInfo *)Nand_VirtualAlloc(sizeof(PageInfo));
	if (!(*pi)) {
		ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		goto ERROR;
	}

	/* when sectorid is -1, apply a pi to caller directly */
	if (sectorid == -1){
		ret = get_pageinfo(pi, cachemanager);
		if (ret == -1) {
			ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
			goto ERROR;
		}

		NandMutex_Unlock(&(cachemanager->mutex));
		return;
	}
	else if (sectorid < 0 || sectorid > cachemanager->L1UnitLen * cachemanager->L1InfoLen / 4) {
		ndprint(1,"ERROR: sectorid = %d func %s line %d\n", sectorid, __FUNCTION__, __LINE__);
		goto ERROR;
	}	

	/* get L1, L2, L3 and L4 cachedata */
	L1_cachedata = cachemanager->L1Info;
	L2_cachedata = cachemanager->L2Info;

	if (cachemanager->L3InfoLen) {
		ret = get_L3cachedata(context, sectorid, &L3_cachedata);
		if (ret == -1) {
			ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
			goto ERROR;
		}
	}

	ret = get_L4cachedata(context, sectorid, L3_cachedata, &L4_cachedata);
	if (ret == -1) {
		ndprint(1,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		goto ERROR;
	}

	/* fill pi */
	(*pi)->L1Info = (unsigned char *)(L1_cachedata->Index);
	(*pi)->L1InfoLen = cachemanager->L1InfoLen;

	if (cachemanager->L2InfoLen)
		(*pi)->L2Info = (unsigned char *)(L2_cachedata->Index 
		+ sectorid / cachemanager->L1UnitLen * (cachemanager->L2InfoLen / 4));
	(*pi)->L2InfoLen = cachemanager->L2InfoLen;

	if (cachemanager->L3InfoLen)
		(*pi)->L3Info = (unsigned char *)(L3_cachedata->Index);
	(*pi)->L3InfoLen = cachemanager->L3InfoLen;
	
	(*pi)->L4Info = (unsigned char *)(L4_cachedata->Index);
	(*pi)->L4InfoLen = cachemanager->L4InfoLen;

	/* lock cachedata */
	if (cachemanager->L3InfoLen) {
		L3_cachedata->head.next = &L4_cachedata->head;
		L4_cachedata->head.next = NULL;
		cachemanager->locked_data = L3_cachedata;
	}
	else
		cachemanager->locked_data = L4_cachedata;

ERROR:
	NandMutex_Unlock(&(cachemanager->mutex));
	return;
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
	CacheData *L3_cachedata = NULL;
	CacheData *L4_cachedata = NULL;
	struct singlelist *pos;
	Context *conptr = (Context *)context;
	CacheManager *cachemanager = conptr->cachemanager;
	
	NandMutex_Lock(&(cachemanager->mutex));

	if (cachemanager->locked_data == NULL)
		goto FREE;

	pos = &(cachemanager->locked_data->head);	
	if (cachemanager->L3InfoLen) {
		L3_cachedata = singlelist_entry(pos, CacheData, head);
		pos = pos->next;
	}

	L4_cachedata = singlelist_entry(pos, CacheData, head);

	/* Insert lockcache back to its cachelist */
	CacheList_Insert(cachemanager->L4Info, L4_cachedata);
	if (cachemanager->L3InfoLen) {
		cachemanager->locked_data->head.next = NULL;
		CacheList_Insert(cachemanager->L3Info, L3_cachedata);
	}
	cachemanager->locked_data = NULL;

FREE:
	Nand_VirtualFree(pi);
	NandMutex_Unlock(&(cachemanager->mutex));
}

