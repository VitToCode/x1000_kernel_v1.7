#include "junkzone.h"
#include "NandAlloc.h"
#include "nanddebug.h"
int Init_JunkZone(int zonecount){
	junkzone *jzone;
	int i;
	jzone = Nand_VirtualAlloc(sizeof(junkzone));
	if(!jzone)
		return 0;
	jzone->node = Nand_VirtualAlloc(sizeof(junkzonenode) * zonecount);
	if(!jzone->node){
		Nand_VirtualFree(jzone);
		return 0;
	}	
	memset(jzone->node,0xff,sizeof(junkzonenode) * zonecount);
	for(i = 0;i < REMOVE_ZONECOUNT;i++)
		jzone->removezoneid[i] = -1;
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
	for(i = 0;i < REMOVE_ZONECOUNT;i++)
	{
		if((jzone->removezoneid[i] != -1) && (jzone->removezoneid[i] == zoneid)){
			NandMutex_Unlock(&jzone->mutex);
			return;
		}
	}
	for(i = 0;i < jzone->zonecount;i++){
		if(jzone->node[i].zoneid == -1) empty = i;
		if( zoneid == jzone->node[i].zoneid){
			if(sectors > jzone->node[i].sectors){
				jzone->node[i].sectors += sectors;
				break;
			}
		}
		if(jzone->node[i].sectors < minsectors){
			minsectors = jzone->node[i].sectors;
			min_index = i;
		}
	}
	if(i >= jzone->zonecount){
		if(empty != -1){
			jzone->node[empty].sectors = sectors;
			jzone->node[empty].zoneid = zoneid;
		}else if(min_index != -1){
			jzone->node[min_index].sectors = sectors;
			jzone->node[min_index].zoneid = zoneid;
		}else{
			ndprint(1,"ERROR: don't run it %s %d\n",__FILE__,__LINE__);
		}
		
	}
	NandMutex_Unlock(&jzone->mutex);
}
int Get_MaxJunkZone(int handle){
	junkzone *jzone = (junkzone *)handle;
	int zoneid = -1;
	int maxsectors = 0;
	int i;
	NandMutex_Lock(&jzone->mutex);
	for(i = 0;i < jzone->zonecount;i++){
		
		if((jzone->node[i].zoneid != -1)&&(maxsectors < jzone->node[i].sectors)){
			zoneid = i;
		}
	}
	for(i = 0;i < REMOVE_ZONECOUNT;i++){
		if(jzone->removezoneid[i] == -1){
			jzone->removezoneid[i] = zoneid;
			break;
		}
	}
	if(i >= REMOVE_ZONECOUNT) zoneid = -1;
	NandMutex_Unlock(&jzone->mutex);
	return zoneid;
}

void Release_MaxJunkZone(int handle,int zoneid){
	junkzone *jzone = (junkzone *)handle;
	int i;
	NandMutex_Lock(&jzone->mutex);
	for(i = 0; i < REMOVE_ZONECOUNT;i++){
		if(jzone->removezoneid[i] == zoneid)
			jzone->removezoneid[i] = -1;
	}
	NandMutex_Unlock(&jzone->mutex);
}
