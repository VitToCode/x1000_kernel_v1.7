#include "os/clib.h"
#include "cachedata.h"
#include "os/NandAlloc.h"
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
	if (index < 0 || index >= cachedata->IndexCount) {
		ndprint(CACHEDATA_ERROR, "ERROR: index = %d func %s line %d \n", index, __FUNCTION__, __LINE__);
		ndprint(CACHEDATA_ERROR, "ERROR: indexid = %d, IndexID = %d, unitLen = %d, IndexCount = %d\n",
				indexid, cachedata->IndexID, cachedata->unitLen, cachedata->IndexCount);
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
/**
 *	CacheData_ismatch  -  Whether indexid in the cachedata or not
 *
 *	@cachedata: operate object
 *	@indexid: sectorid
 */
int CacheData_ismatch ( CacheData *cachedata, unsigned int indexid)
{
	return  (indexid >= cachedata->IndexID &&
		indexid < cachedata->IndexID + cachedata->unitLen * cachedata->IndexCount && cachedata->IndexID != -1); 
}

void CacheData_Dropdata(CacheData *cachedata)
{
        if(cachedata)
                memset(cachedata->Index, 0xff , cachedata->IndexCount * sizeof(unsigned int));
#if 0
        int i = 0;
        for (i=0; i<cachedata->IndexCount; i++) {
                if(cachedata->Index[i] != -1)
                        ndprint(CACHEDATA_ERROR,"i = %d cachedata->Index[%d] = %d\n"
                                        , i, i, cachedata->Index[i]);
        }
#endif
}

void CacheData_Dump( CacheData *cd) {
	int i;
	ndprint(CACHEDATA_INFO,"----- cachedata[%p] IndexID = %d IndexCount = %d unitLen = %d ---\n",
		cd,cd->IndexID,cd->IndexCount,cd->unitLen);
	for(i = 0;i < cd->IndexCount;i++){
		if(i % 16 == 0){
			ndprint(CACHEDATA_INFO,"\n%8d:",cd->IndexID + i);
		}
		ndprint(CACHEDATA_INFO,"%8d\t",cd->Index[i]);
	}
	ndprint(CACHEDATA_INFO,"\n");
}
