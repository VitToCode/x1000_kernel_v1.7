#ifndef _JUNKZONE_H_
#define _JUNKZONE_H_

#include "NandSemaphore.h"
typedef struct _junkzonenode junkzonenode;
typedef struct _junkremovezone junkremovezone;
typedef struct _junkzone junkzone;
#define REMOVE_ZONECOUNT 2
struct _junkzonenode{
	unsigned int sectors;
	unsigned int zoneid;

};

struct _junkremovezone {
	int index;
	int id;
};

struct _junkzone{
	junkzonenode *node;
	NandMutex mutex;
	junkremovezone removezone[REMOVE_ZONECOUNT];
	int zonecount;
};

int Init_JunkZone(int zonecount);
void Deinit_JunkZone(int handle);
void Insert_JunkZone(int handle,int sectors,int zoneid);
int Get_MaxJunkZone(int handle);
void Release_MaxJunkZone(int handle,int zoneid);
void Delete_JunkZone(int handle,int zoneid);
int Get_JunkZoneCount(int handle);

#endif /* _JUNKZONE_H_ */
