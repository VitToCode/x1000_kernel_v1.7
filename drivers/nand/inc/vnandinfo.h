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
	unsigned short _2kPerPage;
	int blm;
	void* align_rpl;
	int* retVal;
	unsigned int *pt_badblock_info;
#ifdef STATISTICS_DEBUG
	TimeByte *timebyte;
#endif
};

struct _VNandManager {
	VNandInfo info;
	PPartArray* pt;
};

#define CONV_PT_VN(pt,vn)							\
	do{									\
		(vn)->_2kPerPage = (pt)->byteperpage / 2048;			\
		(vn)->startBlockID = 0;						\
		(vn)->PagePerBlock = (pt)->pageperblock * (vn)->_2kPerPage;	\
		(vn)->BytePerPage = 2048;					\
		(vn)->TotalBlocks = (pt)->totalblocks;				\
		(vn)->MaxBadBlockCount = (pt)->badblockcount;			\
		(vn)->hwSector = (pt)->hwsector;				\
		(vn)->pt_badblock_info = (pt)->pt_badblock_info;		\
		(vn)->prData = (void*)(pt);					\
	}while(0)

#endif

