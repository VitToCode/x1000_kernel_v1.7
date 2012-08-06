#ifndef _JUNKZONE_H_
#define _JUNKZONE_H_

#include "NandSemaphore.h"
typedef struct _junkzonenode junkzonenode;
typedef struct _junkzone junkzone;
#define REMOVE_ZONECOUNT 2
struct _junkzonenode{
	unsigned short sectors;
	unsigned short zoneid;
	
};
struct _junkzone{
	junkzonenode *node;
	NandMutex mutex;
	int removezoneid[REMOVE_ZONECOUNT];
	int zonecount;
};

int Init_JunkZone(int zonecount);
void Deinit_JunkZone(int handle);
void Insert_JunkZone(int handle,int sectors,int zoneid);
int Get_MaxJunkZone(int handle);
void Release_MaxJunkZone(int handle,int zoneid);

#endif /* _JUNKZONE_H_ */
