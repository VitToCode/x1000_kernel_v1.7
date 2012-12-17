#include "bufflistmanager.h"
#include "os/NandAlloc.h"
#include "os/clib.h"
#include "context.h"
#include "nanddebug.h" 

#define UNITLEN    16
#define UNITDIM(x) (((x) + UNITLEN - 1 ) / UNITLEN)

/**
 *	BuffListManager_BuffList_Init  -  Initialize operation
 *	
*/
int BuffListManager_BuffList_Init (void)
{
	BuffListManager *blm;
	
	blm = (BuffListManager *)Nand_VirtualAlloc(sizeof(BuffListManager));
	if (!blm) {
		ndprint(1, "ERROR: fun %s line %d\n", __FUNCTION__, __LINE__);
		return 0;
	}
	
	blm->mem = (ZoneMemory *)ZoneMemory_Init(UNITLEN);
	if(!(blm->mem)) {
		ndprint(1, "ERROR: fun %s line %d\n", __FUNCTION__, __LINE__);
		return 0;
	}
	
	InitNandMutex(&blm->mutex);

	return (int)blm;
}

/**
 *	BuffListManager_BuffList_DeInit - Deinit operation
 *
 *	@handle: return value of BuffListManager_BuffList_Init
*/
void BuffListManager_BuffList_DeInit (int handle)
{
	BuffListManager *blm = (BuffListManager *)handle;
	
	ZoneMemory_DeInit((int)(blm->mem));
	
	DeinitNandMutex(&blm->mutex);

	Nand_VirtualFree(blm);
}

/**
 *	BuffListManager_getTopNode  -  Get a top node of a list
 *
 *	@handle: return value of BuffListManager_BuffList_Init
 *	@size: size of one node
*/
void *BuffListManager_getTopNode (int handle,int size)
{
	void *node;
	BuffListManager *blm = (BuffListManager *)handle;

	NandMutex_Lock(&blm->mutex);

	node = ZoneMemory_NewUnits((int)(blm->mem),UNITDIM(size));
	((struct singlelist *)node)->next = NULL;
	
	NandMutex_Unlock(&blm->mutex);

	return node;
}

/**
 *	get_tailnode -Get the last node of a list
 *	
 *	@head: top node of singlelist
*/
static struct singlelist *get_tailnode(struct singlelist *head)
{
	struct singlelist *pos;

	if (head->next == NULL)
		return head;

	for (pos = head; pos->next != NULL; pos = pos->next);

	return pos;
}

/**
 *	BuffListManager_getNextNode  -  Get a next node of list p
 *
 *	@handle: return value of BuffListManager_BuffList_Init
 *	@p: top node fo list
 *	@size: size of one node
*/
void *BuffListManager_getNextNode (int handle, void *p,int size)
{
	void *node;
	struct singlelist *pos;
	BuffListManager *blm = (BuffListManager *)handle;

	if (!p) {
		ndprint(1, "NULL error func %s line %d\n", __FUNCTION__, __LINE__);
		return NULL;
	}

	NandMutex_Lock(&blm->mutex);

	node = ZoneMemory_NewUnits((int)(blm->mem),UNITDIM(size));
	pos = get_tailnode((struct singlelist *)p);
	pos->next = (struct singlelist *)node;
	((struct singlelist *)node)->next = NULL;

	NandMutex_Unlock(&blm->mutex);

	return node;
}

/**
 *	BuffListManager_freeList  -  Free a node p of list top
 *
 *	@handle: return value of BuffListManager_BuffList_Init
 *	@top: top node of list
 *	@p: the node want to delete
 *	@size: size of one node
*/
void BuffListManager_freeList (int handle, void **top, void *p,int size)
{
	struct singlelist *pos;
	BuffListManager *blm = (BuffListManager *)handle;

	if (top == NULL || *top == NULL || p == NULL) {
		ndprint(1, "NULL error func %s line %d\n", __FUNCTION__, __LINE__);
		return;
	}

	NandMutex_Lock(&blm->mutex);

	if (*top == p)
		*top = ((struct singlelist *)p)->next;
	else {
		singlelist_for_each(pos,(struct singlelist *)(*top)) {
			if (pos->next == (struct singlelist *)p)
				break;
		}
		pos->next = ((struct singlelist *)p)->next;
	}

	ZoneMemory_DeleteUnits((int)(blm->mem),p,UNITDIM(size));

	NandMutex_Unlock(&blm->mutex);
}

/**
 *	BuffListManager_freeAllList  -  Free a list
 *
 *	@handle: return value of BuffListManager_BuffList_Init
 *	@top: top node of list
 *	@size: size of one node
*/
void BuffListManager_freeAllList (int handle, void **top,int size)
{
	if (top == NULL || *top == NULL) {
		ndprint(1, "NULL error func %s line %d\n", __FUNCTION__, __LINE__);
		return;
	}

	while (*top != NULL)
		BuffListManager_freeList(handle, top, *top,size);
}

/**
 *	BuffListManager_mergerList  -  Merger two lists
 *
 *	@handle: return value of BuffListManager_BuffList_Init
 *	@prev: previous list need to merger
 *	@next: next list need to merger
 *
 *	Add list next at the back of list prev.
*/
void BuffListManager_mergerList (int handle, void *prev, void *next)
{
	struct singlelist *pos;
	BuffListManager *blm = (BuffListManager *)handle;

	NandMutex_Lock(&blm->mutex);

	pos = get_tailnode((struct singlelist *)prev);
	singlelist_add_tail(pos, (struct singlelist *)next);

	NandMutex_Unlock(&blm->mutex);
}

