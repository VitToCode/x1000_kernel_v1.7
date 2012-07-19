#ifndef __SECTORLIST_H__
#define __SECTORLIST_H__

#include "singlelist.h"

typedef struct _SectorList SectorList;
struct _SectorList {
	struct singlelist head;//must be the first member of the struct
	unsigned int startSector;
	int sectorCount;
	void *pData;
};

#endif
