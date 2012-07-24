#ifndef __VNAND_H__
#define __VNAND_H__

#include "vnandinfo.h"
#include "nandinterface.h"
#include "hashnode.h"
#include "hash.h"
#include "l1info.h"
#include "sigzoneinfo.h"

#define VNANDCACHESIZE 32 * 1024

int vNand_PageRead (VNandInfo* vNand,int pageid, int offsetbyte, int bytecount, void * data );
int vNand_PageWrite (VNandInfo* vNand,int pageid, int offsetbyte, int bytecount, void* data );
int vNand_MultiPageRead (VNandInfo* vNand,PageList* pl );
int vNand_MultiPageWrite (VNandInfo* vNand,PageList* pl );
int vNand_CopyData (VNandInfo* vNand,PageList* rpl, PageList* wpl );
int vNand_MultiBlockErase (VNandInfo* vNand,BlockList* pl );
int vNand_IsBadBlock (VNandInfo* vNand,int blockid );
int vNand_MarkBadBlock (VNandInfo* vNand,unsigned int blockid);

int vNand_Init ( VNandManager** context );
void vNand_Deinit ( VNandManager** context );
void Register_StartNand(void (*start)(int),int context);

int vNand_register_nanddriver (int context,NandInterface* interface);

#endif
