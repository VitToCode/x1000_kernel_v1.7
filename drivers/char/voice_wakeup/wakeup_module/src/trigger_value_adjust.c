
#include <common.h>
#include "interface.h"
#include "dmic_config.h"
#include "trigger_value_adjust.h"


int thr_table[TRIGGER_CNTS] = {3000,  5000};
int tri_cnt[TRIGGER_CNTS] = {2, 6};

int quantity_thr(int thr)
{
	int i;
	for(i = 0; i < TRIGGER_CNTS; i++) {
		if(thr_table[i] == thr)
			break;
	}
	return i;
}
int quantity_tri(int times)
{
	if(times < 4) {
		return 0; /* UP */
	} else {
		return 1; /* DOWN */
	}
}

/*
 * @return, adjusted value.
 * */
int adjust_trigger_value(int times_per_unit, int cur_thr)
{
	int result;
	int fix_times = quantity_tri(times_per_unit);
	int fix_thr   = quantity_thr(cur_thr);
	//int fix_result;

	//fix_result = fix_times;

	result = fix_times - fix_thr > 0 ? 1000 * (fix_times - fix_thr) : cur_thr;

	serial_put_hex(result);
	return result;
}


