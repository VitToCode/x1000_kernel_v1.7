#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "testfunc.h"
#include "hash.h"

#define HASHNODE_COUNT 20

static void dumpHashNode(HashNode *hashnode, SigZoneInfo *top, unsigned short *zoneID)
{
	unsigned short offset;
	
	printf("head = %d, tail = %d, minlifetime = %d, maxlifetime = %d, count = %d\n", 
		(short)(hashnode->head), (short)(hashnode->tail), HashNode_getminlifetime(hashnode), HashNode_getmaxlifetime(hashnode), HashNode_getcount(hashnode));

	if (HashNode_getcount(hashnode) == 0)
		return;
	
	printf("==================================================================\n");	
	offset = hashnode->head;
	while (1) {		
		printf("--[%d]--(%d)--lifetime:%d--\n", offset, (short)zoneID[offset],(offset + top)->lifetime);

		offset = zoneID[offset];
		if (offset == (unsigned short)(-1))
			break;
	}
	printf("==================================================================\n");	
}

static void dumpAllZoneID(unsigned short *zoneID, int count)
{
	int i;

	for (i = 0; i < count; i++) {
		if (zoneID[i] != (unsigned short)(-1))
			printf("--[%d]--(%d)--\n", i, (short)zoneID[i]);
	}
}

static void dumpHash(Hash *hash, SigZoneInfo *top, unsigned short *zoneID)
{
	int i;
	
	printf("\nhash:minlifetime = %d, maxlifetime = %d, usezone_count = %d\n", 
		Hash_getminlifetime(hash), Hash_getmaxlifetime(hash), Hash_getcount(hash));

	if (hash->usezone_count == 0)
		return;
	
	for (i = 0; i < HASHNODE_COUNT; i++) {
		if (HashNode_getcount(hash->top[i]) != 0)
			dumpHashNode(hash->top[i], top, zoneID);
	}
}

static int Handle(int argc, char *argv[]){
	int i, ret;
	int count = 10;
	unsigned int find_lifetime = 444;
	Hash *hash;
	unsigned short zoneid[count * count];
	SigZoneInfo sigzoneinfo[count];
	SigZoneInfo szi;
	SigZoneInfo *szi_find = &szi;
	
	for (i = 0; i < count; i++) {
		//sigzoneinfo[i].lifetime = random() % 3000;
		sigzoneinfo[i].lifetime = 103 * i + 4;
		//sigzoneinfo[i].lifetime = 100;
	}
	
	ret = Hash_init(&hash, sigzoneinfo, zoneid, count * count);
	if (ret == -1) {
		printf("Hash_init failed!\n");
		return -1;
	}
	printf("\n=====Hash_init OK!=====");	
	dumpHash(hash, sigzoneinfo, zoneid);
	dumpAllZoneID(zoneid,count * count);

	for (i = 0; i < count; i++) {
		ret = Hash_Insert(hash, &sigzoneinfo[i]);
		if (ret < 0) {
			printf("HashNode_insert failed\n");
			return -1;
		}
		dumpHash(hash, sigzoneinfo, zoneid);
		dumpAllZoneID(zoneid, count * count);
	}
	printf("=====insert %d sigzoneinfo completed!=====\n", count);
	
	ret = Hash_FindFirstLessLifeTime(hash,find_lifetime,&szi_find);
	if (ret != -1) {
		printf("\nfind first lifetime < %d is: %d\n", find_lifetime, szi_find->lifetime);
		sleep(1);

		while (1) {
			ret = Hash_FindNextLessLifeTime(hash,ret,&szi_find);
			if (ret != -1) {
				printf("find next lifetime < %d is: %d\n",find_lifetime,  szi_find->lifetime);
				sleep(1);
			}
			else {
				printf("Can't find no more!\n");
				sleep(1);
				break;
			}
		}
	}
	else {
		printf("Can't find lifetime < %d, minlifetime of hash is %d!\n", find_lifetime, Hash_getminlifetime(hash));
		sleep(1);
	}
	dumpHash(hash, sigzoneinfo, zoneid);
	dumpAllZoneID(zoneid, count * count);

	for (i = 0; i < count; i++) {
	//for (i = count - 1; i >= 0; i--) {
		ret = Hash_delete(hash, &sigzoneinfo[i]);
		if (ret < 0)
			printf("Hash_delete failed\n");
		dumpHash(hash, sigzoneinfo, zoneid);
		dumpAllZoneID(zoneid, count * count);
	}
	printf("=====delete %d sigzoneinfo completed!=====\n", count);

	Hash_deinit(&hash);
	dumpAllZoneID(zoneid, count * count);
	printf("=====Hash_deinit OK!=====\n");
	
	return 0;
}



static char *mycommand="hash";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
