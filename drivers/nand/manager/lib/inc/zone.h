#ifndef __ZONE_H__
#define __ZONE_H__

#include "sigzoneinfo.h"
#include "pageinfo.h"
#include "pagelist.h"
#include "vnandinfo.h"

#define BLOCK_PER_ZONE             	4
#define BLOCKPERZONE(context)           BLOCK_PER_ZONE
#define REREAD_PAGEINFO
typedef struct _Zone Zone;

struct _Zone {
	int startblockID;
	int endblockID;
	unsigned int currentLsector;
	unsigned short ZoneID;
	unsigned short pageCursor;
	unsigned short allocPageCursor;
	SigZoneInfo *top;
	SigZoneInfo *prevzone;
	SigZoneInfo *nextzone;
	SigZoneInfo *sigzoneinfo;
	VNandInfo *vnand;
	int context;
	void *mem0;
	unsigned char memflag;
	unsigned short NextPageInfo;/*record next infopage  pageID */
	unsigned short sumpage;
	unsigned short allocedpage;
	unsigned short L1InfoLen;
	unsigned short L2InfoLen;
	unsigned short L3InfoLen;
	unsigned short L4InfoLen;
	unsigned char *L1Info;
};

int Zone_FindFirstPageInfo ( Zone *zone, PageInfo* pi );
int Zone_FindNextPageInfo ( Zone *zone, PageInfo* pi );
int Zone_ReleasePageInfo ( Zone *zone, PageInfo* pi );
int Zone_ReadPageInfo ( Zone *zone, unsigned int pageID, PageInfo* pi );
int Zone_MultiWritePage ( Zone *zone, unsigned int pagecount, PageList* pl, PageInfo* pi );
int Zone_RawMultiReadPage ( Zone *zone, PageList* pl );
int Zone_AllocNextPage ( Zone *zone );
int Zone_AllocSkipToBlock ( Zone *zone );
int Zone_MarkEraseBlock ( Zone *zone, unsigned int PageID, int Mode );
int Zone_Init ( Zone *zone, SigZoneInfo* prev, SigZoneInfo* next );
int Zone_DeInit ( Zone *zone );
int Zone_RawMultiWritePage ( Zone *zone, PageList* pl );
unsigned short Zone_GetFreePageCount(Zone *zone);
int Pageinfo_Reread(Zone *zone, int pageid, int blm);
int Zone_SeekToNextPageInfo ( Zone *zone,unsigned int startpageid,PageInfo* pi);
#endif