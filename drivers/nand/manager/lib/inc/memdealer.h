#ifndef _MEMDEALER_H_
#define _MEMDEALER_H_
#include <bilist.h>
struct MemoryDealer
{
	void *heap;
	unsigned int heapsize;
	int zmid; 
	struct bilist_head top;
};

int InitContinueMemory(void *h,int size);
void DeinitContinueMemory(int mid);

void *Allocate(int mid,int size);
void *PageAllocate(int mid,int size);
void Deallocate(int mid,void *v);

#endif /* _MEMDEALER_H_ */
