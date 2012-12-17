#ifndef __LPARTITION_H__
#define __LPARTITION_H__

#define LPARTITION(OBJ) ((LPartition*)OBJ)

#ifndef String
#define String char*
#endif

#include "singlelist.h"
typedef struct _LPartition LPartition;

struct _LPartition {
	struct singlelist head;
    int startSector;
    int sectorCount;
    const char* name;
    int mode;
    int pc; /*partcontext*/
	
	int hwsector;
	unsigned int segmentsize;
};

#endif
