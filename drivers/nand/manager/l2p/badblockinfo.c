#include "badblockinfo.h"
#include "NandAlloc.h"
#include "clib.h"

int BadBlockInfo_Init(int *table,int startblockid,int blocks,int blocksperzone){
	int i,k,n,z;
	BadBlockInfo *bdbi;
	int mapsize;
	int prezonecount = (blocks + blocksperzone - 1) / blocksperzone;
	mapsize = (blocks + sizeof(int) - 1) / sizeof(int) * sizeof(int) / 8;
	bdbi = Nand_VirtualAlloc(sizeof(BadBlockInfo) + prezonecount * sizeof(int) + mapsize);
	bdbi->badblocktable = table;
	bdbi->zone_startBlockID = (int *)(bdbi + 1);
	bdbi->blockmap = (int *)((int)bdbi->zone_startBlockID + prezonecount * sizeof(int));
	memset(bdbi->blockmap,0,mapsize);
	bdbi->startblockid = startblockid;
	bdbi->blocks = blocks;
	bdbi->blocksperzone = blocksperzone;
	k = 0;
	n = 0;
	z = 0;
	
	for(i = startblockid;i < startblockid + blocks;i++){
		if(table[n] != i)
		{
			if(k == 0){
				bdbi->zone_startBlockID[z] = i;
				z++;
			}
			k++;
			if(k >= blocksperzone)
				k = 0;
			bdbi->blockmap[(i - startblockid) / 32] |=  1 << ((i - startblockid) % 32);
		}else
			n++;
	}
	bdbi->zonecount = z;

	return (int)bdbi;
}
void BadBlockInfo_Deinit(int handle){
	BadBlockInfo *bdbi = (BadBlockInfo *)handle;
	Nand_VirtualFree(bdbi);
}

int BadBlockInfo_Get_Zone_startBlockID(int handle,int zoneid){
	BadBlockInfo *bdbi = (BadBlockInfo *)handle;
	return bdbi->zone_startBlockID[zoneid];
}

int BadBlockInfo_Get_blockID(int handle,int zoneid,int zblockid){
	BadBlockInfo *bdbi = (BadBlockInfo *)handle;
	int startblockid = bdbi->zone_startBlockID[zoneid];
	int *bdmap = &bdbi->blockmap[startblockid / 32];
	int soffset = startblockid % 32;
	int n = soffset,k = 0,z = 0;

	while(k < zblockid + 1){
		if (n < 32) {
			if(*bdmap & (1 << n))
				k++;
			n++;
			z++;
		}
		else {
			n = 0;
			bdmap++;
		}
	}
	
	return startblockid + z - 1;
}

int BadBlockInfo_ConvertBlockToZoneID(int handle,int blockid){
	int i;
	BadBlockInfo *bdbi = (BadBlockInfo *)handle;
	int zoneid = blockid / bdbi->blocksperzone;
       	for(i = zoneid;i >= 0;i--){
		if(bdbi->zone_startBlockID[i] <= blockid)
			return i;
	}

	return -1;
}

int BadBlockInfo_Get_ZoneCount(int handle){
	BadBlockInfo *bdbi = (BadBlockInfo *)handle;
	return bdbi->zonecount;	
}
