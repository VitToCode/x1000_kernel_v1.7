#ifndef __CONTEXT_H__
#define __CONTEXT_H__

#include "vnandinfo.h"
#include "cacheinfo.h"
#include "sigzoneinfo.h"
#include "bufflistmanager.h"
#include "l1info.h"
#include "zonemanager.h"
#include "cachemanager.h"
#include "recycle.h"
#include "junkzone.h"
#include "timerdebug.h"

#define SECTOR_SIZE 512
#define INTERNAL_TIME  2*1000000000

typedef struct _Context Context;

struct _Context {
	ZoneManager *zonep;
	BuffListManager *blm;
	CacheManager *cachemanager;
	Recycle *rep;
	VNandInfo vnand;
	CacheInfo *cacheinfo;
	SigZoneInfo *top;
	L1Info *l1info;
	int thandle;
	int junkzone; //l2p recycle
	long long t_startrecycle;
#ifdef STATISTICS_DEBUG
	TimeByte *timebyte;
#endif
};

#define CONTEXT_VNAND(context) 	((Context *)context)->vnand
#endif
