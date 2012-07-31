#ifndef _NANDDEBUG_H_
#define _NANDDEBUG_H_

#define DEBUG 1
#define VNAND_INFO  1
#define VNAND_DEBUG 2
#define VNAND_ERROR 3

#define SIGBLOCK_INFO  1
#define SIGBLOCK_DEBUG 2
#define SIGBLOCK_ERROR 3

#define L2PCONVERT_INFO    1
#define L2PCONVERT_DEBUG   2
#define L2PCONVERT_ERROR   3

#define CACHEDATA_INFO  1
#define CACHEDATA_DEBUG 2
#define CACHEDATA_ERROR 3

#define CACHEMANAGER_INFO  1
#define CACHEMANAGER_DEBUG 2
#define CACHEMANAGER_ERROR 3

#define CACHELIST_INFO  1
#define CACHELIST_DEBUG 2
#define CACHELIST_ERROR 3

#define ZONEMANAGER_INFO  1
#define ZONEMANAGER_DEBUG 2
#define ZONEMANAGER_ERROR 3

#define HASH_INFO  1
#define HASH_DEBUG 2
#define HASH_ERROR 3

#define HASHNODE_INFO  1
#define HASHNODE_DEBUG 2
#define HASHNODE_ERROR 3

#define ZONE_INFO  1
#define ZONE_DEBUG 2
#define ZONE_ERROR 3

#define TASKMANAGER_INFO  1
#define TASKMANAGER_DEBUG 2
#define TASKMANAGER_ERROR 3

#define PARTITION_INFO  1
#define PARTITION_DEBUG 2
#define PARTITION_ERROR 3

#ifndef  LINUX_KERNEL
#define ndprint(level,...) printf(__VA_ARGS__);
#else
#define ndprint(level,...) printk(__VA_ARGS__);
#endif

#endif /* _NANDDEBUG_H_ */
