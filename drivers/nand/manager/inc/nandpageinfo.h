#ifndef __NANDPAGEINFO_H__
#define __NANDPAGEINFO_H__

typedef struct _NandPageInfo NandPageInfo;
struct _NandPageInfo {
	unsigned short NextPageInfo;
	unsigned short ZoneID;
	unsigned short len;
	unsigned short L1Index;
	unsigned char *L2Info;
	unsigned int L2Index;
	unsigned char *L3Info;
	unsigned int L3Index;
	unsigned char* L4Info;
	unsigned short MagicID;
	unsigned short crc;
} __attribute__ ((packed));
#define CONVERT_DATA_NANDPAGEINFO(data,p,l4,l3,l2)			\
	do{								\
		unsigned char *d = (unsigned char *)data;		\
		p = (NandPageInfo *)d;					\
		p->L4Info = d + sizeof(NandPageInfo);			\
		if((l3) != 0){						\
			if((l2) == 0){					\
				p->L3Info = p->L4Info + (l4);		\
			}else{						\
				p->L2Info = p->L4Info + (l4);		\
				p->L3Info = p->L2Info + (l2);		\
			}						\
		}							\
	}while(0)
#endif
