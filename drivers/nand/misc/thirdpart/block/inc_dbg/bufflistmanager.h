#ifndef __BUFFLISTMANAGER_H__
#define __BUFFLISTMANAGER_H__

#include "sectorlist.h"

inline int BuffListManager_BuffList_Init(void)
{
	return 0;
}

inline void BuffListManager_BuffList_DeInit(int handle)
{
}

inline void *BuffListManager_getTopNode(int handle,int size)
{
	SectorList *ptr;

	ptr = vzalloc(size);
	if (!ptr)
		printk("ERROR(nand block): alloc memory for ndisk->sl error!\n");

	return (void *)ptr;
}

inline void *BuffListManager_getNextNode(int handle, void *p,int size)
{
	SectorList *ptr = NULL;

	ptr = vzalloc(size);
	if (!ptr || !p)
		printk("ERROR(nand block): alloc memory for ndisk->sl error!\n");
	else
		((SectorList *)p)->head.next = &ptr->head;

	return (void *)ptr;
}

inline void BuffListManager_freeList(int handle, void **top, void *p,int size)
{
}

inline void BuffListManager_freeAllList(int handle, void **top,int size)
{
	SectorList *ptr;
	struct singlelist *plist;

	if (!top || !(*top)) {
		printk("ERROR: top is null\n");
		return;
	}

	ptr = (SectorList *)(*top);
	if (ptr)
		plist = &ptr->head;
	else
		return;

	while(plist) {
		ptr = singlelist_entry(plist, SectorList, head);
		//printk("free sectorlist %p\n", ptr);
		plist = ptr->head.next;
		vfree(ptr);
	}
}

inline void BuffListManager_mergerList(int handle, void *prev, void *next)
{
}

#endif
