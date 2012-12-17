#ifndef __NANDZONEINFO_H__
#define __NANDZONEINFO_H__

#include "zoneinfo.h"

typedef struct _NandZoneInfo NandZoneInfo;
struct _NandZoneInfo {
	ZoneInfo preZone;
	ZoneInfo localZone;
	ZoneInfo nextZone;
	unsigned int serialnumber;
    	unsigned short crc;
};

#endif
