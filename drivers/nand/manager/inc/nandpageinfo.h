#ifndef __NANDPAGEINFO_H__
#define __NANDPAGEINFO_H__
#include "ncrc16.h"
typedef struct _NandPageInfo NandPageInfo;
union TotalSector {
	unsigned char *L2Info;
	unsigned int sectors;
};
struct _NandPageInfo {
	unsigned short NextPageInfo;
	unsigned short ZoneID;
	unsigned short len;
	unsigned short L1Index;
	union TotalSector L2Info_Sector;
	//unsigned char *L2Info;  // used for record sectors written by fs

	unsigned int L2Index;
	unsigned char *L3Info;
	unsigned int L3Index;
	unsigned char* L4Info;
	unsigned short MagicID;
	unsigned short crc;
};

#define PACKAGE_PAGEINFO_CRC(p) (p)->crc = nand_crc16(0,(unsigned char *)(p),(unsigned int)&(((NandPageInfo *)0)->crc))

#define IS_PAGEINFO(p) (((p)->MagicID == 0xaaaa) && (p)->crc == nand_crc16(0,(unsigned char *)(p),(unsigned int)&(((NandPageInfo *)0)->crc)))

#define CONVERT_DATA_NANDPAGEINFO(data,p,l4,l3,l2)				\
	do{															\
		unsigned char *d = (unsigned char *)data;				\
		p = (NandPageInfo *)d;									\
		p->L4Info = d + sizeof(NandPageInfo);					\
		if((l3) != 0){											\
			if((l2) == 0){										\
				p->L3Info = p->L4Info + (l4);					\
			}else{												\
				p->L2Info_Sector.L2Info = p->L4Info + (l4);	\
				p->L3Info = p->L2Info_Sector.L2Info + (l2);	\
			}													\
		}														\
	}while(0)
#endif
