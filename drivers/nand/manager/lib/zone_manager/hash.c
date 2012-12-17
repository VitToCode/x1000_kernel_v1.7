#include "os/clib.h"
#include "hash.h"
#include "os/NandAlloc.h"
#include "nanddebug.h"

/**
 *	Hash_init  -  Initialize operation
 *
 *	@hash: operate object
 *	@top: the base address of sigzoneinfo[]
 *	@zoneid: object which hashtable need to operation
 *	@count: count of zoneid[]
 */
int Hash_init ( Hash **hash, SigZoneInfo *top, unsigned short *zoneid, int count )
{
	int i;

	*hash = (Hash *)Nand_VirtualAlloc(sizeof(Hash));
	if (!(*hash)) {
		ndprint(HASH_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		return -1;
	}
	
	(*hash)->top = (HashNode **)Nand_VirtualAlloc(sizeof(HashNode *) * HASHNODE_COUNT);
	if (!((*hash)->top)) {
		ndprint(HASH_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		Nand_VirtualFree(*hash);
		return -1;
	}
	
	(*hash)->maxlifetime = 0;
	(*hash)->minlifetime = -1;
	(*hash)->usezone_count = 0;	

	(*hash)->zoneID_count = count;

	for (i = 0; i < HASHNODE_COUNT; i++) {
		if (HashNode_init(&((*hash)->top[i]), top, zoneid, count) == -1)
			goto ERROR;
	}

	return 0;
ERROR:
	ndprint(HASH_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
	Nand_VirtualFree((*hash)->top);
	Nand_VirtualFree(*hash);
	return -1;
}

/**
 *	Hash_deinit  -  Deinit operation
 *
 *	@hash: operate object
 */
void Hash_deinit (Hash **hash)
{
	int i;

	for (i = 0; i < HASHNODE_COUNT; i++)
		HashNode_deinit(&((*hash)->top[i]));
	
	Nand_VirtualFree((*hash)->top);
	Nand_VirtualFree(*hash);
}

/**
 *	Hash_Insert  -  Insert operation
 *
 *	@hash: operate object
 *	@szi: the object wanted to insert
 *
 *	Insert a offset of sinzoneinfo into hashtable. 
 *	hash arithmetic formula is : lifetime % 100 / (100 / HASHNODE_COUNT).
 */
int Hash_Insert ( Hash *hash, SigZoneInfo *szi )
{
	int ret, pos;

	if (hash->usezone_count >= hash->zoneID_count) {
		ndprint(HASH_ERROR,"zoneID[] was full fun %s line %d\n", __FUNCTION__, __LINE__);
		return -1;
	}

	pos = szi->lifetime % 100 / (100 / HASHNODE_COUNT);
	ret = HashNode_insert(hash->top[pos], szi);
	if (ret != 0)
		return -1;

	hash->usezone_count++;

	/* update minlifetime and maxlifetime */
	if (szi->lifetime != -1 && szi->lifetime > hash->maxlifetime)
		hash->maxlifetime = szi->lifetime;
	if (szi->lifetime < hash->minlifetime)
		hash->minlifetime = szi->lifetime;
	
	return 0;
}

/**
 *	Hash_delete  -  Delete operation
 *
 *	@hash: operate object
 *	@szi: the object wanted to delete
 *
 *	Delete a offset of sinzoneinfo of hashtable. 
 */
int Hash_delete ( Hash *hash, SigZoneInfo *szi )
{
	int i, ret, pos;

	if (hash->usezone_count == 0) {
		ndprint(HASH_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		return -1;
	}
	else if (hash->usezone_count == 1) {
		hash->minlifetime = -1;
		hash->maxlifetime = 0;
	}

	pos = szi->lifetime % 100 / (100 / HASHNODE_COUNT);
	ret = HashNode_delete(hash->top[pos], szi);
	if (ret == -1) {
		ndprint(HASH_ERROR,"ERROR: func %s line %d\n", __FUNCTION__, __LINE__);
		return -1;
	}

	/* when maxlifetime or minlifetime was deleted, you should find a new one */
	if (szi->lifetime == hash->maxlifetime) {
		hash->maxlifetime = 0;
		for (i = 0; i < HASHNODE_COUNT; i++) {
			if (hash->top[i]->maxlifetime != -1 && hash->top[i]->maxlifetime > hash->maxlifetime)
				hash->maxlifetime = hash->top[i]->maxlifetime;
		}
	}
	else if (szi->lifetime == hash->minlifetime) {
		hash->minlifetime = 0xffffffff;
		for (i = 0; i < HASHNODE_COUNT; i++) {
			if (hash->top[i]->minlifetime < hash->minlifetime)
				hash->minlifetime = hash->top[i]->minlifetime;
		}
	}
	
	hash->usezone_count--;

	return 0;
}

/**
 *	Hash_FindFirstLessLifeTime  -  FindFirstLessLifeTime
 *
 *	@hash: operate object
 *	@lifetime: the condition of find
 *	@szi: need to fill when found the node which less then lifetime
 *
 *	Find the fisrt lifetime which is less than the given lifetime among hashtable,
 *	then give the sigzoneinfo which is correspond with the found lifetime to caller.
 *	The return value is the found lifetime's location of hashnode which should be
 *	called at function of Hash_FindNextLessLifeTime.
*/
int Hash_FindFirstLessLifeTime ( Hash *hash, unsigned int lifetime, SigZoneInfo **szi )
{
	int offset;
	int flag = 0;
	int count = 0;

	hash->find_lifetime = lifetime;
	
	if (lifetime < hash->minlifetime) {
		ndprint(HASH_ERROR,"ERROR: func %s line %d lifetime=%d hash->minilifetime = %d\n"
                                , __FUNCTION__, __LINE__,lifetime,hash->minlifetime);
		*szi = NULL;
		return -1;
	}

	/* find the last hashnode which minlifetime is less than given lifetime,
	 *  then get the fisrt node which lifetime is less than given lifetime.
	 */
	hash->prev_pos = lifetime % 100 / (100 / HASHNODE_COUNT);
	while(1) {
		if (count == HASHNODE_COUNT)
			break;

		if (HashNode_getcount(hash->top[hash->prev_pos])) {
			if (HashNode_getminlifetime(hash->top[hash->prev_pos]) >= lifetime) {
				if (flag) {
					flag = 0;
					break;
				}
			}
			else
				flag = 1;
		}
		
		hash->prev_pos = (hash->prev_pos + HASHNODE_COUNT - 1) % HASHNODE_COUNT;
		count++;
	}

	while (1) {
		hash->prev_pos = (hash->prev_pos + 1) % HASHNODE_COUNT;
		if (HashNode_getcount(hash->top[hash->prev_pos]))
			break;
	}
	
	hash->first_pos = hash->prev_pos;

	offset = HashNode_FindFirstLessLifeTime(hash->top[hash->prev_pos], lifetime, szi);

	return offset;
}

/**
 *	Hash_FindNextLessLifeTime  -  FindNextLessLifeTime
 *
 *	@hash: operate object
 *	@prev: the previous location of hashnode which is saved by latest find operation
 *	@szi: need to fill when found the node which less then lifetime
 *
 *	Find the next lifetime which is less than the given lifetime among hashtable,
 *	then give the sigzoneinfo which is correspond with the found lifetime to caller.
 *	The return value is the found lifetime's location of hashnode which should be
 *	called at next time of use HashNode_FindNextLessLifeTime. If return value 
 *	was -1, it indicate that all less than given lifetime were found out.
*/
int Hash_FindNextLessLifeTime ( Hash *hash, int prev, SigZoneInfo **szi )
{
	int offset;
	int pos = hash->prev_pos;

	offset = HashNode_FindNextLessLifeTime(hash->top[pos], prev, szi);
	if (offset != -1)
		return offset;

	/* find next lifetime in the same hashnode first, when found out, 
	 * then find in next hashnode till all hashnode were found.
	 */
	while (1) {
		pos = (pos + 1) % HASHNODE_COUNT;
		if (pos == hash->first_pos)
			return -1;

		if (HashNode_getminlifetime(hash->top[pos]) < hash->find_lifetime)
			break;
	}
	hash->prev_pos = pos;

	offset = HashNode_FindFirstLessLifeTime(hash->top[pos], hash->find_lifetime, szi);

	return offset;
}

/**
 *	Hash_getminlifetime  -  Get the minimum lifetime among the hashtable
 *
 *	@hash: operate object
*/
unsigned int Hash_getminlifetime (Hash *hash)
{
	return hash->minlifetime;
}

/**
 *	Hash_getmaxlifetime  -  Get the maximum lifetime among the hashtable
 *
 *	@hash: operate object
*/
unsigned int Hash_getmaxlifetime (Hash *hash)
{
	return hash->maxlifetime;
}

/**
 *	Hash_getcount  -  Get the count of node among the hashtable
 *
 *	@hash: operate object
*/
unsigned int Hash_getcount (Hash *hash)
{
	return hash->usezone_count;
}

