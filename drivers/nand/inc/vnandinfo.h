#ifndef __VNANDINFO_H__
#define __VNANDINFO_H__

#include "nandinterface.h"
#include "ppartition.h"
#include "timerdebug.h"

typedef struct _VNandInfo VNandInfo;
typedef struct _VNandManager VNandManager;
struct _VNandInfo {
	int startBlockID;
	int PagePerBlock;
	int BytePerPage;
	int TotalBlocks;
	int* prData;
	int MaxBadBlockCount;
	unsigned short hwSector;
	int mode;
	struct badblockhandle *badblock;
	struct virt2phy_page *v2pp;
#ifdef STATISTICS_DEBUG
	TimeByte *timebyte;
#endif
};

struct _VNandManager {
	VNandInfo info;
	PPartArray* pt;
};

#define CONV_PT_VN(pt,vn)											\
	do{																\
		(vn)->startBlockID = 0;										\
		(vn)->PagePerBlock = (pt)->pageperblock * (pt)->v2pp->_2kPerPage;	\
		(vn)->BytePerPage = 2048;									\
		(vn)->TotalBlocks = (pt)->totalblocks;						\
		(vn)->MaxBadBlockCount = (pt)->badblockcount;				\
		(vn)->hwSector = (pt)->hwsector;							\
		(vn)->prData = (void*)(pt);									\
		(vn)->badblock = (pt)->badblock;							\
		(vn)->v2pp = (pt)->v2pp;									\
		(vn)->mode = (pt)->mode;									\
	}while(0)

#endif

