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

#define SECTOR_SIZE 512

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
};

#define CONTEXT_VNAND(context) 	((Context *)context)->vnand
#endif
