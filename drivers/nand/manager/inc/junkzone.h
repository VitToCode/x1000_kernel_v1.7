#ifndef _JUNKZONE_H_
#define _JUNKZONE_H_

#include "NandSemaphore.h"
typedef struct _junkzonenode junkzonenode;
typedef struct _junkremovezone junkremovezone;
typedef struct _junkzone junkzone;
#define REMOVE_ZONECOUNT 2

#define REMOVE_ZONETYPE  1
#define CURRENT_ZONETYPE  2

struct _junkzonenode{
	unsigned int sectors;
	unsigned int type;
};

struct _junkremovezone {
	int id;
};

struct _junkzone{
	junkzonenode *node;
	NandMutex mutex;
	junkremovezone removezone[REMOVE_ZONECOUNT];
	int zonecount;
	int current_zoneid;
};

int Init_JunkZone(int zonecount);
void Deinit_JunkZone(int handle);
void Insert_JunkZone(int handle,int sectors,int zoneid);
int Get_MaxJunkZone(int handle,int minjunksector);
void Release_MaxJunkZone(int handle,int zoneid);
void Delete_JunkZone(int handle,int zoneid);
int Get_JunkZoneCount(int handle);
int Get_JunkZoneRecycleTrig(int handle);
void SetCurrentZoneID_JunkZone(int handle,int zoneid);
void dump_JunkZone(int handle);

#endif /* _JUNKZONE_H_ */
