#ifndef _NANDDEBUG_H_
#define _NANDDEBUG_H_

#define DEBUG 1
#define VNAND_INFO  1
#define VNAND_DEBUG 2
#define VNAND_ERROR 3

#define ZONEMANAGER_INFO  1
#define ZONEMANAGER_DEBUG 2

#define ZONE_INFO  1
#define ZONE_DEBUG 2
#define ZONE_ERROR 3

#define TASKMANAGER_INFO  1
#define TASKMANAGER_DEBUG 2

#define ndprint(level,...) printk(__VA_ARGS__);


#endif /* _NANDDEBUG_H_ */
