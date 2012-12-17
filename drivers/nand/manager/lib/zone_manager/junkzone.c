#include "junkzone.h"
#include "os/NandAlloc.h"
#include "nanddebug.h"
#include "context.h"
#include "vnandinfo.h"
#include "zone.h"
#include "os/clib.h"
int Init_JunkZone(int context,int zonecount){
	junkzone *jzone;
	Context *conptr = (Context *)context;
	int i;
	unsigned int timestamp;
	jzone = Nand_VirtualAlloc(sizeof(junkzone));
	if (!jzone)
		return 0;
	jzone->context = context;
	jzone->node = Nand_VirtualAlloc(sizeof(junkzonenode) * zonecount);
	if (!jzone->node) {
		Nand_VirtualFree(jzone);
		return 0;
	}
	timestamp = nd_get_timestamp();
	for(i = 0;i < zonecount;i++) {
		jzone->node[i].sectors = 0;
		jzone->node[i].type = 0;
		jzone->node[i].timestamp = timestamp;
	}
	jzone->zonecount = zonecount;
	jzone->maxjunksectors = conptr->vnand.PagePerBlock * BLOCKPERZONE(context) * (conptr->vnand.BytePerPage / SECTOR_SIZE);
	jzone->current_zoneid = -1;
	jzone->max_timestamp = timestamp;
	jzone->min_node = NULL;
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
	junkzone *jzone = (junkzone *)handle;

	NandMutex_Lock(&jzone->mutex);
	if (zoneid == -1 || zoneid >= jzone->zonecount) {
		if(zoneid != -1)
			ndprint(JUNKZONE_ERROR,"set zoneid too large! zoneid = %d zonecount = %d\n",zoneid,jzone->zonecount);
		NandMutex_Unlock(&jzone->mutex);
		return;
	}
	jzone->node[zoneid].sectors += sectors;
	jzone->max_timestamp = nd_get_timestamp();
	jzone->node[zoneid].timestamp = jzone->max_timestamp;
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
	jzone->node[zoneid].type = CURRENT_ZONETYPE;
	NandMutex_Unlock(&jzone->mutex);
}

int Get_MaxJunkZone(int handle,int minjunksector){
	junkzone *jzone = (junkzone *)handle;
	int zoneid = -1;
	int maxsectors = minjunksector;
	int i;

	NandMutex_Lock(&jzone->mutex);
	for (i = 0; i < jzone->zonecount; i++) {
		if(maxsectors <= jzone->node[i].sectors && jzone->node[i].type == 0 && (&jzone->node[i] != jzone->min_node)) {
			maxsectors = jzone->node[i].sectors;
			zoneid = i;
		}
	}
	if(zoneid != -1) {
		jzone->node[zoneid].timestamp = -1;
	}
	NandMutex_Unlock(&jzone->mutex);
	if (zoneid != -1)
		ndprint(JUNKZONE_INFO,"Get_MaxJunkZone zoneid = %d,sectors = %d\n"
				, zoneid,jzone->node[zoneid].sectors);
	return zoneid;
}

void Release_MaxJunkZone(int handle,int zoneid){
	Delete_JunkZone(handle,zoneid);
}

