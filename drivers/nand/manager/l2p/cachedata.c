#include "clib.h"
#include "cachedata.h"
#include "NandAlloc.h"
#include "nanddebug.h"

/**
 *	CacheData_Init  -  Initialize operation
 *
 *	@cachedata: operate object
 *	@indexcount: count of pageid in index
 *	@unitlen: how mang sectors one pageid indicate
 */
CacheData * CacheData_Init (unsigned short indexcount, unsigned int unitlen )
{
	int i;
	CacheData *cachedata;	
	cachedata = Nand_VirtualAlloc(sizeof(CacheData));
	if(cachedata == NULL){
		ndprint(CACHEDATA_ERROR,"ERROR: fun %s line %d\n", __FUNCTION__, __LINE__);
		return 0;
	}
	cachedata->Index = NULL;
	cachedata->Index = (unsigned int *)Nand_VirtualAlloc(UNIT_SIZE * indexcount);
	if (!(cachedata->Index)) {
		ndprint(CACHEDATA_ERROR,"ERROR: fun %s line %d\n", __FUNCTION__, __LINE__);
		Nand_VirtualFree(cachedata);
		return 0;
	}

	for (i = 0; i < indexcount; i++)
		cachedata->Index[i] = -1;
	
	cachedata->IndexID = -1;
	cachedata->IndexCount = indexcount;
	cachedata->unitLen = unitlen;
	cachedata->head.next = NULL;

	return cachedata;
}

/**
 *	CacheData_DeInit  -  Deinit operation
 *
 *	@cachedata: operate object
 */
void CacheData_DeInit ( CacheData *cachedata )
{
	Nand_VirtualFree(cachedata->Index);
	Nand_VirtualFree(cachedata);
}

/**
 *	CacheData_get  -  Get a pageid when given a sectorid
 *
 *	@cachedata: operate object
 *	@indexid: local sectorid
 */
unsigned int CacheData_get ( CacheData *cachedata, unsigned int indexid )
{
	unsigned int index;
	if(cachedata->IndexID == -1){
		ndprint(CACHEDATA_ERROR,"ERROR func %s line %d \n", __FUNCTION__, __LINE__);		
		return -1;
	}
	index = (indexid - cachedata->IndexID) / cachedata->unitLen;
	if (index < 0) {
		ndprint(CACHEDATA_ERROR,"ERROR: index = %d func %s line %d \n", index, __FUNCTION__, __LINE__);
		return -1;
	}
	return cachedata->Index[index];
}

/**
 *	CacheData_set  -  Inset or update a sectorid and corresponding pageid into cachedata
 *
 *	@cachedata: operate object
 *	@indexid: local sectorid
 *	@data: pageid
 */
void CacheData_set ( CacheData *cachedata,  unsigned int indexid, unsigned int data )
{
	unsigned int index;
	if(cachedata->IndexID == -1){
		ndprint(CACHEDATA_ERROR,"ERROR func %s line %d \n", __FUNCTION__, __LINE__);			
		return;
	}
	index = (indexid - cachedata->IndexID) / cachedata->unitLen;
	if (index < 0) {
		ndprint(CACHEDATA_ERROR,"ERROR: index = %d func %s line %d \n", index, __FUNCTION__, __LINE__);
		return;
	}
	
	cachedata->Index[index] = data;
}

/**
 *	CacheData_find  -  Get a sectorid when given a pageid
 *
 *	@cachedata: operate object
 *	@data: pageid
 */
unsigned int CacheData_find ( CacheData *cachedata, unsigned int data )
{
	int i;
	int flag = 0;

	for (i = 0; i < cachedata->IndexCount; i++) {
		if (cachedata->Index[i] == data) {
			flag =1;
			break;
		}
	}

	if (flag)
		return i * cachedata->unitLen + cachedata->IndexID;
	else
		return -1;
}

void CacheData_update ( CacheData *cachedata, unsigned int startID,unsigned char *data)
{
	cachedata->IndexID = startID;
	memcpy(cachedata->Index,data,cachedata->IndexCount << 2);
}
