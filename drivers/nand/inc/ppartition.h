#ifndef __PPARTITION_H__
#define __PPARTITION_H__


#include "singlelist.h"

#define DIRECT_MANAGER 0
#define ZONE_MANAGER   1
#define ONCE_MANAGER  2

typedef struct _PPartition PPartition;
typedef struct _PPartArray PPartArray;

struct badblockhandle {
	unsigned int *pt_badblock_info;
	unsigned int *pt_availableblockid;
};
struct virt2phy_page {
	int blm;
	int* retVal;
	unsigned short _2kPerPage;
};
struct _PPartition {
	const char *name;
	int startblockID;
	int pageperblock;
	int byteperpage;
	int totalblocks;
	int badblockcount;
	int hwsector;
	int startPage;
	int PageCount;
	int mode;
	void *prData;
	struct badblockhandle *badblock;
	struct virt2phy_page *v2pp;
};

#define PPARTITION(pt) ((PPartition *)pt)
struct _PPartArray{
	int ptcount;
	PPartition* ppt;
};

#endif
