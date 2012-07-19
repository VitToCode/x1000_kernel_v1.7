#ifndef __PARTITIONINTERFACE_H__
#define __PARTITIONINTERFACE_H__

#include "vnandinfo.h"
#include "sectorlist.h"

#define PARTITIONINTERFACE(OBJ) ((PartitionInterface*)OBJ)


typedef struct _PartitionInterface PartitionInterface;

struct  _PartitionInterface {
	int (*PartitionInterface_iOpen)( VNandInfo* vn, PPartition* pt );
	int (*PartitionInterface_iClose)( int handle );
	int (*PartitionInterface_Read)( int context, SectorList* sl );
	int (*PartitionInterface_Write)( int context, SectorList* sl );
	int (*PartitionInterface_Ioctrl)( int context, int cmd, int argv );
};

#endif
