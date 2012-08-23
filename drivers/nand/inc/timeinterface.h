#ifndef __TIMEINTERFACE_H__
#define __TIMEINTERFACE_H__

#include "timerdebug.h"

int Nd_TimerdebugInit(void);
void Nd_TimerdebugDeinit(TimeByte *tb);
void Get_StartTime(TimeByte *tb, unsigned mode);
void Calc_Speed(TimeByte *tb, void *ps,unsigned int mode);

#endif