void Delete_JunkZone(int handle,int zoneid){
	junkzone *jzone = (junkzone *)handle;
	NandMutex_Lock(&jzone->mutex);
	if(zoneid >= 0 && zoneid < jzone->zonecount) {
		jzone->node[zoneid].sectors = 0;
		jzone->node[zoneid].type = 0;
		jzone->node[zoneid].timestamp = -1;

	}
	NandMutex_Unlock(&jzone->mutex);
}
void Drop_JunkZone(int handle,int zoneid){
	junkzone *jzone = (junkzone *)handle;
	NandMutex_Lock(&jzone->mutex);
	if(zoneid >= 0 && zoneid < jzone->zonecount) {
		jzone->node[zoneid].sectors = 0;
		jzone->node[zoneid].type = DROP_ZONETYPE;
		jzone->node[zoneid].timestamp = -1;

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
int Get_JunkZoneRecycleZone(int handle) {
	junkzone *jzone = (junkzone *)handle;
	int zoneid = -1;
	NandMutex_Lock(&jzone->mutex);
	if(jzone->min_node) {
		zoneid = jzone->min_node - &jzone->node[0];
		ndprint(JUNKZONE_INFO,"recycle junk zoneid = %d,sectors = %d validpage = %d\n",zoneid,jzone->min_node->sectors,ZoneManager_GetValidPage(jzone->context,zoneid));
		jzone->min_node = NULL;
	}

	NandMutex_Unlock(&jzone->mutex);
	return zoneid;
}
int Get_JunkZoneRecycleTrig(int handle,int minpercent) {
	junkzone *jzone = (junkzone *)handle;
	int ret = 0;
	int curtime;
	int i;
	int k,junkpercent;
	int max_k = 0;
	Context *conptr = (Context *)(jzone->context);
	VNandInfo *vnand = &conptr->vnand;
	unsigned int spp = vnand->BytePerPage / SECTOR_SIZE;
	int validpage;
	int curpercent = 0;
	curtime = nd_get_timestamp();
	NandMutex_Lock(&jzone->mutex);
	for(i = 0;i < jzone->zonecount;i++) {
		/*
		if(jzone->node[i].timestamp != -1 && jzone->node[i].sectors > minsectors && jzone->node[i].type == 0) {
			k = (int)((unsigned int)curtime - (unsigned int)jzone->node[i].timestamp);
			k = k / 1000;
			k = k * (int)jzone->node[i].sectors;
			if(max_k < k) {
				max_k = k;
				jzone->min_node = &jzone->node[i];
			}
		}
		*/
		if(jzone->node[i].timestamp != -1 && jzone->node[i].type == 0) {
			validpage = ZoneManager_GetValidPage(jzone->context,i);
			if(validpage > 60000)
				validpage = 2048;
			junkpercent = (jzone->node[i].sectors + (jzone->maxjunksectors - validpage * spp)) * 10000 / jzone->maxjunksectors;
			if(junkpercent > minpercent) {
				k = (int)((unsigned int)curtime - (unsigned int)jzone->node[i].timestamp);
				k = k / 1000;
				k = k * (junkpercent - minpercent);
				if(max_k < k) {
					max_k = k;
					curpercent = junkpercent;
					jzone->min_node = &jzone->node[i];
				}
			}
		}
	}
	// 1s
	if(jzone->min_node) {
		ndprint(JUNKZONE_INFO,"max_k = %d junkpercent = %d\n",max_k,curpercent);
		ret = 1;
	}
	NandMutex_Unlock(&jzone->mutex);
	return ret;
}
int Get_JunkZoneForceRecycleTrig(int handle,int percent) {
	int i;
	int maxsectors = 0;
	junkzone *jzone = (junkzone *)handle;
	NandMutex_Lock(&jzone->mutex);
	for (i = 0; i < jzone->zonecount; i++) {
		if(jzone->node[i].sectors > maxsectors && jzone->node[i].type == 0)
			maxsectors = jzone->node[i].sectors;
	}
	NandMutex_Unlock(&jzone->mutex);
	return (maxsectors >= jzone->maxjunksectors * percent / 100);
}
void dump_JunkZone(int handle) {
	junkzone *jzone = (junkzone *)handle;
	int i;
	ndprint(JUNKZONE_INFO, "========================== junk zone =========================\n");
	for (i = 0; i < jzone->zonecount; i++) {
		ndprint(JUNKZONE_INFO, "junkzone[%d]:\t type = %d,\t sectors = %d\n",
				i, jzone->node[i].type, jzone->node[i].sectors);
	}
}

int Get_JunkZoneSectors(int handle,unsigned int zid) {
	int sectors;
	junkzone *jzone = (junkzone *)handle;
	if(zid > jzone->zonecount) return 0;
	NandMutex_Lock(&jzone->mutex);
	sectors = jzone->node[zid].sectors;
	NandMutex_Unlock(&jzone->mutex);
	return sectors;
}
void Set_BufferToJunkZone(int handle,void *d,int bytes,int maxsectors) {
	unsigned int *package_junk;
	junkzone *jzone = (junkzone *)handle;
	int count = bytes / sizeof(unsigned int);
	int i;
	unsigned short sectors,zid;
	package_junk = (unsigned int *)d;
	NandMutex_Lock(&jzone->mutex);
	for(i = 0;i < count;i++) {
		if(package_junk[i] != -1) {
			sectors = package_junk[i] & 0xffff;
			zid = package_junk[i] >> 16;
			if(sectors <= maxsectors)
				jzone->node[zid].sectors = sectors;
		}
	}
	NandMutex_Unlock(&jzone->mutex);
}

int Get_JunkZoneToBuffer(int handle,void *d,int bytes,int *rank,int rankcount) {
	unsigned int *package_junk;
	junkzone *jzone = (junkzone *)handle;
	int i,n,pjunk_count = 0;
	unsigned int max,min;
	int totalsectors = 0;
	package_junk = (unsigned int *)d;
	NandMutex_Lock(&jzone->mutex);
	n = 0;
	while(n < rankcount) {
		if(n == 0){
			max = -1;
			min = rank[n];
		}else if(n == rankcount - 1) {
			min = 0;
			max = rank[n];
		} else {
			min = rank[n];
			max = rank[n - 1];
		}

		for(i = 0;i < jzone->zonecount;i++) {
			if(max > jzone->node[i].sectors && min <= jzone->node[i].sectors){
				if(pjunk_count > bytes / 4) {
					break;
				}
				package_junk[pjunk_count++] = (i << 16) | jzone->node[i].sectors;
				totalsectors += jzone->node[i].sectors;
			}
		}
		if(i < jzone->zonecount)
			break;
		n++;
	}
	NandMutex_Unlock(&jzone->mutex);
	return pjunk_count;
}
