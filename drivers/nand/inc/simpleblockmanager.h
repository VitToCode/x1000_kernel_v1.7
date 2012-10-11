#ifndef __SIMPLEBLOCKMANAGER_H__
#define __SIMPLEBLOCKMANAGER_H__

#include "sectorlist.h"
#include "vnandinfo.h"
#include "bufflistmanager.h"
#include "ppartition.h"
#include "pmanager.h"

#define SUCCESS		0
#define ENAND		-1        // common error      
#define DMA_AR		-2
#define IO_ERROR	-3     // nand status error
#define TIMEOUT		-4
#define ECC_ERROR	-5   //uncorrectable ecc errror
#define DATA_WRITED	-6
#define ERROR_BADBLOCK_TANTO	-7
#define MODE_ERROR		-8
#define ERROR_NOMEM		-9

#define BLOCK_FIRST_PAGE	0

#define	SIMP_WRITE		0
#define SIMP_READ		1




typedef struct _SmbContext SmbContext;

struct _SmbContext {
    VNandInfo vnand;
    int mode;
    int morebadblocks;
    int lblockid;
    int poffb;		//start page offset in block
    int spp;
    int spb;
    int ppb;
    int bpp;		// bytes per page
    BuffListManager* blm;
};

//int SimpleBlockManager_Write ( int context, SectorList *sl );
//int SimpleBlockManager_Read ( int context, SectorList *sl );
int SimpleBlockManager_Init(PManager* pm);
void SimpleBlockManager_Deinit(int handle);
#endif
