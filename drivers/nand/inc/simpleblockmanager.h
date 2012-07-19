#ifndef __SIMPLEBLOCKMANAGER_H__
#define __SIMPLEBLOCKMANAGER_H__

#include "sectorlist.h"
#include "vnandinfo.h"
#include "bufflistmanager.h"
#include "ppartition.h"
#include "pmanager.h"

typedef struct _SmbContext SmbContext;

struct _SmbContext {
    VNandInfo* vnand;
    int mode;
    int morebadblock;
    int reqblockid;
    BuffListManager* blm;
};

//int SimpleBlockManager_Write ( int context, SectorList *sl );
//int SimpleBlockManager_Read ( int context, SectorList *sl );
int SimpleBlockManager_Init(PManager* pm);
void SimpleBlockManager_Deinit(int handle);
#endif
