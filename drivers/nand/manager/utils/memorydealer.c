
/*
  Simple Best Fit Allocator
  from android memory dealer
*/
#include <bilist.h>
#include <NandSemaphore.h>
#include <zonememory.h>
#include "string.h"
#include <memdealer.h>
#include "nanddebug.h"

struct chunk{
	unsigned int  start;
	unsigned int size : 28;
	unsigned int free : 4;
	struct list_head head;
};

typedef struct chunk chunk_t;

#define KMEMALIGN (4)
#define PAGESIZE  (4*1024)
#define PAGE_ALIGNED 1
#define NO_MEMORY   -1

#define UNITLEN    sizeof(chunk_t)
#define UNITDIM(x) (((x) + UNITLEN - 1 ) / UNITLEN)

/*
static unsigned int heapsize=0;
static NandMutex mutex;
static void *heap = NULL;
struct list_head top;
static int zmid = 0;


static NandMutex mutex;
	
*/


static int alloc(struct MemoryDealer *dealer,int size, unsigned int flags){
	chunk_t* free_chunk,*cur;
	struct list_head *pos;
	int extra;

	if (size == 0) {
        return 0;
    }
	size = (size + KMEMALIGN - 1) / KMEMALIGN;
	free_chunk = 0;

	list_for_each(pos,&dealer->top){
		cur = list_entry(pos,chunk_t,head);
        extra = 0;
        if (flags & PAGE_ALIGNED)
            extra = ( -cur->start & ((PAGESIZE/KMEMALIGN)-1) );
		// best fit
        if (cur->free && (cur->size >= (size+extra))) {
            if ((!free_chunk) || (cur->size < free_chunk->size)) {
                free_chunk = cur;
            }
            if (cur->size == size) {
				free_chunk = cur;   //android bug
                break;
            }
        }     
	}
	if (free_chunk) {
        int free_size = free_chunk->size;
        free_chunk->free = 0;
        free_chunk->size = size;
        if (free_size > size) {
            int extra = 0;
			int tail_free;
			chunk_t* split;
            if (flags & PAGE_ALIGNED)
                extra = ( -free_chunk->start & ((PAGESIZE/KMEMALIGN)-1) ) ;

            if (extra) {
                split = (chunk_t *)ZoneMemory_NewUnit(dealer->zmid);
				if(split == NULL){
					free_chunk->size = free_size;
					return NO_MEMORY;
				}else{
					split->start = free_chunk->start;
					split->size = extra;
					split->free = 1;
					list_add_tail(&split->head,&free_chunk->head);
					free_chunk->start += extra;
				}
            }
            tail_free = free_size - (size + extra);
            if (tail_free > 0) {
                split = (chunk_t *)ZoneMemory_NewUnit(dealer->zmid);				
				if(split == NULL){
					free_chunk->size = free_size;
					return NO_MEMORY;
				}else{
					
					split->start = free_chunk->start + free_chunk->size;
					split->size = tail_free;
					split->free = 1;
					list_add(&split->head,&free_chunk->head); //for s_alloc
				}
		    }
        }
        return (free_chunk->start)*KMEMALIGN;
    }
    return NO_MEMORY;
}

static chunk_t* dealloc(struct MemoryDealer *dealer,size_t start)
{
	chunk_t* cur;
	struct list_head *pos;
	int flag = 0;
	start = start / KMEMALIGN;
	list_for_each(pos,&dealer->top){
        cur = list_entry(pos,chunk_t,head);
		if (cur->start == start) {
            // merge freed blocks together
			chunk_t* p;
            chunk_t* n;
			chunk_t* freed = cur;
			flag = 1;
			cur->free = 1;				
			p = list_entry(pos->prev,chunk_t,head);
			n = list_entry(pos->next,chunk_t,head);
			
			if (!list_is_last(&p->head,&dealer->top) && (p->free || !cur->size)) {
				freed = p;
				p->size += cur->size;
				list_del(pos);
				ZoneMemory_DeleteUnit(dealer->zmid,cur);
				cur = p;
			}
			
			if ( (n->free || !cur->size)) {
				freed = cur;
				cur->size += n->size;
				list_del(&n->head);
				ZoneMemory_DeleteUnit(dealer->zmid,n);
				ndprint(1,"nnnnnnnnnnnnnn\n");
			}
			return freed;
        }
	}
	if(flag)
	{
		ndprint(1,"no free start = 0x%08x\n",start * KMEMALIGN);
	}
    return 0;
}
int InitContinueMemory(void *h,int size){
	
	struct MemoryDealer *dealer;
	chunk_t *p;
	int zmid;
	zmid = ZoneMemory_Init(UNITLEN);
	dealer = (struct MemoryDealer *)ZoneMemory_NewUnits(zmid,UNITDIM(sizeof(struct MemoryDealer)));
	dealer->zmid = zmid;
	INIT_LIST_HEAD(&dealer->top);
	dealer->heap = h;
	dealer->heapsize = size;
	p = (chunk_t *)ZoneMemory_NewUnit(zmid);
	p->free = 1;
	p->start = 0;
	p->size = size;
	list_add(&p->head,&dealer->top);

	return (int)dealer;
}

void DeinitContinueMemory(int mid){
	struct MemoryDealer *dealer = (struct MemoryDealer *)mid;
	ZoneMemory_DeInit(dealer->zmid);
}

void *Allocate(int mid,int size){
	struct MemoryDealer *dealer = (struct MemoryDealer *)mid;
	unsigned char *p = dealer->heap;
	int offset;
	offset = alloc(dealer,size, 0);	
	if(offset == NO_MEMORY)
		return 0;
	return p + offset;
}
void *PageAllocate(int mid,int size){
	struct MemoryDealer *dealer = (struct MemoryDealer *)mid;
	unsigned char *p = dealer->heap;
	int offset;
	offset = alloc(dealer,size, 1);	
	if(offset == NO_MEMORY)
		return 0;
	return p + offset;
}
void Deallocate(int mid,void *v){
	struct MemoryDealer *dealer = (struct MemoryDealer *)mid;
	unsigned char *p = dealer->heap;
	int offset;

	offset = (unsigned int)v - (unsigned int)p;
   	if((offset < 0) && (offset < dealer->heapsize))
		return;
	dealloc(dealer,offset);
}

void dumplist(int mid){
	struct MemoryDealer *dealer = (struct MemoryDealer *)mid;
	struct list_head *pos;
	chunk_t* cur;
	ndprint(1,"=================================\n");
	list_for_each(pos,&dealer->top){
		cur = list_entry(pos,chunk_t,head);
		ndprint(1,"pos = %p,pos->prev = %p,pos->next = %p cur->start = %d cur->size = %d,cur->free = %d\n",pos,pos->prev,pos->next,cur->start,cur->size,cur->free);
	}
}
