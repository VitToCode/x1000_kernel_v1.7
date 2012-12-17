#ifndef __L1INFO_H__
#define __L1INFO_H__

#include "os/NandSemaphore.h"

typedef struct _L1Info L1Info;

struct _L1Info {
    unsigned int* page;
    int len;
    NandMutex mutex;
};

/** Operations **/
/*public*/
unsigned int L1Info_get ( int context,unsigned int sectID );
void L1Info_set ( int context,unsigned int SectorID, unsigned int PageID );
#endif
