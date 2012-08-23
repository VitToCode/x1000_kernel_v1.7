#include "cachelist.h"
#include "NandAlloc.h"
#include "nanddebug.h"

/**
 *	CacheList_Init  -  Initialize operation
 *
 *	@cachelist: operate object
 */
CacheList* CacheList_Init (void){
	CacheList *cachelist;
	cachelist = (CacheList *)Nand_VirtualAlloc(sizeof(CacheList));
	if (!cachelist) {
		ndprint(CACHELIST_ERROR,"ERROR: fun %s line %d\n", __FUNCTION__, __LINE__);
		return 0;
	}
	
	cachelist->top = NULL;
	cachelist->tail = NULL;
	cachelist->listCount = 0;

	return cachelist;
}

/**
 *	CacheList_DeInit  -  Deinit operation
 *
 *	@cachelist: operate object
 */
void CacheList_DeInit ( CacheList *cachelist )
{
	Nand_VirtualFree(cachelist);
}

/** 
 *	is_indexid_match  -  Whether indexid in the cachedata or not
 *	
 *	@cachedata: operate object
 *	@indexid: sectorid
 */
static int is_indexid_match(CacheData *cachedata, unsigned int indexid)
{
	if (indexid >= cachedata->IndexID && 
		indexid < cachedata->IndexID + cachedata->unitLen * cachedata->IndexCount)
		return 1;
	else 
		return 0;
}

/** 
 *	CacheList_getTail  -  Delete the tail node of list, and return this cachedata
 *
 *	@cachelist: operate object
 */
CacheData *CacheList_getTail ( CacheList *cachelist )
{
	struct singlelist *pos, *q;
	CacheData *cachedata;

	if (cachelist->listCount == 0)
		return NULL;
	else if (cachelist->listCount == 1) {
		cachedata = cachelist->tail;
		cachelist->top = NULL;
		cachelist->tail = NULL;
		cachelist->listCount--;
		
		return cachedata;
	}

	for (pos = &(cachelist->top->head); pos->next->next != NULL; pos = pos->next);
	q = pos->next;

	/* the tail node of list must be changed */
	cachelist->tail = singlelist_entry(pos,CacheData,head);
	cachedata = singlelist_entry(q,CacheData,head);
	pos->next = NULL;
	
	cachelist->listCount--;
	
	return cachedata;
}

/** 
 *	CacheList_get  -  Delete the node of list when indexid is matched, and return this cachedata
 *
 *	@cachelist: operate object
 *	@indexid: sectorid
 */
CacheData *CacheList_get ( CacheList *cachelist, unsigned int indexid )
{
	int flag = 0;
	struct singlelist *pos, *q;
	CacheData *prev_data = NULL;
	CacheData *next_data = NULL;

	if (!cachelist) {
		ndprint(CACHELIST_ERROR,"ERROR: CacheList was null fun %s line %d\n", 
			__FUNCTION__, __LINE__);
		return NULL;
	}

	if (cachelist->listCount == 0) {
		ndprint(CACHELIST_INFO,"Warning: CacheList has no content fun %s line %d\n", 
			__FUNCTION__, __LINE__);
		return NULL;
	}

	if (is_indexid_match(cachelist->top,indexid)) {
		prev_data = cachelist->top;
		if (cachelist->top->head.next == NULL)
			cachelist->top = NULL;
		else
			cachelist->top = singlelist_entry(cachelist->top->head.next, CacheData, head);

		cachelist->listCount--;
		return prev_data;
	}

	/* first find the corresponding cachedata */
	singlelist_for_each(pos,&(cachelist->top->head)) {
		q = pos->next;
		if (q) {
			prev_data = singlelist_entry(pos,CacheData,head);
			next_data = singlelist_entry(q,CacheData,head);
		}
		else
			break;

		if (next_data && is_indexid_match(next_data,indexid)) {
			flag = 1;
			break;
		}
	}

	/*  if found corresponding cachedata, then delete it out of cachelist */
	if (flag) {
		pos->next = q->next;

		if (cachelist->tail == next_data)
			cachelist->tail = prev_data;

		cachelist->listCount--;
		flag = 0;
		
		return next_data;
	}
	else
		return NULL;
}

/**
 *	CacheList_find  -  Get a sectorid when given a pageid
 *
 *	@cachelist: operate object
 *	@data: pageid
 */
unsigned int CacheList_find ( CacheList *cachelist, unsigned int data )
{
	unsigned int ret = -1;
	struct singlelist *pos;
	CacheData *cachedata;

	singlelist_for_each(pos,&(cachelist->top->head)) {
		cachedata = singlelist_entry(pos,CacheData,head);
		ret = CacheData_find(cachedata,data);
		if (ret != -1)
			break;
	}

	return ret;
}

/**
 *	CacheList_Insert  -  Insert cachedata into cachelist before top node
 *
 *	@cachelist: operate object
 *	@data: the cachedata wanted to insert
 */
void CacheList_Insert ( CacheList *cachelist, CacheData *data )
{
	if (cachelist->top == NULL) {
		data->head.next = NULL;
		cachelist->top = data;
		cachelist->tail = data;
	}
	else {
		data->head.next = &(cachelist->top->head);
		cachelist->top = data;
	}

	cachelist->listCount++;
}

CacheData * CacheList_getTop ( CacheList *cachelist, unsigned int indexid )
{
	CacheData *prev_data = NULL;
	if (is_indexid_match(cachelist->top,indexid)) {
		prev_data = cachelist->top;
		if (cachelist->top->head.next == NULL)
			cachelist->top = NULL;
		else
			cachelist->top = singlelist_entry(cachelist->top->head.next, CacheData, head);
		
		cachelist->listCount--;
	}
	return prev_data;
}
