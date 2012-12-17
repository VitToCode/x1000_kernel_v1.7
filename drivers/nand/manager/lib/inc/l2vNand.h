#ifndef __L2VNAND_H__
#define __L2VNAND_H__

#include "vnandinfo.h"
#include "nandinterface.h"
#include "hashnode.h"
#include "hash.h"
#include "l1info.h"
#include "sigzoneinfo.h"

int vNand_MultiPageRead (VNandInfo* vNand,PageList* pl );
int vNand_MultiPageWrite (VNandInfo* vNand,PageList* pl );
int vNand_CopyData (VNandInfo* vNand,PageList* rpl, PageList* wpl );
int vNand_MultiBlockErase (VNandInfo* vNand,BlockList* pl );
int vNand_IsBadBlock (VNandInfo* vNand,int blockid );
int vNand_MarkBadBlock (VNandInfo* vNand,int blockid );

int  vNand_Init ( VNandManager** context );
void vNand_Deinit ( VNandManager** context );

int vNand_UpdateErrorPartition(VNandManager* vm, PPartition *pt);

#endif
