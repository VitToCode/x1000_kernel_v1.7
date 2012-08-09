#ifndef __NANDMANGER_H__
#define __NANDMANGER_H__

#include "sectorlist.h"
#include "lpartition.h"
#include <linux/device.h>
#include <linux/vmalloc.h>

LPartition *p1, *p2, *p3;
static char *buf, *buf0, *buf1, *buf2;
struct file *file0, *file1, *file2;
#define BUF_SIZE		(1 * 1024 * 1024)
#define DISK0_SIZE		(48 * 1024 * 1024)
#define DISK1_SIZE		(1 * 1024 * 1024)
#define DISK2_SIZE		(1 * 1024 * 1024)
#define SECTOR_SIZE 	512

//#define BUF_EMULATION
#define FILE_EMULATION
#define DBG_FUNC() printk("nand block debug: func = %s \n", __func__)

inline int NandManger_open ( char* name, int mode )
{
	int handle = mode;

	DBG_FUNC();

	printk("handle = %d\n", handle);

	return handle;
}

inline int NandManger_read ( int context, SectorList* bl )
{
	struct singlelist *plist = NULL;
	SectorList* sl = NULL;
	char *buf = NULL;
	char *name = NULL;
	struct file *tmp_file = NULL;
	int readsize = 0;
	loff_t pos;
	mm_segment_t old_fs;

	DBG_FUNC();

	if (context == 0) {
		buf = buf0;
		tmp_file = file0;
		name = "ndisk0";
	} else if (context == 1) {
		buf = buf1;
		tmp_file = file1;
		name = "ndisk1";
	} else if (context == 2) {
		buf = buf2;
		tmp_file = file2;
		name = "ndisk2";
	} else
		printk("READ: unknown disk!\n");

	if (!bl) {
		return 0;
		printk("ERROR: READ, Sectorlist is null\n!");
	}		

	singlelist_for_each(plist, &bl->head) {
		sl = singlelist_entry(plist, SectorList, head);
#ifdef BUF_EMULATION
		printk("READ: %s, pData = %p, buf = %p, startSector = %d, sectorCount = %d\n",
			   name, sl->pData, buf, sl->startSector, sl->sectorCount);

		memcpy(sl->pData, (void *)buf + (sl->startSector * SECTOR_SIZE), sl->sectorCount * SECTOR_SIZE);
#endif

#ifdef FILE_EMULATION
		old_fs = get_fs();
		set_fs(KERNEL_DS);

		pos = sl->startSector * SECTOR_SIZE;

		//printk("READ: %s, tmp_file = %p, pData = %p, startSector = %d, sectorCount = %d, pos = %d\n",
		//	   name, tmp_file, sl->pData, sl->startSector, sl->sectorCount, (int)pos);

		readsize = vfs_read(tmp_file, sl->pData, sl->sectorCount * SECTOR_SIZE, &pos);
		if (readsize != (sl->sectorCount * SECTOR_SIZE))
			printk("ERROR: read error, readsize = %d, pos = %d\n", readsize, (int)pos);

		set_fs(old_fs);
#endif
	}

	return 0;
}

inline int NandManger_write ( int context, SectorList* bl )
{
	struct singlelist *plist = NULL;
	SectorList* sl = NULL;
	char *buf = NULL;
	char *name = NULL;
	struct file *tmp_file = NULL;
	int writen = 0;
	loff_t pos;
	mm_segment_t old_fs;

	DBG_FUNC();

	if (context == 0) {
		buf = buf0;
		tmp_file = file0;
		name = "ndisk0";
	} else if (context == 1) {
		buf = buf1;
		tmp_file = file1;
		name = "ndisk1";
	} else if (context == 2) {
		buf = buf2;
		tmp_file = file2;
		name = "ndisk2";
	} else
		printk("WRITE: unknown disk!\n");

	if (!bl) {
		return 0;
		printk("ERROR: WRITE, Sectorlist is null\n!");
	}		

	singlelist_for_each(plist, &bl->head) {
		sl = singlelist_entry(plist, SectorList, head);
#ifdef BUF_EMULATION
		printk("WRITE: %s, pData = %p, buf = %p, startSector = %d, sectorCount = %d\n",
			   name, sl->pData, buf, sl->startSector, sl->sectorCount);

		memcpy((void *)buf + (sl->startSector * SECTOR_SIZE), sl->pData, sl->sectorCount * SECTOR_SIZE);
#endif

#ifdef FILE_EMULATION
		old_fs = get_fs();
		set_fs(KERNEL_DS);

		pos = sl->startSector * SECTOR_SIZE;

		//printk("WRITE: %s, tmp_file = %p, pData = %p, startSector = %d, sectorCount = %d, pos = %d\n",
		//	   name, tmp_file, sl->pData, sl->startSector, sl->sectorCount, (int)pos);

		writen = vfs_write(tmp_file, sl->pData, sl->sectorCount * SECTOR_SIZE, &pos);
		if (writen != (sl->sectorCount * SECTOR_SIZE))
			printk("ERROR: write error, writen = %d, pos = %d\n", writen, (int)pos);

		set_fs(old_fs);
#endif
	}

	return 0;
}

inline int NandManger_ioctrl ( int context, int cmd, int args )
{
	DBG_FUNC();
	return 0;
}

