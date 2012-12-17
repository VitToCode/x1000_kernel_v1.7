#ifndef __HASHNODE_H__
#define __HASHNODE_H__

#include "sigzoneinfo.h"

typedef struct _HashNode HashNode;
struct _HashNode {
	unsigned short head;
	unsigned short tail;
	unsigned int count;
	unsigned int maxlifetime;
	unsigned int minlifetime;
	SigZoneInfo *base_szi; 	//base address of Sigzoneinfo[]
	unsigned short *zoneID; 	//save offset of each sigzoneinfo
	int zoneID_count; 		//count of zoneID[]
	unsigned int find_lifetime; 	//the lifetime someone want to find
};

int HashNode_init ( HashNode **hashnode, SigZoneInfo *top, unsigned short *zoneid, int count);
void HashNode_deinit ( HashNode **hashnode );
int HashNode_insert ( HashNode *hashnode, SigZoneInfo *sigzoneinfo );
int HashNode_delete ( HashNode *hashnode, SigZoneInfo *sigzoneinfo );
unsigned int HashNode_getminlifetime (HashNode *hashnode);
unsigned int HashNode_getmaxlifetime (HashNode *hashnode);
unsigned int HashNode_getcount (HashNode *hashnode);
int HashNode_FindFirstLessLifeTime(HashNode *hashnode, unsigned int lifetime, SigZoneInfo **sigzoneinfo);
int HashNode_FindNextLessLifeTime(HashNode *hashnode, int prev, SigZoneInfo **sigzoneinfo);
SigZoneInfo *HashNode_get (HashNode *hashnode);
unsigned short HashNode_peekZoneID(HashNode *hashnode,unsigned short offset);
SigZoneInfo *HashNode_getTop (HashNode *hashnode);
#endif
