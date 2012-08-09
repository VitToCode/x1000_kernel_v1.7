/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2007 by dsqiu (dsqiu@ingenic.cn)
 */

#include "testfunc.h"
#include <stdio.h>
#include "memdealer.h"

#include <malloc.h>
static int Handle(int argc, char *argv[]){
	unsigned int *heap;
	unsigned int *buf;
	unsigned int **p;
	int i;
	int did;
	p = malloc(512*1024);
	heap = malloc(51 * 1024);
	printf("test...!\n");
	did = InitContinueMemory(heap,51*1024);
	buf = Allocate(did,4*1024);
	for(i = 0;i < 4;i++)
		p[i] = Allocate(did,1024);
	//Deallocate(p[2]);
//	dumplist();
	for(i = 4;i < 1024*512/50;i++){
		p[i] = Allocate(did,50);
		//printf("p[%d] = %p\n",i,p[i]);

	}

	//p[2] = Allocate(1024);

//	dumplist();


	
	for(i = 0;i < 1024*512/50;i++){
		printf("p[%d] = %p\n",i,p[i]);
		Deallocate(did,p[i]);
		/*
		if(i % 20 == 0){
			
			Deallocate(buf);
			buf = Allocate(rand()%4*1024);
			printf("buf = %p\t",buf);
		}
		*/
	}
	if(buf)
		Deallocate(did,buf);
	buf = Allocate(did,51);
	if(buf)
		Deallocate(did,buf);

	dumplist();
	printf("heap:%p  buf:%p\n",heap,buf);
	if(buf)
		Deallocate(did,buf);
	
	DeinitContinueMemory(did);	
	printf("finish!\n");
	free(heap);
	free(p);
	return 0;
}



static char *mycommand="mm";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