inline int NandManger_close ( int context )
{
	DBG_FUNC();
	return 0;
}

inline int NandManger_getPartition ( LPartition **pt )
{
	int handle = 0;
	DBG_FUNC();

	p1 = kzalloc(sizeof(LPartition), GFP_KERNEL);
	p1->name = "ndisk0";
	p1->sectorCount = DISK0_SIZE/SECTOR_SIZE;
	p1->mode = 0;

	p2 = kzalloc(sizeof(LPartition), GFP_KERNEL);
	p2->name = "ndisk1";
	p2->sectorCount = DISK1_SIZE/SECTOR_SIZE;
	p2->mode = 1;

	p3 = kzalloc(sizeof(LPartition), GFP_KERNEL);
	p3->name = "ndisk2";
	p3->sectorCount = DISK2_SIZE/SECTOR_SIZE;
	p3->mode = 2;

	*pt = p1;
	p1->head.next = &p2->head;
	p2->head.next = &p3->head;
	p3->head.next = NULL;

	return handle;
}

inline int NandManger_Init ( void )
{
	DBG_FUNC();

#ifdef BUF_EMULATION
	buf0 = vzalloc(DISK0_SIZE);
	if (!buf0)
		printk("ERROR: buf0 alloc memory error!!!\n");

	buf1 = vzalloc(DISK1_SIZE);
	if (!buf1)
		printk("ERROR: buf1 alloc memory error!!!\n");

	buf2 = vzalloc(DISK2_SIZE);
	if (!buf2)
		printk("ERROR: buf2 alloc memory error!!!\n");
#endif

#ifdef FILE_EMULATION
	{
		mm_segment_t old_fs;
		int writen;
		loff_t pos;
		int i;

		/* alloc a buf, data is 0x00, size is 1Mbyte */
		buf = vzalloc(BUF_SIZE);
		if (!buf)
			printk("ERROR: buf alloc memory error!!!\n");

		file0 = filp_open("/data/local/tmp/ndisk0", O_RDWR | O_CREAT, 0666);
		if (!file0)
			printk("ERROR: can not create file '/data/local/tmp/ndisk0'!\n");

		if (file0) {
			pos = 0;
			old_fs = get_fs();
			set_fs(KERNEL_DS);		
			for (i = 0; i < (DISK0_SIZE / BUF_SIZE); i++) {
				writen = vfs_write(file0, buf, BUF_SIZE, &pos);
				if (writen != BUF_SIZE)
					printk("ERROR: init '/data/local/tmp/ndisk0' error, writen = %d, buf = %p, pos = %d\n",
						   writen, buf, (int)pos);
			}
			set_fs(old_fs);
		}

		file1 = filp_open("/data/local/tmp/ndisk1", O_RDWR | O_CREAT | O_LARGEFILE, 0666);
		if (!file1)
			printk("ERROR: can not create file '/data/local/tmp/ndisk1'!\n");

		if (file1) {
			pos = 0;
			old_fs = get_fs();
			set_fs(KERNEL_DS);
			for (i = 0; i < (DISK1_SIZE / BUF_SIZE); i++) {
				writen = vfs_write(file1, buf, BUF_SIZE, &pos);
				if (writen != BUF_SIZE)
					printk("ERROR: init '/data/local/tmp/ndisk1' error, writen = %d, buf = %p, pos = %d\n",
						   writen, buf, (int)pos);
			}
			set_fs(old_fs);
		}

		file2 = filp_open("/data/local/tmp/ndisk2", O_RDWR | O_CREAT | O_LARGEFILE, 0666);
		if (!file2)
			printk("ERROR: can not create file '/data/local/tmp/ndisk1'!\n");

		if (file2) {
			pos = 0;
			old_fs = get_fs();
			set_fs(KERNEL_DS);
			for (i = 0; i < (DISK1_SIZE / BUF_SIZE); i++) {
				writen = vfs_write(file2, buf, BUF_SIZE, &pos);
				if (writen != BUF_SIZE)
					printk("ERROR: init '/data/local/tmp/ndisk2' error, writen = %d, buf = %p, pos = %d\n",
						   writen, buf, (int)pos);
			}
			set_fs(old_fs);
		}

		vfree(buf);
	}
#endif

	return 0;
}

inline void NandManger_Deinit (void)
{
	DBG_FUNC();

#ifdef BUF_EMULATION
	kfree(buf0);
	kfree(buf1);
	kfree(buf2);
#endif

#ifdef FILE_EMULATION
	if (filp_close(file0, NULL) < 0)
		printk("ERROR: close file '/data/local/tmp/ndisk0' error!\n");

	if (filp_close(file1, NULL) < 0)
		printk("ERROR: close file '/data/local/tmp/ndisk1' error!\n");

	if (filp_close(file2, NULL) < 0)
		printk("ERROR: close file '/data/local/tmp/ndisk2' error!\n");
#endif
}

inline int NandManger_Register_start(int (*start)(void))
{
	DBG_FUNC();
	return 0;
}
#endif

/*	legacy_mbr *legacymbr = (legacy_mbr *)buf0;
	legacymbr.partition_record[0].start_sect = 4;
	legacymbr.partition_record[0].nr_sects = 12 * 1024 * 1024 / 512 - 4 - 1;
	legacymbr.signature = MSDOS_MBR_SIGNATURE;
*/
