#ifndef __NANDPAGEINFO_H__
#define __NANDPAGEINFO_H__

typedef struct _NandPageInfo NandPageInfo;
struct _NandPageInfo {
	unsigned short NextPageInfo;
	unsigned short ZoneID;
	unsigned short len;
	unsigned char *L1Info;
	unsigned short L1Index;
	unsigned char *L2Info;
	unsigned short L2Index;
	unsigned char *L3Info;
	unsigned short L3Index;
	unsigned char* L4Info;
	unsigned short MagicID;
	unsigned short crc;
};

#endif
