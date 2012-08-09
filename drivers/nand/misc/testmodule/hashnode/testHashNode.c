#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "testfunc.h"
#include "hashnode.h"

static void dumpHashNode(HashNode *hashnode, SigZoneInfo *top, unsigned short *zoneID)
{
	unsigned short offset;
	
	printf("\nhead = %d, tail = %d, minlifetime = %d, maxlifetime = %d, count = %d\n", 
		(short)(hashnode->head), (short)(hashnode->tail), HashNode_getminlifetime(hashnode), HashNode_getmaxlifetime(hashnode), HashNode_getcount(hashnode));

	if (HashNode_getcount(hashnode) == 0)
		return;
	
	printf("==================================================================\n");	
	offset = hashnode->head;
	while (1) {		
		printf("--[%d]--(%d)--lifetime:%d--\n", offset, (short)zoneID[offset],(top + offset)->lifetime);

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

static int Handle(int argc, char *argv[]){
	int i, ret;
	int count = 10;
	unsigned int find_lifetime = 800;
	unsigned short zoneid[count * count];
	HashNode *hashnode;
	SigZoneInfo sigzoneinfo[count];
	SigZoneInfo szi;
	SigZoneInfo *szi_get;
	SigZoneInfo *szi_find = &szi;

	for (i = 0; i < count; i++) {
		//sigzoneinfo[i].lifetime = random();
		sigzoneinfo[i].lifetime = 100 * i + 10;
		//sigzoneinfo[i].lifetime = 100;
	}
	
	ret = HashNode_init(&hashnode, sigzoneinfo, zoneid, count * count);
	if (ret == -1) {
		printf("HashNode_init failed!\n");
		return -1;
	}
	printf("=====HashNode_init OK!=====\n");	
	dumpHashNode(hashnode, sigzoneinfo, zoneid);
	dumpAllZoneID(zoneid, count * count);

	for (i = 0; i < count; i++) {
		ret = HashNode_insert(hashnode, &sigzoneinfo[i]);
		if (ret < 0) {
			printf("HashNode_insert failed\n");
			return -1;
		}
		dumpHashNode(hashnode, sigzoneinfo, zoneid);
		dumpAllZoneID(zoneid, count * count);
	}
	printf("=====insert %d sigzoneinfo completed!=====\n", count);

	ret = HashNode_FindFirstLessLifeTime(hashnode,find_lifetime,&szi_find);
	if (ret != -1) {
		printf("\nfind first lifetime < %d is: %d\n", find_lifetime, szi_find->lifetime);
		sleep(1);

		while (1) {
			ret = HashNode_FindNextLessLifeTime(hashnode,ret,&szi_find);
			if (ret != -1) {
				printf("find next lifetime < %d is: %d\n", find_lifetime, szi_find->lifetime);
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
		printf("Can't find lifetime < %d, minlifetime of hashnode is %d!\n", find_lifetime, HashNode_getminlifetime(hashnode));
		sleep(1);
	}

	for (i = 0; i < 3; i++) {
		szi_get = HashNode_get(hashnode);
		if (!szi_get) {
			printf("get sigzoneinfo failed!\n");
			return -1;
		}
		printf("\nget sigzoneinfo szi_get->lifetime = %d", szi_get->lifetime);
		dumpHashNode(hashnode, sigzoneinfo, zoneid);
		dumpAllZoneID(zoneid, count * count);
		sleep(1);
	}

	for (i = 0; i < count; i++) {
	//for (i = count - 1; i >= 0; i--) {
		ret = HashNode_delete(hashnode, &sigzoneinfo[i]);
		if (ret < 0)
			printf("HashNode_delete failed\n");
		dumpHashNode(hashnode, sigzoneinfo, zoneid);
		dumpAllZoneID(zoneid, count * count);
	}
	printf("=====delete %d sigzoneinfo completed!=====\n", count);

	HashNode_deinit(&hashnode);
	dumpAllZoneID(zoneid, count * count);
	printf("=====HashNode_deinit OK!=====\n");
	
	return 0;
}



static char *mycommand="hn";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
