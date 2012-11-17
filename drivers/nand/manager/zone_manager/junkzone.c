#include "junkzone.h"
#include "NandAlloc.h"
#include "nanddebug.h"

int Init_JunkZone(int zonecount){
	junkzone *jzone;
	int i;

	jzone = Nand_VirtualAlloc(sizeof(junkzone));
	if (!jzone)
		return 0;

	jzone->node = Nand_VirtualAlloc(sizeof(junkzonenode) * zonecount);
	if (!jzone->node) {
		Nand_VirtualFree(jzone);
		return 0;
	}
	memset(jzone->node,0,sizeof(junkzonenode) * zonecount);

	for (i = 0; i < REMOVE_ZONECOUNT; i++) {
		jzone->removezone[i].id = -1;
	}

	jzone->zonecount = zonecount;
	jzone->current_zoneid = -1;
	InitNandMutex(&jzone->mutex);

	return (int)jzone;
}

void Deinit_JunkZone(int handle){
	junkzone *jzone = (junkzone *)handle;

	DeinitNandMutex(&jzone->mutex);
	Nand_VirtualFree(jzone->node);
	Nand_VirtualFree(jzone);
}

void Insert_JunkZone(int handle,int sectors,int zoneid){
	int i;
	junkzone *jzone = (junkzone *)handle;

	NandMutex_Lock(&jzone->mutex);
	if (zoneid == -1 || zoneid >= jzone->zonecount) {
		if(zoneid != -1)
			ndprint(JUNKZONE_ERROR,"set zoneid too large! zoneid = %d zonecount = %d\n",zoneid,jzone->zonecount);
		NandMutex_Unlock(&jzone->mutex);
		return;
	}

	for (i = 0; i < REMOVE_ZONECOUNT; i++) {
		if (jzone->removezone[i].id == zoneid) {
			NandMutex_Unlock(&jzone->mutex);
			return;
		}
	}
	jzone->node[zoneid].sectors += sectors;
	NandMutex_Unlock(&jzone->mutex);
}
void SetCurrentZoneID_JunkZone(int handle,int zoneid) {
	junkzone *jzone = (junkzone *)handle;

	if(jzone->current_zoneid == zoneid)
		return;
	NandMutex_Lock(&jzone->mutex);
	if(jzone->current_zoneid != -1)
		jzone->node[jzone->current_zoneid].type = 0;

	jzone->current_zoneid = zoneid;
	jzone->node[jzone->current_zoneid].type = CURRENT_ZONETYPE;
	NandMutex_Unlock(&jzone->mutex);
}
int Get_MaxJunkZone(int handle,int minjunksector){
	junkzone *jzone = (junkzone *)handle;
	int zoneid = -1;
	int maxsectors = minjunksector;
	int i;

	NandMutex_Lock(&jzone->mutex);

	for (i = 0; i < jzone->zonecount; i++) {
		if(maxsectors <= jzone->node[i].sectors && jzone->node[i].type == 0) {
			maxsectors = jzone->node[i].sectors;
			zoneid = i;
		}
	}
	if(zoneid != -1){
		for (i = 0; i < REMOVE_ZONECOUNT; i++) {
			if (jzone->removezone[i].id == -1) {
				jzone->removezone[i].id = zoneid;
				jzone->node[zoneid].type = REMOVE_ZONETYPE;
				break;
			}
		}

		if (i >= REMOVE_ZONECOUNT)
			zoneid = -1;
	}
	NandMutex_Unlock(&jzone->mutex);

	//ndprint(JUNKZONE_INFO,"Get_MaxJunkZone zoneid = %d\n", zoneid);
	return zoneid;
}

void Release_MaxJunkZone(int handle,int zoneid){
	junkzone *jzone = (junkzone *)handle;
	int i;
	NandMutex_Lock(&jzone->mutex);
	for (i = 0; i < REMOVE_ZONECOUNT; i++) {
		if (jzone->removezone[i].id == zoneid) {
			jzone->node[jzone->removezone[i].id].sectors = 0;
			jzone->node[jzone->removezone[i].id].type = 0;
			jzone->removezone[i].id = -1;
		}
	}
	NandMutex_Unlock(&jzone->mutex);
}

void Delete_JunkZone(int handle,int zoneid){
	junkzone *jzone = (junkzone *)handle;
	int i;
	NandMutex_Lock(&jzone->mutex);
	for (i = 0; i < REMOVE_ZONECOUNT; i++) {
		if (jzone->removezone[i].id == zoneid) {
			ndprint(JUNKZONE_ERROR, "ERROR: %s, zoneid is already in removezone, zoneid = %d\n",
					__func__, zoneid);
			NandMutex_Unlock(&jzone->mutex);
			return;
		}
	}
	if(zoneid > 0 && zoneid < jzone->zonecount) {
		jzone->node[zoneid].sectors = 0;
		jzone->node[zoneid].type = 0;
	}
	NandMutex_Unlock(&jzone->mutex);
}

int Get_JunkZoneCount(int handle){
	int i;
	int count = 0;
	junkzone *jzone = (junkzone *)handle;

	NandMutex_Lock(&jzone->mutex);
	for (i = 0; i < jzone->zonecount; i++) {
		if (jzone->node[i].sectors > (128 * 3))
			count++;
	}
	NandMutex_Unlock(&jzone->mutex);

	return count;
}

int Get_JunkZoneRecycleTrig(int handle) {
	junkzone *jzone = (junkzone *)handle;
	int usedcount = Get_JunkZoneCount(handle);
	return usedcount > jzone->zonecount * 2 / 3;
}

void dump_JunkZone(int handle) {
	junkzone *jzone = (junkzone *)handle;
	int i;
	ndprint(JUNKZONE_INFO, "========================== junk zone =========================\n");
	for (i = 0; i < jzone->zonecount; i++) {
		ndprint(JUNKZONE_INFO, "junkzone[%d]:\t type = %d,\t sectors = %d\n",
				i, jzone->node[i].type, jzone->node[i].sectors);
	}
	ndprint(JUNKZONE_INFO, "========================== remove zone =========================\n");
	for (i=0; i < REMOVE_ZONECOUNT; i++) {
		ndprint(JUNKZONE_INFO, "removezone[%d]:\t zoneid = %d\n",
				i, jzone->removezone[i].id);
	}
}
