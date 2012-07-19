#ifndef __DATACACHE_H__
#define __DATACACHE_H__

#define DEBUG_CACHE

#define SECTOR_SIZE 	512

#ifdef DEBUG_CACHE
#define CACHE_MAX_NUM 	10
#else
#define CACHE_MAX_NUM 	2 * 1024 * 1024 / SECTOR_SIZE  // 2M cache
#endif

#define MERGER_SECTOR_COUNT 4

typedef struct {
	unsigned short Cache_Id;
	unsigned short  Hit_Count;	
	unsigned int Sector_Id;
	unsigned char   Data[SECTOR_SIZE];
	unsigned char Write_Flag;  //用来标记是否为写操作
} NAND_CACHE;

void nand_cache_init(void);
int is_cache_hit(unsigned int sectorid, unsigned short *cache_id);
void write_all_data_back(void);
unsigned short apply_cache(void);
void copy_data_to_cache(unsigned short cache_id, unsigned int sectorid, unsigned char *buf, int write_flag);
void get_data_from_cache(unsigned short cache_id, unsigned char *buf);

#endif

