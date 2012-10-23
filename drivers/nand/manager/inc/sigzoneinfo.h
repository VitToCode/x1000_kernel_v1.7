#ifndef __SIGZONEINFO_H__
#define __SIGZONEINFO_H__

typedef struct _SigZoneInfo SigZoneInfo;
struct _SigZoneInfo {
    unsigned int lifetime;
    unsigned short badblock;
    unsigned short validpage;
    unsigned short pre_zoneid;
    unsigned short next_zoneid;
} __attribute__ ((packed));

#define INIT_SIGZONEINFO(x) do{					\
		(x)->lifetime = 0xffffffff;				\
		(x)->badblock = 0;					\
		(x)->validpage = -1;					\
		(x)->pre_zoneid = -1;					\
		(x)->next_zoneid = -1;					\
	}while(0)

SigZoneInfo* SigZoneInfo_get ( SigZoneInfo *this, unsigned short zoneID );
void SigZoneInfo_set ( SigZoneInfo *this, unsigned short zoneID, SigZoneInfo sigzoneinfo );
void SigZoneInfo_Init ( SigZoneInfo *this, int context );
void SigZoneInfo_DeInit ( SigZoneInfo *this, int context );

#endif
