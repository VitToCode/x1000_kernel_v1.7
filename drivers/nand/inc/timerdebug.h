#ifndef __TIMER_H__
#define __TIMER_H__

#include "clib.h"
#include "sectorlist.h"
#include "pagelist.h"
#include "NandAlloc.h"

#define SECTOR_SIZE 512

typedef struct _TimeByte TimeByte;
struct _TimeByte {
	int R_byte;
	int W_byte;
	long long SR_time;
	long long ER_time;
	long long SW_time;
	long long EW_time;
	int W_speed;
	int R_speed;
	unsigned int rcount;
	unsigned int wcount;
};

static inline int nd_timerdebug_init(void)
{
	TimeByte *timebyte = (TimeByte*)Nand_VirtualAlloc(sizeof(TimeByte));
	memset(timebyte,0,sizeof(TimeByte));
	return (int)timebyte;
}

static inline int get_plslbyte(TimeByte *tb, void *pl_sl, unsigned int mode, unsigned int flag)
{
	struct singlelist *pos;
	PageList *pl = NULL;
	SectorList *sl = NULL;

	if (pl_sl == NULL){
		return -1;
	}
	switch(mode){
	case 0: /*PageList*/
		if(flag){/*write*/
			singlelist_for_each(pos,&(((PageList*)pl_sl)->head)){
				pl = singlelist_entry(pos,PageList,head);
				tb->W_byte += pl->Bytes;
			}
			return tb->W_byte;
		}else{/*read*/
			singlelist_for_each(pos,&(((PageList*)pl_sl)->head)){
				pl = singlelist_entry(pos,PageList,head);
				tb->R_byte += pl->Bytes;
			}
			return tb->R_byte;
		}
	case 1: /*SectorList*/
		if(flag){/*write*/
			singlelist_for_each(pos,&(((SectorList*)pl_sl)->head)){
				sl = singlelist_entry(pos,SectorList,head);
			 	tb->W_byte += sl->sectorCount * SECTOR_SIZE;
			}
			return tb->W_byte;
		}else{/*read*/
			singlelist_for_each(pos,&(((SectorList*)pl_sl)->head)){
				sl = singlelist_entry(pos,SectorList,head);
				tb->R_byte += sl->sectorCount * SECTOR_SIZE;
			}
			return tb->R_byte;
		}
	default:
		break;
	}
	return -1;
}

#endif
