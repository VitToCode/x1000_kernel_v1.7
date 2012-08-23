/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2007 by dsqiu (dsqiu@ingenic.cn)
 */


#ifndef _BADBLOCKINFO_H_
#define _BADBLOCKINFO_H_

typedef struct _BadBlockInfo BadBlockInfo;

struct _BadBlockInfo {
	int *badblocktable;
	int *blockmap;
	int *zone_startBlockID;
	int startblockid;
	int blocks;
        int blocksperzone;
	int zonecount;
};

int BadBlockInfo_Init(int *table,int startblockid,int blocks,int blocksperzone);
void BadBlockInfo_Deinit(int handle);
int BadBlockInfo_Get_Zone_startBlockID(int handle,int zoneid);
int BadBlockInfo_Get_blockID(int handle,int zoneid,int zblockid);
int BadBlockInfo_ConvertBlockToZoneID(int handle,int blockid);
int BadBlockInfo_Get_ZoneCount(int handle);

#endif /* _BADBLOCKINFO_H_ */
