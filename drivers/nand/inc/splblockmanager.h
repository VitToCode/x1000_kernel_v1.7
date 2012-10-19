#ifndef __SPLBLOCKMANAGER_H__
#define __SPLBLOCKMANAGER_H__

#include "sectorlist.h"
#include "vnandinfo.h"
#include "bufflistmanager.h"
#include "ppartition.h"
#include "pmanager.h"

#define SUCCESS		0
#define ENAND		-1      // common error      
#define DMA_AR		-2
#define IO_ERROR	-3 		// nand status error
#define TIMEOUT		-4
#define ECC_ERROR	-5   	//uncorrectable ecc errror
#define DATA_WRITED	-6
#define ERROR_BADBLOCK_TANTO	-7
#define MODE_ERROR		-8
#define ERROR_NOMEM		-9

#define BLOCK_FIRST_PAGE	0
#define SECTOR_SIZE 	512
// spl size is 16k, so x-boot offset start at sector 16k / SECTOR_SIZE
#define X_BOOT_OFFSET 	((16 * 1024) / SECTOR_SIZE)
#define X_BOOT_BLOCK	2 	// block 0,1 write spl, block 2 write x-boot
#define X_BOOT_START_SECTOR(spb) (X_BOOT_BLOCK * (spb))

#define	SPL_WRITE		0
#define SPL_READ		1

typedef struct _SplContext SplContext;

struct _SplContext {
    VNandInfo vnand;
    int mode;
    int morebadblocks;
    int lblockid;
    int pblockid;
    int poffb;		//start page offset in block
    int spp;
    int spb;
    int ppb;
    int bpp;		// bytes per page
    BuffListManager* blm;
};

int SplBlockManager_Init(PManager* pm);
void SplBlockManager_Deinit(int handle);
#endif
