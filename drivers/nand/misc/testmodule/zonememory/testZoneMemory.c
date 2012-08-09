/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2007 by dsqiu (dsqiu@ingenic.cn)
 */

#include "testfunc.h"
#include "zonememory.h"
#include <string.h>

void dumpZoneMemory(ZoneMemory *z){
	printf("ZoneMemory->top = %p\n",z->top);
	printf("ZoneMemory->usize = %d\n",z->usize);
}

void dumpBitmap(unsigned int *bi,int bitsize){
	int i,j;
	
	printf("==bitmap== %d\n",bitsize);
	for(i = 0;i < bitsize / 32;i++){
		printf("\n%08d:\t",32*i);
		for(j = 0;j < 32;j++){
			if(j % 8 == 0) printf(" | ");	
			if((*bi & (1 << j)) == 0)
				printf("%s","O");
			else
				printf("%s","H");
		}
		bi++;
	}
	for(j = 0;j < (bitsize & 31);j++){
			if(j % 8 == 0) printf(" | ");	
			if((*bi & (1 << j)) == 0)
				printf("%s","O");
			else
				printf("%s","H");
	}
	printf("\n");
}
void dumpZoneBuffer(ZoneBuffer *zb){
	struct singlelist *p;
	ZoneBuffer* t;
	singlelist_for_each(p,&zb->head){
		t = singlelist_entry(p,ZoneBuffer,head);
		printf("========%p======\n",t);
		printf("ZoneBuffer->mBuffer = %p\n",t->mBuffer);
		printf("ZoneBuffer->bitmap = %p\n",t->bitmap);
		printf("ZoneBuffer->bitsize = %d\n",t->bitsize);
		printf("ZoneBuffer->head = %p\n",&t->head);
		dumpBitmap(t->bitmap,t->bitsize);
	}
}
static int Handle(int argc, char *argv[]){
	int zid;
	ZoneMemory *z;
	unsigned int **p=NULL,*o;
	int i,j;
	zid = ZoneMemory_Init(160);
	z = (ZoneMemory*) zid;
	o = ZoneMemory_NewUnit(zid);
	memset(o,0,160);
	o = ZoneMemory_NewUnit(zid);
	memset(o,0,160);
	printf("release o = %p\n",o);
	ZoneMemory_DeleteUnit(zid,o);
	o = ZoneMemory_NewUnit(zid);
	memset(o,0,160);
	
	dumpZoneMemory(z);
	
	dumpZoneBuffer(z->top);

	p = malloc(1000 * 4 * 2);
	for(i = 0; i < 1;i++){
		/*
		if((i % 5 == 0))
		{
			printf("alloc\n");
			dumpZoneBuffer(z->top);
		}
		*/
		p[2*i] = (unsigned int *)ZoneMemory_NewUnit(zid);
		memset(p[2*i],0,160);
		p[2*i+1] = (unsigned int *)ZoneMemory_NewUnits(zid,2);
		memset(p[2*i+1],0,160*2);
	}
	printf("\n=================================================\n");
	printf("alloc\n");
	//dumpZoneBuffer(z->top);
	printf("\n=================================================\n");
	for(i = 0; i < 1;i++){

		if(i % 10 == 0)
		{
			printf("\nfree\n");
			dumpZoneBuffer(z->top);
		}

		for(j = 0; j < 1;j++){
			printf("====================\n");
			p[2*j] = (unsigned int *)ZoneMemory_NewUnit(zid);
			memset(p[2*j],0,160);
			p[2*j+1] = (unsigned int *)ZoneMemory_NewUnits(zid,2);
			memset(p[2*j+1],0,160*2);
		}

		ZoneMemory_DeleteUnit(zid,p[2*i]);
		ZoneMemory_DeleteUnits(zid,p[2*i+1],2);
	}
	ZoneMemory_DeleteUnit(zid,(void *)0x99999999);
	free(p);
	printf("\n=================================================\n");	
	printf("free\n");
	dumpZoneBuffer(z->top);

	printf("\n=================================================\n");
	p = malloc(1000 * 4);
	for(i = 0; i < 2;i++){
		if((i % 5 == 0))
		{
			printf("alloc\n");
			dumpZoneBuffer(z->top);
		}

		p[i] = (unsigned int *)ZoneMemory_NewUnits(zid,20);
	}
printf("\n=================================================\n");
	printf("alloc\n");
	dumpZoneBuffer(z->top);
printf("\n=================================================\n");
	free(p);
	
	


	ZoneMemory_DeInit(zid);
	
	printf("finish!\n");
	return 0;
}



static char *mycommand="zm";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
