#ifndef __NandSigZONEINFO_H__
#define __NandSigZONEINFO_H__
#include "zoneinfo.h"
typedef struct _NandSigZoneInfo NandSigZoneInfo;
struct _NandSigZoneInfo {
	unsigned short ZoneID;
   	unsigned short badblock;
	unsigned int lifetime;
	ZoneInfo prezoneinfo;
};

#endif
