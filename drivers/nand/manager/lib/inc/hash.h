#ifndef __HASH_H__
#define __HASH_H__

#include "hashnode.h"

#define HASHNODE_COUNT 20

typedef struct _Hash Hash;

struct _Hash {
	HashNode **top;
	unsigned int usezone_count;	//count of use zone
	unsigned int maxlifetime;
	unsigned int minlifetime;
	int zoneID_count;		//count of zoneID[]
	int first_pos;		//the first position of find_lifetime in hashtable
	int prev_pos;		//the previous position of find_lifetime int hashtable
	unsigned int find_lifetime;	//the lifetime someone want to find
};

int Hash_init ( Hash **hash, SigZoneInfo *top, unsigned short *zoneid, int count );
void Hash_deinit (Hash **hash);
int Hash_Insert ( Hash *hash, SigZoneInfo *szi );
int Hash_delete ( Hash *hash, SigZoneInfo *szi );
int Hash_FindFirstLessLifeTime ( Hash *hash, unsigned int lifetime, SigZoneInfo **szi );
int Hash_FindNextLessLifeTime ( Hash *hash, int prev, SigZoneInfo **szi );
unsigned int Hash_getminlifetime (Hash *hash);
unsigned int Hash_getmaxlifetime (Hash *hash);
unsigned int Hash_getcount (Hash *hash);

#endif
