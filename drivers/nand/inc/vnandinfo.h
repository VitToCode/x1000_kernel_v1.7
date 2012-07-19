#ifndef __VNANDINFO_H__
#define __VNANDINFO_H__

#include "nandinterface.h"
#include "ppartition.h"

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
    unsigned int *pt_badblock_info;
};

struct _VNandManager {
	VNandInfo info;
    PPartArray* pt;
};

#define CONV_PT_VN(pt,vn)												\
	do{																	\
		(vn)->startBlockID = (pt)->startblockID;						\
		(vn)->PagePerBlock = (pt)->pageperblock;						\
		(vn)->BytePerPage = (pt)->byteperpage;							\
		(vn)->TotalBlocks = (pt)->totalblocks;							\
		(vn)->MaxBadBlockCount = (pt)->badblockcount;					\
		(vn)->hwSector = (pt)->hwsector;								\
		(vn)->prData = (void*)(pt);										\
	}while(0)

#endif

