#ifndef __VNAND_H__
#define __VNAND_H__

#include "vnandinfo.h"
#include "nandinterface.h"
#include "hashnode.h"
#include "hash.h"
#include "l1info.h"
#include "sigzoneinfo.h"

#define L4INFOLEN 1136

#define VNANDCACHESIZE (128 * L4INFOLEN)
#define BADBLOCKINFOSIZE 1

struct vnand_operater{
	NandInterface *operator;
	unsigned char *vNand_buf;
	NandMutex mutex;
	void (*start_nand)(int);
	int context;
};
#define CHECK_OPERATOR(ops)											\
	do{																\
		if(v_nand_ops.operator && !v_nand_ops.operator->i##ops){	\
			ndprint(VNAND_INFO,"i%s isn't registed\n",#ops);		\
			return -1;												\
		}															\
	}while(0)

#define VN_OPERATOR(ops,...)								\
	({														\
		int __ret;											\
		CHECK_OPERATOR(ops);								\
		__ret = v_nand_ops.operator->i##ops (__VA_ARGS__);	\
		__ret;												\
	})


int vNand_PageRead (VNandInfo* vNand,int pageid, int offsetbyte, int bytecount, void * data );
int vNand_PageWrite (VNandInfo* vNand,int pageid, int offsetbyte, int bytecount, void* data );
int __vNand_MultiPageRead (VNandInfo* vNand,PageList* pl );
int __vNand_MultiPageWrite (VNandInfo* vNand,PageList* pl );
int __vNand_CopyData (VNandInfo* vNand,PageList* rpl, PageList* wpl );
int __vNand_MultiBlockErase (VNandInfo* vNand,BlockList* pl );
int __vNand_IsBadBlock (VNandInfo* vNand,int blockid );
int __vNand_MarkBadBlock (VNandInfo* vNand,int blockid);

int  __vNand_Init ( VNandManager** context );
void __vNand_Deinit ( VNandManager** context );
void Register_StartNand(void *start,int context);

int vNand_register_nanddriver (int context,NandInterface* interface);

void vNand_Lock(void);
void vNand_unLock(void);

#endif
