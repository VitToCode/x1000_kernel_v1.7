#ifndef __PAGEINFO_H__
#define __PAGEINFO_H__

typedef struct _PageInfo PageInfo;

struct _PageInfo {
	unsigned int PageID;
	unsigned short L1Index;
	unsigned short L2InfoLen;
	unsigned char* L2Info;
	unsigned short L2Index;
	unsigned short L3InfoLen;
	unsigned char* L3Info;
	unsigned short L3Index;
	unsigned short L4InfoLen;
	unsigned char* L4Info;
	unsigned char* L1Info;
	int L1InfoLen;
	int context;
	unsigned short zoneID;
};

#endif
