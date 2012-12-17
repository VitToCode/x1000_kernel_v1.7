#ifndef _PAGEINFODEBUG_H_
#define _PAGEINFODEBUG_H_
#include "pageinfo.h"

struct PageInfoDebug
{
	unsigned int *l4cache;
	unsigned int l4len;
	unsigned int *l3cache;
	unsigned int l3len;
	unsigned int *l2cache;
	unsigned int l2len;
	unsigned int *l1cache;
	unsigned int l1len;

	unsigned int L1UnitLen;	//how mang sectors one pageid indicate
	unsigned int L2UnitLen;
	unsigned int L3UnitLen;
	unsigned int L4UnitLen;

	unsigned int pageid;
	unsigned int *diffdata;

	unsigned int *sectorid;
};
struct PageInfoDebug *Init_L2p_Debug(int context);
void Deinit_L2p_Debug(struct PageInfoDebug *pdebug);
void L2p_Debug_SaveCacheData(struct PageInfoDebug *pdebug,PageInfo *pi);
void L2p_Debug_SetstartPageid(struct PageInfoDebug *pdebug,int pageid);
void L2p_Debug_CheckData(struct PageInfoDebug *pdebug,PageInfo *pi,int count);

#endif /* _PAGEINFODEBUG_H_ */
