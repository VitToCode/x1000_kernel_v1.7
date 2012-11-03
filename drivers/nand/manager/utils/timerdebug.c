#include "timerdebug.h"
#include "clib.h"
#include "sectorlist.h"
#include "pagelist.h"
#include "NandAlloc.h"
#include "context.h"
#include "nanddebug.h"

#define TOTAL_BYTE   1*1024*1024


static int get_plslbyte(TimeByte *tb, void *pl_sl, unsigned int mode, unsigned int flag)
{
	struct singlelist *pos;
	PageList *pl = NULL;
	SectorList *sl = NULL;

	if (pl_sl == NULL){
		return -1;
	}
	switch(flag){
	case 0: /*PageList*/
		if(mode){/*write*/
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
		break;
	case 1: /*SectorList*/
		if(mode){/*write*/
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
		break;
	default:
		break;
	}
	return -1;
}

int Nd_TimerdebugInit(void)
{
	TimeByte *timebyte = (TimeByte*)Nand_VirtualAlloc(sizeof(TimeByte));
	memset(timebyte,0,sizeof(TimeByte));
	return (int)timebyte;
}

void Nd_TimerdebugDeinit(TimeByte *tb)
{
	Nand_VirtualFree(tb);
}
void Get_StartTime(TimeByte *tb, unsigned mode)
{
	if(mode){/*write*/
		if(tb->wcount == 0){
			tb->SW_time = nd_getcurrentsec_ns();
			tb->wcount = 1;
		}
	}else{/*read*/
		if(tb->rcount == 0){
			tb->SR_time = nd_getcurrentsec_ns();
			tb->rcount = 1;
		}
	}
}

void Calc_Speed(TimeByte *tb, void *ps,unsigned int mode,unsigned int listflag)
{
	unsigned int totalbyte, time;
	int rema = 0;
	if (mode){/*write*/
		if ((totalbyte=get_plslbyte(tb,ps,mode,listflag)) >= TOTAL_BYTE){
			tb->EW_time += nd_getcurrentsec_ns();
#ifdef __KERNEL__
            time = (unsigned int)div_s64_rem((tb->EW_time - tb->SW_time), 1000000 ,&rema);
#else
			time = (unsigned int)((tb->EW_time - tb->SW_time) / 1000000);
#endif
			tb->W_speed = totalbyte / time;
            tb->wcount = 0;
			tb->W_byte = 0;
            tb->EW_time = 0;
			ndprint(TIMER_INFO,"\n%s_WRITE: totalbytes = %d bytes, "
                                        "time = %d ms, speed = %d.%03d MB/s \n",
                                        listflag == 1 ? "  L2P" : "vNand",
                                        totalbyte, time,
                                        tb->W_speed/1000, tb->W_speed%1000);
		} else {
			tb->EW_time += nd_getcurrentsec_ns();
            tb->EW_time -= tb->SW_time;
			tb->wcount = 0;
                }
	}else{/*read*/
		if ((totalbyte=get_plslbyte(tb,ps,mode,listflag)) >= TOTAL_BYTE){
			tb->ER_time += nd_getcurrentsec_ns();
#ifdef __KERNEL__
            time = (unsigned int)div_s64_rem((tb->ER_time - tb->SR_time), 1000000, &rema);
#else
			time = (unsigned int)((tb->ER_time - tb->SR_time) / 1000000);
#endif
			tb->R_speed = totalbyte / time;
			tb->rcount = 0;
			tb->R_byte = 0;
            tb->ER_time = 0;
			ndprint(TIMER_INFO,"\n%s_READ: totalbytes = %d bytes, "
                                        "time = %d ms, speed = %d.%0d MB/s \n",
                                        listflag == 1 ? "  L2P" : "vNand",
                                        totalbyte, time,
                                        tb->R_speed/1000, tb->R_speed%1000);
		} else {
			tb->ER_time += nd_getcurrentsec_ns();
            tb->ER_time -= tb->SR_time;
			tb->rcount = 0;
              }
	}
}
