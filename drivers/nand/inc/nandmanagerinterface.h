#ifndef __NANDMANGER_H__
#define __NANDMANGER_H__

#include "pmanager.h"
#include "sectorlist.h"
#include "lpartition.h"
#include "partitioninterface.h"


/*public*/
int  NandManger_open ( int handle,const char* name, int mode );
int NandManger_read ( int context, SectorList* bl );
int NandManger_write ( int context, SectorList* bl );
int NandManger_ioctrl ( int context, int cmd, int args );
int NandManger_close ( int context );
int NandManger_getPartition ( int handle, LPartition** pt );
int NandManger_Init ( void );
void NandManger_Deinit (int handle);
int NandManger_Register_Manager ( int handle, int mode, PartitionInterface* pi );
void NandManger_startNotify(int handle,void (*start)(int),int prdata);
#endif
