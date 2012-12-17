#ifndef __CACHEINFO_H__
#define __CACHEINFO_H__

#define CACHEINFO(OBJ) ((CacheInfo*)OBJ)

#ifndef String
#define String char*
#endif


typedef struct _CacheInfo CacheInfo;

struct _CacheInfo {
  /** Attributes **/
  /*public*/
    int L1Len;
    int L2Len;
    int L3Len;
    int iscompress;
    int blockperzone;
  /** Associations **/
/** Operations **/
};

/** Operations **/
#endif
