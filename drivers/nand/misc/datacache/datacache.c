#include "clib.h"
#include "datacache.h"

NAND_CACHE nand_cache[CACHE_MAX_NUM];//  __attribute__ ((aligned (4)));

/**
 *	nand_cache_init - Initialize operation
 */
void nand_cache_init(void)
{
	int i;

	for (i = 0; i < CACHE_MAX_NUM; i++) {
		nand_cache[i].Cache_Id = i;
		nand_cache[i].Sector_Id = -1;
		nand_cache[i].Hit_Count = 0;
		nand_cache[i].Write_Flag = 0;
		memset(nand_cache[i].Data, 0, SECTOR_SIZE);
	}
}

/**
 *	is_cache_hit - whether cache hit or not
 *
 *	@sectorid: id for sector
 *	@cache_id: id for cache which need to fill when cache hit
 */
int is_cache_hit(unsigned int sectorid, unsigned short *cache_id)
{
	int i;

	for (i = 0; i < CACHE_MAX_NUM; i++) {
		if (sectorid == nand_cache[i].Sector_Id) {
			*cache_id = i;
			return 1;
		}
	}

	return 0;
}

/**
 *	is_sectorid_match - whether cache matched or not
 *
 *	@sectorid: id for sector
 *	@cache_id: id for cache which need to fill when cache matched
 */
static int is_sectorid_match(unsigned int sectorid, unsigned int *cacheid)
{
	int i;

	for (i = 0; i < CACHE_MAX_NUM; i++) {
		if (nand_cache[i].Write_Flag && nand_cache[i].Sector_Id == sectorid) {
			*cacheid = i;
			return 1;
		}
	}

	return 0;
}

/**
 *	get_minhitcount - find the cache which hitcount is minimum
 */
static unsigned short get_minhitcount(void)
{
	int i;
	unsigned short cache_id = 0;

	for (i = 1; i < CACHE_MAX_NUM; i++) {
		if (nand_cache[cache_id].Hit_Count > nand_cache[i].Hit_Count)
			cache_id = i;
	}

	return cache_id;
}

/**
 *	get_gitcount - find the cache which hitcount > given, and
 *	mininum among all found
 *
 *	@cache_id: id for cache
 */
static unsigned short get_hitcount(unsigned short cache_id)
{
	int i;
	unsigned short cacheid = -1;
	int sub_value = 0x7fffffff;
	int sub_value1;
	unsigned short hit_count = nand_cache[cache_id].Hit_Count;

	for (i = 0; i < CACHE_MAX_NUM; i++) {
		sub_value1 = nand_cache[i].Hit_Count - hit_count;

		if (sub_value1 > 0 && sub_value1 < sub_value) {
			sub_value = sub_value1;
			cacheid = i;
		}
	}

	return cacheid;
}

/**
 *	get_merger_count - get count how mang sectors' data continuous
 *
 *	@cache_id: id for cache
 */
static int get_merger_count(unsigned short cache_id)
{
	int count = 0;
	unsigned int cacheid;
	unsigned int sectorid1, sectorid2;

	sectorid1 = nand_cache[cache_id].Sector_Id ;
	sectorid2 = nand_cache[cache_id].Sector_Id;

	while(is_sectorid_match(++sectorid1, &cacheid))
		count++;

	while(is_sectorid_match(sectorid2--, &cacheid))
		count++;

	return count;
}

/**
 *	is_all_cache_write - whether all cache is used or not
 */
static int is_all_cache_write(void)
{
	int i;

	for (i = 0; i < CACHE_MAX_NUM; i++) {
		if (nand_cache[i].Sector_Id == -1)
			return 0;
	}

	return 1;
}

/**
 *	get_idle_cache - get a unused cache
 */
static unsigned short get_idle_cache(void)
{
	int i;

	for (i = 0; i <CACHE_MAX_NUM; i++) {
		if (nand_cache[i].Hit_Count == 0)
			break;
	}

	return i;
}

/*
 *	invalid_cache - invalid cache
 *
 *	@cache_id: id for cache
 */
static void invalid_cache(unsigned short cache_id)
{
	nand_cache[cache_id].Sector_Id = -1;
	nand_cache[cache_id].Hit_Count = 0;
	nand_cache[cache_id].Write_Flag = 0;
	memset(nand_cache[cache_id].Data, 0xff, SECTOR_SIZE);
}

/**
 *	write_data_back - write data to nand from cache
 *
 *	@cache_id: id for cache
 */
static void write_data_back(unsigned short cache_id)
{
	unsigned int cacheid;
	unsigned int sectorid1, sectorid2;

	sectorid1 = nand_cache[cache_id].Sector_Id ;
	sectorid2 = nand_cache[cache_id].Sector_Id;

	while(is_sectorid_match(++sectorid1, &cacheid)) {
		//nand_multi_write(cacheid);
		invalid_cache(cacheid);
	}

	while(is_sectorid_match(sectorid2--, &cacheid)) {
		//nand_multi_write(cacheid);
		invalid_cache(cacheid);
	}
}

/**
 *	write_all_data_back - write data to nand from all cache
 */
void write_all_data_back(void)
{
	int i;

	for (i = 0; i < CACHE_MAX_NUM; i++) {
		if (nand_cache[i].Write_Flag) {
			//nand_multi_write(cache_id);
			invalid_cache(i);
		}
	}
}

/**
 *	apply_cache - apply one cache
 *	
 *	when cache was replaced you should wirte the data 
 *	back to nand if the data is valid.
 */
unsigned short apply_cache(void)
{
	int count;
	int flag = 0;
	unsigned short min_cacheid;
	unsigned short cache_id = -1;

	if (!is_all_cache_write())
		return get_idle_cache();
	
	while (1) {
		if (cache_id == (unsigned short)(-1)) {
			cache_id = get_minhitcount();
			min_cacheid = cache_id;
		}
		else
			cache_id = get_hitcount(cache_id);

		if (cache_id == (unsigned short)(-1))
			break;

		count = get_merger_count(cache_id);

		if (count >= MERGER_SECTOR_COUNT) {		
			flag = 1;
			break;
		}
	}

	if (flag)
		write_data_back(cache_id);
	else {
		cache_id = min_cacheid;
		nand_cache[cache_id].Hit_Count = 0;
		 if(nand_cache[cache_id].Write_Flag)
			write_data_back(cache_id);
	}

	return cache_id;
}

/**
 *	copy_data_to_cache - write data to cache
 *	
 *	@cache_id: id for cache
 *	@sectorid: id for sector
 *	@buf: data which to write
 *	@write_flag: indicate operation is write to nand or read from nand, 1 for write, 0 for read
 */
void copy_data_to_cache(unsigned short cache_id, unsigned int sectorid, unsigned char *buf, int write_flag)
{
	memcpy(nand_cache[cache_id].Data, buf, SECTOR_SIZE);
	nand_cache[cache_id].Sector_Id = sectorid;
	nand_cache[cache_id].Hit_Count++;

	if(write_flag)
		nand_cache[cache_id].Write_Flag = 1;
}

/**
 *	get_data_from_cache - read data from cache
 *	
 *	@cache_id: id for cache
 *	@buf: save the data which read from cache
 */
void get_data_from_cache(unsigned short cache_id, unsigned char *buf)
{
	memcpy(buf, nand_cache[cache_id].Data, SECTOR_SIZE);
	nand_cache[cache_id].Hit_Count++;
}
