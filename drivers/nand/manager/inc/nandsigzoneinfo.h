#ifndef __NandSigZONEINFO_H__
#define __NandSigZONEINFO_H__

typedef struct _NandSigZoneInfo NandSigZoneInfo;
struct _NandSigZoneInfo {
	unsigned short ZoneID;
    	unsigned short badblock;
	unsigned int lifetime;
};

#endif
