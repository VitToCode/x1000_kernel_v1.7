#include "hashnode.h"
#include "NandAlloc.h"
#include "nanddebug.h"

/**
 *	find_location  -  find the index of zoneID when zoneID[index] = value 
 *
 *	@hashnode: operate object
 *	@value: the number which want to be found
*/
static unsigned short find_location(HashNode *hashnode, short value)
{
	unsigned short offset;

	if (hashnode->count == 0) {
		ndprint(HASHNODE_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		return -1;
	}

	offset = hashnode->head;
	while (1) {
		if (hashnode->zoneID[offset] == value)
			return offset;
		offset = hashnode->zoneID[offset];

		if (offset == (unsigned short)(-1))
			return (unsigned short)(-1);
	}
}

/**
 *	HashNode_init  -  Initialize operation
 *	
 *	@hashnode: the object need to initalize
 *	@top: the base address of sigzoneinfo[]
 *	@zoneid: object which hashtable need to operation
 *	@count: count of zoneid[]
*/
int HashNode_init ( HashNode **hashnode, SigZoneInfo *top, unsigned short *zoneid, int count)
{
	int i;

	for (i = 0; i < count; i++)
		zoneid[i] = -1;

	*hashnode = (HashNode *)Nand_VirtualAlloc(sizeof(HashNode));
	if (!(*hashnode)) {
		ndprint(HASHNODE_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		return -1;
	}

	(*hashnode)->zoneID = zoneid;
	(*hashnode)->zoneID_count = count;
	(*hashnode)->head = -1;
	(*hashnode)->tail = -1;
	(*hashnode)->count = 0;
	(*hashnode)->maxlifetime = 0;
	(*hashnode)->minlifetime = -1;
	(*hashnode)->base_szi = top;

	return 0;
}

/**
 *	HashNode_deinit  -  Deinit operation
 *
 *	@hashnode: operate object
*/
void HashNode_deinit ( HashNode **hashnode )
{
	unsigned short offset;

	if ((*hashnode)->count != 0 && (*hashnode)->count != 1) {	
		offset = (*hashnode)->tail;
		while (1) {
			offset = find_location((*hashnode), offset);
			(*hashnode)->zoneID[offset] = -1;

			if (offset == (*hashnode)->head)
				break;
		}
	}

	Nand_VirtualFree(*hashnode);
}

/**
 *	HashNode_insert  -  Insert operation
 *
 *	@hashnode: operate object
 *	@sigzoneinfo: the object wanted to insert
 *
 *	Insert a offset of sinzoneinfo at back of hashnode' tail. 
*/
int HashNode_insert ( HashNode *hashnode, SigZoneInfo *sigzoneinfo )
{
	unsigned short offset = sigzoneinfo - hashnode->base_szi;
	
	if (hashnode->count >= hashnode->zoneID_count) {
		ndprint(HASHNODE_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		return -1;
	}

	/* first insert */
	if (hashnode->head == (unsigned short)(-1)) {
		hashnode->head = offset;
		hashnode->tail = offset;
		hashnode->zoneID[offset] = -1;
	}
	else {
		hashnode->zoneID[hashnode->tail] = offset;
		hashnode->zoneID[offset] = -1;
		hashnode->tail = offset;
	}

	/* update minlifetime and maxlifetime */
	if (sigzoneinfo->lifetime != -1 && sigzoneinfo->lifetime > hashnode->maxlifetime)
		hashnode->maxlifetime = sigzoneinfo->lifetime;
	if (sigzoneinfo->lifetime < hashnode->minlifetime)
		hashnode->minlifetime = sigzoneinfo->lifetime;

	hashnode->count++;

	return 0;
}

/**
 *	HashNode_delete  -  Delete operation
 *
 *	@hashnode: operate object
 *	@sigzoneinfo: the object wanted to delete
 *
 *	Delete a offset of sinzoneinfo of hashnode. 
*/
int HashNode_delete ( HashNode *hashnode, SigZoneInfo *sigzoneinfo )
{
	unsigned short location;
	unsigned short offset;
	unsigned short temp;
	
	if (hashnode->count == 0) {
		ndprint(HASHNODE_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		return -1;
	}
	else if (hashnode->count == 1) {
		hashnode->head = -1;
		hashnode->tail = -1;
		hashnode->maxlifetime = 0;
		hashnode->minlifetime = -1;
		hashnode->count--;
		return 0;
	}

	offset = sigzoneinfo - hashnode->base_szi;
	if (offset == hashnode->head){//delete head
		temp = hashnode->head;
		hashnode->head = hashnode->zoneID[hashnode->head];
		hashnode->zoneID[temp] = -1;
	}
	else if (offset == hashnode->tail) {//delete tail
		location = find_location(hashnode, offset);
		hashnode->zoneID[offset] = -1;
		hashnode->zoneID[location] = -1;
		hashnode->tail = location;
	}
	else {//delete between head and tail
		location = find_location(hashnode, offset);
		if (location == (unsigned short)(-1)) {
			ndprint(HASHNODE_ERROR,"Can't find func %s line %d\n", __FUNCTION__, __LINE__);
			return -1;
		}
	
		hashnode->zoneID[location] = hashnode->zoneID[offset];
		hashnode->zoneID[offset] = -1;
	}

	/* when maxlifetime or minlifetime was deleted, you should find a new one */
	if (sigzoneinfo->lifetime == hashnode->maxlifetime) {
		hashnode->maxlifetime = 0;
		offset = hashnode->head;
		while (1) {
			if ((hashnode->base_szi + offset)->lifetime != -1 && 
				(hashnode->base_szi + offset)->lifetime > hashnode->maxlifetime)
				hashnode->maxlifetime = (hashnode->base_szi + offset)->lifetime;

			offset = hashnode->zoneID[offset];
			if (offset == (unsigned short)(-1))
				break;
		}
	}
	else if (sigzoneinfo->lifetime == hashnode->minlifetime) {
		hashnode->minlifetime = 0xffffffff;
		offset = hashnode->head;
		while (1) {
			if ((hashnode->base_szi + offset)->lifetime < hashnode->minlifetime)
				hashnode->minlifetime = (hashnode->base_szi + offset)->lifetime;

			offset = hashnode->zoneID[offset];
			if (offset == (unsigned short)(-1))
				break;
		}
	}

	hashnode->count--;

	return 0;
}

/**
 *	HashNode_getminlifetime  -  Get the minimum lifetime among the hashnode
 *
 *	@hashnode: operate object
*/
unsigned int HashNode_getminlifetime (HashNode *hashnode)
{
	return hashnode->minlifetime;
}

/**
 *	HashNode_getmaxlifetime  -  Get the maximum lifetime among the hashnode
 *
 *	@hashnode: opreate object
*/
unsigned int HashNode_getmaxlifetime (HashNode *hashnode)
{
	return hashnode->maxlifetime;
}

/**
 *	HashNode_getcount  -  Get the count of node among the hashnode
 *
 *	@hashnode: operate object
*/
unsigned int HashNode_getcount (HashNode *hashnode)
{
	return hashnode->count;
}

/**
 *	HashNode_FindFirstLessLifeTime  -  FindFirstLessLifeTime
 *
 *	@hashnode: operate object
 *	@lifetime: the condition of find
 *	@sigzoneinfo: need to fill when found the node which less then lifetime
 *
 *	Find the fisrt lifetime which is less than the given lifetime among hashnode,
 *	then give the sigzoneinfo which is correspond with the found lifetime to caller.
 *	The return value is the found lifetime's location of hashnode which should be
 *	called at function of HashNode_FindNextLessLifeTime.
*/
int HashNode_FindFirstLessLifeTime(HashNode *hashnode, unsigned int lifetime, SigZoneInfo **sigzoneinfo)
{
	int offset;
	
	hashnode->find_lifetime = lifetime;

	if (hashnode->minlifetime >= lifetime) {
		ndprint(HASHNODE_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		*sigzoneinfo = NULL;
		return -1;
	}
	
	/* find the fisrt node which lifetime is less than given lifetime. */
	offset = hashnode->head;
	while (1) {
		if (offset == (unsigned short)(-1))
			return -1;

		if ((hashnode->base_szi + offset)->lifetime < lifetime)
			break;

		offset = hashnode->zoneID[offset];
	}

	*sigzoneinfo = hashnode->base_szi + offset;

	return offset;
}

/**
 *	HashNode_FindNextLessLifeTime  -  FindNextLessLifeTime
 *
 *	@hashnode: operate object
 *	@prev: the previous location of hashnode which is saved by latest find operation
 *	@sigzoneinfo: need to fill when found the node which less then lifetime
 *
 *	Find the next lifetime which is less than the given lifetime among hashnode,
 *	then give the sigzoneinfo which is correspond with the found lifetime to caller.
 *	The return value is the found lifetime's location of hashnode which should be
 *	called at next time of use HashNode_FindNextLessLifeTime. If return value 
 *	was -1, it indicate that all less than given lifetime were found out.
*/
int HashNode_FindNextLessLifeTime(HashNode *hashnode, int prev, SigZoneInfo **sigzoneinfo)
{
	int offset;

	if (hashnode->minlifetime >= hashnode->find_lifetime) {
		ndprint(HASHNODE_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		*sigzoneinfo = NULL;
		return -1;
	}

	/* find the next node which lifetime is less than given lifetime. */
	offset = hashnode->zoneID[prev];
	while (1) {
		if (offset == (unsigned short)(-1))
			return -1;

		if ((hashnode->base_szi + offset)->lifetime < hashnode->find_lifetime)
			break;

		offset = hashnode->zoneID[offset];
	}

	*sigzoneinfo = hashnode->base_szi + offset;

	return offset;
}

/**
 *	HashNode_get  -  Give the minlifetime of hashnode to caller
 *
 *	@hashnode: operate object
*/
SigZoneInfo *HashNode_get (HashNode *hashnode)
{
	int offset;
	SigZoneInfo *szi = NULL;
	
	if (hashnode->count == 0)
		return NULL;

	offset = hashnode->head;
	while (1) {
		if ((hashnode->base_szi + offset)->lifetime == hashnode->minlifetime)
			break;

		offset = hashnode->zoneID[offset];
	}
	
	szi = hashnode->base_szi + offset;

	HashNode_delete(hashnode, szi);

	return szi;
}

