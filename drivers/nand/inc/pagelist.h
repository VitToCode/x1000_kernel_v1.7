#ifndef __PAGELIST_H__
#define __PAGELIST_H__

#include "singlelist.h"

typedef struct _PageList PageList;

struct _PageList {
	struct singlelist head;//must be the first member of the struct
	unsigned int startPageID;
	unsigned short OffsetBytes;
	unsigned short Bytes;
	void* pData;
	int retVal;
};

#endif
