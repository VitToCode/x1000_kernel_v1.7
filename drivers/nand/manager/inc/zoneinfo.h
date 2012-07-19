#ifndef __ZONEINFO_H__
#define __ZONEINFO_H__

typedef struct _ZoneInfo ZoneInfo;
struct _ZoneInfo {
	unsigned short validpage;
    	unsigned short badblock;
    	unsigned short ZoneID;
	unsigned short MagicID;
	unsigned int lifetime;
};

#define CONV_SZ_ZI(sz,zi)												\
	do{																	\
		(zi)->validpage = (sz)->validpage;						\
		(zi)->badblock 	= (sz)->badblock;						\
		(zi)->lifetime 	= (sz)->lifetime;							\
	}while(0)
	
#define CONV_ZI_SZ(zi,sz)												\
	do{																	\
		(sz)->validpage = (zi)->validpage;						\
		(sz)->badblock 	= (zi)->badblock;						\
		(sz)->lifetime 	= (zi)->lifetime;							\
	}while(0)

#endif
