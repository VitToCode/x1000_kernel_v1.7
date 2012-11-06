#include "cachelist.h"
#include "NandAlloc.h"
#include "nanddebug.h"

#define CACHELIST_DEBUG_DATA 1

/**
 *	CacheList_Init  -  Initialize operation
 *
 *	@cachelist: operate object
 */
CacheList* CacheList_Init (void) {
	CacheList *cachelist;
	cachelist = (CacheList *)Nand_VirtualAlloc(sizeof(CacheList));
	if (!cachelist) {
		ndprint(CACHELIST_ERROR,"ERROR: fun %s line %d\n", __FUNCTION__, __LINE__);
		return 0;
	}

	INIT_LIST_HEAD(&cachelist->top);
	cachelist->listCount = 0;

	return cachelist;
}

/**
 *	CacheList_DeInit  -  Deinit operation
 *
 *	@cachelist: operate object
 */
void CacheList_DeInit ( CacheList *cachelist ) {
	Nand_VirtualFree(cachelist);
}
/** 
 *	CacheList_getTail  -  Delete the tail node of list, and return this cachedata
 *
 *	@cachelist: operate object
 */
CacheData *CacheList_getTail ( CacheList *cachelist ){
	struct list_head *tail;
	
	if(list_empty(&cachelist->top))
		return NULL;
	tail = cachelist->top.prev;
	list_del(tail);
	cachelist->listCount--;
	
	return list_entry(tail,CacheData,head);
}

/** 
 *	CacheList_get  -  Delete the node of list when indexid is matched, and return this cachedata
 *
 *	@cachelist: operate object
 *	@indexid: sectorid
 */
CacheData *CacheList_get ( CacheList *cachelist, unsigned int indexid ) {
	CacheData *cd,*fcd = NULL;
	struct list_head *pos;
	list_for_each(pos,&cachelist->top) {
		cd = list_entry(pos,CacheData,head);
		if(CacheData_ismatch(cd,indexid)){
			fcd = cd;
			list_del(&fcd->head);
			cachelist->listCount--;
			break;
		}
	}
	return fcd;
}

/**
 *	CacheList_Insert  -  Insert cachedata into cachelist before top node
 *
 *	@cachelist: operate object
 *	@data: the cachedata wanted to insert
 */
#ifdef CACHELIST_DEBUG_DATA
static CacheData * checklist(CacheList *cachelist, CacheData *data ) {
	struct list_head *pos;
	CacheData *cd;	
	list_for_each(pos,&cachelist->top) {
		cd = list_entry(pos,CacheData,head);
		if(cd->IndexID == data->IndexID && data->IndexID != -1 && cd->IndexID != -1) {
			ndprint(CACHELIST_ERROR,"cacheid %d is dup\n",data->IndexID);
			CacheData_Dump(cd);
			CacheData_Dump(data);
			return cd;
		}
	}
	return 0;
}
#endif
void CacheList_Insert ( CacheList *cachelist, CacheData *data )
{
#ifdef CACHELIST_DEBUG_DATA
	CacheData *cd = checklist(cachelist,data);
	if(cd) {
		list_del(&cd->head);
	}
#endif

	list_add(&data->head,&cachelist->top);
	cachelist->listCount++;
}

CacheData * CacheList_getTop ( CacheList *cachelist)
{
	struct list_head *top;
	if(list_empty(&cachelist->top)) 
		return NULL;
	top = cachelist->top.next;
	list_del(top);
	cachelist->listCount--;
	return list_entry(top,CacheData,head);
}
void CacheList_Dump(CacheList *cachelist) {
	struct list_head *pos;
	CacheData *cd;
	ndprint(CACHELIST_INFO,"======== cachelistp[%p] count = %d ========\n",cachelist,cachelist->listCount);
	list_for_each(pos,&cachelist->top) {
		cd = list_entry(pos,CacheData,head);
		CacheData_Dump(cd);
	}
}
