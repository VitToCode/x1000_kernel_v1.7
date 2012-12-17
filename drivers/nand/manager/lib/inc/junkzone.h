#ifndef _JUNKZONE_H_
#define _JUNKZONE_H_

#include "os/NandSemaphore.h"
typedef struct _junkzonenode junkzonenode;
typedef struct _junkremovezone junkremovezone;
typedef struct _junkzone junkzone;
#define REMOVE_ZONECOUNT 2

#define REMOVE_ZONETYPE   1
#define CURRENT_ZONETYPE  2
#define DROP_ZONETYPE     3

struct _junkzonenode{
	unsigned int sectors;
	unsigned int type;
	unsigned int timestamp;
};


struct _junkzone{
	int context;
	junkzonenode *node;
	NandMutex mutex;
	int zonecount;
	int current_zoneid;
	int maxjunksectors;
	unsigned int max_timestamp;
	junkzonenode *min_node;
};
int Init_JunkZone(int context,int zonecount);
void Deinit_JunkZone(int handle);
void Insert_JunkZone(int handle,int sectors,int zoneid);
int Get_MaxJunkZone(int handle,int minjunksector);
void Release_MaxJunkZone(int handle,int zoneid);
void Delete_JunkZone(int handle,int zoneid);
int Get_JunkZoneCount(int handle);
int Get_JunkZoneRecycleTrig(int handle,int minpercent);
void SetCurrentZoneID_JunkZone(int handle,int zoneid);
void dump_JunkZone(int handle);
int Get_JunkZoneSectors(int handle,unsigned int zid);
void Set_BufferToJunkZone(int handle,void *d,int bytes,int maxsectors);
int Get_JunkZoneToBuffer(int handle,void *d,int bytes,int *rank,int count);
int Get_JunkZoneForceRecycleTrig(int handle,int percent);
int Get_JunkZoneRecycleZone(int handle);
void Drop_JunkZone(int handle,int zoneid);
#endif /* _JUNKZONE_H_ */
