#ifndef __L2PCONVERT_H__
#define __L2PCONVERT_H__

#define MAXDIFFTIME 20

#include "context.h"
#include "partitioninterface.h"
#include "bufflistmanager.h"
#include "pmanager.h"
#include "vnandinfo.h"
#include "ppartition.h"
#include "sectorlist.h"

enum cmd {
	SUSPEND,
	RESUME,
};

int L2PConvert_Init(PManager *pm);
void L2PConvert_Deinit(int handle);
int L2PConvert_ZMOpen(VNandInfo *vnand, PPartition *pt);
int L2PConvert_ZMClose(int handle);
int L2PConvert_ReadSector ( int handle, SectorList *sl );
int L2PConvert_WriteSector ( int handle, SectorList *sl );
int L2PConvert_Ioctrl(int handle, int cmd, int argv);

#endif
