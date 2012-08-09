#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include "testfunc.h"
#include "datacache.h"

extern NAND_CACHE nand_cache[CACHE_MAX_NUM];

static void dumpDataCache()
{
	int i;

	printf("========================================\n");
	
	for (i = 0; i < CACHE_MAX_NUM; i++) {
		if (nand_cache[i].Sector_Id != -1)
			printf("--[%d]--sectorid: %d --hitcount: %d --data: %s\n", i, nand_cache[i].Sector_Id, nand_cache[i].Hit_Count, nand_cache[i].Data);
	}
	
	printf("========================================\n");
 }

static int Handle(int argc, char *argv[]){
	int i, ret;
	unsigned short cacheid;
	unsigned char buf0[SECTOR_SIZE] = "1234567890";
	unsigned char buf1[SECTOR_SIZE] = "abcdefghijklmnopqrstuvwxyz";
	unsigned char buf2[SECTOR_SIZE];

	nand_cache_init();
	printf("nand_cache_init OK!\n");

	for (i = 0; i < CACHE_MAX_NUM; i++) {
		cacheid = apply_cache();
		copy_data_to_cache(cacheid, i, buf0, 0);
		
		dumpDataCache();
	}

	for (i = 0; i < 2 * CACHE_MAX_NUM; i++) {
		ret = is_cache_hit(i, &cacheid);
		if (ret) {
			get_data_from_cache(cacheid, buf2);
			printf("cache hit. i = %d, data: %s\n", i,buf2);
		}
		else {
			printf("cache miss. i = %d\n", i);
			cacheid = apply_cache();
			copy_data_to_cache(cacheid, i, buf1, 1);
		}
		
		dumpDataCache();

		//sleep(1);
	}

	return 0;
}



static char *mycommand="dc";
int GetInterface(PTestFunc *f, char **command){
	
	*f = Handle;
	*command = mycommand;
	return 0;
}
