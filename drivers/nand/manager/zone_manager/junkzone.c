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
	memset(jzone->node,0xff,sizeof(junkzonenode) * zonecount);

	for (i = 0; i < REMOVE_ZONECOUNT; i++) {
		jzone->removezone[i].index = -1;
		jzone->removezone[i].id = -1;
	}

	jzone->zonecount = zonecount;
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
	int empty = -1;
	unsigned int minsectors = -1;
	int min_index = -1;

	NandMutex_Lock(&jzone->mutex);
	if (zoneid == -1) {
		NandMutex_Unlock(&jzone->mutex);
		return;
	}

	for (i = 0; i < REMOVE_ZONECOUNT; i++) {
		if (jzone->removezone[i].id == zoneid) {
			NandMutex_Unlock(&jzone->mutex);
			return;
		}
	}

	for (i = 0; i < jzone->zonecount; i++) {
		if (jzone->node[i].zoneid == -1) {
			empty = i;
			continue;
		}
		if (zoneid == jzone->node[i].zoneid) {
			jzone->node[i].sectors += sectors;
			break;
		}
		if (jzone->node[i].sectors < minsectors) {
			minsectors = jzone->node[i].sectors;
			min_index = i;
		}
	}

	if (i >= jzone->zonecount) {
		if (empty != -1) {
			jzone->node[empty].sectors = sectors;
			jzone->node[empty].zoneid = zoneid;
		} else if (min_index != -1) {
			jzone->node[min_index].sectors = sectors;
			jzone->node[min_index].zoneid = zoneid;
		} else {
			ndprint(JUNKZONE_ERROR,"ERROR: don't run it %s %d\n",__FILE__,__LINE__);
		}
	}
	NandMutex_Unlock(&jzone->mutex);
}

int Get_MaxJunkZone(int handle){
	junkzone *jzone = (junkzone *)handle;
	int zoneid = -1;
	int zoneindex = -1;
	int maxsectors = 0;
	int i,j;

	NandMutex_Lock(&jzone->mutex);
	for (i = 0; i < jzone->zonecount; i++) {
		if (jzone->node[i].zoneid != -1){
			// isremovezone
			for(j = 0;j < REMOVE_ZONECOUNT;j++){
				if(jzone->removezone[j].id != -1 && jzone->node[i].zoneid == jzone->removezone[j].id)
					break;
			}
			if(j >= REMOVE_ZONECOUNT){
				if((maxsectors < jzone->node[i].sectors)) {
					maxsectors = jzone->node[i].sectors;
					zoneid = jzone->node[i].zoneid;
					zoneindex = i;
				}
			}
		}
		/*ndprint(JUNKZONE_INFO,"jzone[%d] \t zoneid = %d \t sectors = %d\n",
		  i, jzone->node[i].zoneid, jzone->node[i].sectors);*/
	}

	for (i = 0; i < REMOVE_ZONECOUNT; i++) {
		if (jzone->removezone[i].id == -1) {
			jzone->removezone[i].id = zoneid;
			jzone->removezone[i].index = zoneindex;
			break;
		}
	}

	if (i >= REMOVE_ZONECOUNT)
		zoneid = -1;

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
			jzone->node[jzone->removezone[i].index].sectors = -1;
			jzone->node[jzone->removezone[i].index].zoneid = -1;
			jzone->removezone[i].index = -1;
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
	for (i = 0; i < jzone->zonecount; i++) {
		if (jzone->node[i].zoneid != -1){
			if(jzone->node[i].zoneid == zoneid) {
				jzone->node[i].sectors = -1;
				jzone->node[i].zoneid = -1;
				//ndprint(JUNKZONE_INFO, "%s, zoneid = %d\n", __func__, zoneid);
			}
		}
	}
	NandMutex_Unlock(&jzone->mutex);
}

int Get_JunkZoneCount(int handle){
	int i;
	int count = 0;
	junkzone *jzone = (junkzone *)handle;

	NandMutex_Lock(&jzone->mutex);
	for (i = 0; i < jzone->zonecount; i++) {
		if ((jzone->node[i].zoneid != -1) && (jzone->node[i].sectors > (128 * 3)))
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
		ndprint(JUNKZONE_INFO, "junkzone[%d]:\t zoneid = %d,\t sectors = %d\n",
				i, jzone->node[i].zoneid, jzone->node[i].sectors);
	}
	ndprint(JUNKZONE_INFO, "========================== remove zone =========================\n");
	for (i=0; i < REMOVE_ZONECOUNT; i++) {
		ndprint(JUNKZONE_INFO, "removezone[%d]:\t zoneid = %d,\t index = %d\n",
				i, jzone->removezone[i].id, jzone->removezone[i].index);
	}
}
