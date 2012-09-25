#ifndef __VNAND_H__
#define __VNAND_H__

#include "vnandinfo.h"
#include "nandinterface.h"
#include "hashnode.h"
#include "hash.h"
#include "l1info.h"
#include "sigzoneinfo.h"

#define VNANDCACHESIZE 32 * 1024
#define BADBLOCKINFOSIZE 1

int vNand_PageRead (VNandInfo* vNand,int pageid, int offsetbyte, int bytecount, void * data );
int vNand_PageWrite (VNandInfo* vNand,int pageid, int offsetbyte, int bytecount, void* data );
int __vNand_MultiPageRead (VNandInfo* vNand,PageList* pl );
int __vNand_MultiPageWrite (VNandInfo* vNand,PageList* pl );
int __vNand_CopyData (VNandInfo* vNand,PageList* rpl, PageList* wpl );
int __vNand_MultiBlockErase (VNandInfo* vNand,BlockList* pl );
int __vNand_IsBadBlock (VNandInfo* vNand,int blockid );
int vNand_MarkBadBlock (VNandInfo* vNand,unsigned int blockid);

int  __vNand_Init ( VNandManager** context );
void __vNand_Deinit ( VNandManager** context );
void Register_StartNand(void *start,int context);

int vNand_register_nanddriver (int context,NandInterface* interface);

#endif
