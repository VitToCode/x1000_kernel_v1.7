#ifndef __SIGZONEINFO_H__
#define __SIGZONEINFO_H__

typedef struct _SigZoneInfo SigZoneInfo;

struct _SigZoneInfo {
  /** Attributes **/
  /*public*/
    unsigned int lifetime;
    unsigned short badblock;
    unsigned short validpage;
  /** Associations **/
/** Operations **/
};

/** Operations **/
/*public*/
SigZoneInfo* SigZoneInfo_get ( SigZoneInfo *this, unsigned short zoneID );
void SigZoneInfo_set ( SigZoneInfo *this, unsigned short zoneID, SigZoneInfo sigzoneinfo );
void SigZoneInfo_Init ( SigZoneInfo *this, int context );
void SigZoneInfo_DeInit ( SigZoneInfo *this, int context );
#endif
