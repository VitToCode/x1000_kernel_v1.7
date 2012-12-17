/**
 * NandAlloc.c
 **/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>
#include <sys/types.h>

#include "os/NandAlloc.h"

void *Nand_VirtualAlloc(int size)
{
	return malloc(size);
}

void Nand_VirtualFree(void *val)
{
	return free(val);
}

void *Nand_ContinueAlloc(int size)
{
	void *v = malloc(size);
	if(v == 0)
		printf("==================== size = %d\n",size);
	return v;
}

void Nand_ContinueFree(void *val)
{
	return free(val);
}
