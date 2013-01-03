#ifndef __ZONEMANAGER_H__
#define __ZONEMANAGER_H__

#include "hashnode.h"
#include "hash.h"
#include "l1info.h"
#include "os/NandSemaphore.h"
#include "vnandinfo.h"
#include "sigzoneinfo.h"
#include "zone.h"
#include "pageinfo.h"
#include "pagelist.h"
#include "zoneidlist.h"
#include "utils/ndfifo.h"
typedef struct _Wpages Wpages;
struct _Wpages {
	unsigned int startpage;
	unsigned short pagecnt;
};

typedef struct _ZoneValidInfo ZoneValidInfo;
struct _ZoneValidInfo {
	Wpages *wpages;
	int zoneid;
	int current_count;
};

#define NULL_INIT  0
#define LOCAL_INIT 1
#define PRE_INIT   2
#define NEXT_INIT  4
#define ALL_INIT (LOCAL_INIT | PRE_INIT | NEXT_INIT)
typedef struct _ZoneManager ZoneManager;
struct _ZoneManager {
	HashNode* freeZone;
	Hash* useZone;
	L1Info *L1;
	SigZoneInfo* sigzoneinfo;
	unsigned int *sigzoneinfo_initflag;
	NandMutex HashMutex;
	VNandInfo *vnand;
	unsigned short* zoneID;
	unsigned int 	zoneIDcount;
	int zonemem;
	unsigned int l2infolen;
	unsigned int l3infolen;
	unsigned int l4infolen;
	unsigned char * mem0;
	unsigned char memflag[8];
	unsigned int maxserial;
	unsigned short last_zone_id;
	unsigned short last_rzone_id;
	Zone *last_zone;
	Zone *write_zone;
	Zone *ahead_zone[4];
	unsigned int aheadflag[4];
	SigZoneInfo *prev;
	SigZoneInfo *next;
	unsigned int pt_zonenum;
	int context;
	unsigned char *last_data_buf;
	PageInfo *last_pi;
	PageList *pl;
	unsigned int pagecount;
	ZoneValidInfo zonevalidinfo;
	int last_data_read_error;
	unsigned int old_l1info;
	//int badblockinfo;
	ZoneIDList *page0error_zoneidlist;
	ZoneIDList *page1error_zoneidlist;
	int page2_error_dealt;
	int runblockfifo;
	int dropzonefifo;
};

enum ErrType {
	PAGE0,
	PAGE1,
};

Zone *ZoneManager_Get_Used_Zone(ZoneManager *zonep, unsigned short zoneid);
Zone* ZoneManager_AllocZone (int context);
void ZoneManager_FreeZone (int context,Zone* zone );
void ZoneManager_DropZone (int context,Zone* zone );
L1Info* ZoneManager_GetL1Info (int context);
Hash* ZoneManager_GetUsedZoneTable (int context);
void ZoneManager_PutUsedZoneTable (int context,Hash* tbl );
int ZoneManager_Init (int context );
void ZoneManager_DeInit (int context );
unsigned short * ZoneManager_GetzoneIDarray (int context);
unsigned short  ZoneManager_RecyclezoneID(int,unsigned int);
unsigned short ZoneManager_ForceRecyclezoneID(int ,unsigned int lifetime);
Zone * ZoneManager_AllocRecyclezone(int context,unsigned short ZoneID);
void ZoneManager_FreeRecyclezone(int context,Zone *zone);
unsigned int ZoneManager_Getminlifetime(int context);
unsigned int ZoneManager_Getmaxlifetime(int context);
unsigned int ZoneManager_Getusedcount(int context);
unsigned int ZoneManager_Getfreecount(int context);
unsigned int ZoneManager_Getptzonenum(int context);
void  ZoneManager_SetCurrentWriteZone(int context,Zone *zone);
Zone *ZoneManager_GetCurrentWriteZone(int context);
unsigned int ZoneManager_SetAheadZone(int context,Zone *zone);
unsigned int ZoneManager_GetAheadZone(int context,Zone **zone);
unsigned int ZoneManager_GetAheadCount(int context);
void ZoneManager_SetPrevZone(int context,Zone *zone);
SigZoneInfo *ZoneManager_GetPrevZone(int context);
void ZoneManager_SetNextZone(int context,Zone *zone);
SigZoneInfo *ZoneManager_GetNextZone(int context);
int ZoneManager_convertPageToZone(int context,unsigned int pageid);
int ZoneManager_Move_UseZone_to_FreeZone(ZoneManager *zonep,unsigned short zoneID);
int ZoneManager_GetValidPage(int context,int zoneid);
void ZoneManager_SetRunBadBlock(int context,unsigned int pageid);
int ZoneManager_GetRunBadBlock(int context);
void ZoneManager_DelRunBadBlock(int context,int zoneid);
#endif
