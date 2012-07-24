#include "pagelist.h"
#include "blocklist.h"
#include "ppartition.h"
#include "nandinterface.h"
#include "vnandinfo.h"
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/fs.h>

#define MAXALOWBADBLOCK 1000
struct badblock {
	int blockid;
	int isbad;
};
static struct badblock bblocks[MAXALOWBADBLOCK];

struct Nand2K
{
    int PagePerBlock;
    int BytePerPage;
    int TotalBlocks;
	int MaxBadBlockCount;
} vNandChipInfo = {64, 2048, 128*8*4, 20}; //128M


PPartition ppt[] = {{"x-boot", 128*8*0, 64, 2048, 128*8, 20, 512, 0, 64*128*8, 1, NULL},
					/*{"kernel", 128*8*1, 64, 2048, 128*8, 20, 512, 0, 64*128*8, 1, NULL},
					{"ubifs", 128*8*2, 64, 2048, 128*8, 20, 512, 0, 64*128*8, 0, NULL},
					{"data", 128*8*3, 64, 2048, 128*8, 20, 512, 0, 64*128*8, 1, NULL},*/
					{"error", 128*8*1, 64, 2048, 1, 20, 512, 0, 64, 2, NULL}};

PPartArray partition={5, ppt};

struct vNand2K
{
	struct file *filp;
	char *pagebuf;
	struct Nand2K *nand;
};

static int em_vNand_InitNand (void *vd ){
	struct file *filp;
	char *buf;
	int i,n;
	loff_t pos;
	mm_segment_t old_fs;
	int cnt;
	VNandManager* vNand = vd;
	char ptname[256];
	struct vNand2K *vNandChip;

	vNand->info.PagePerBlock = vNandChipInfo.PagePerBlock; 			//64
	vNand->info.BytePerPage = vNandChipInfo.BytePerPage;			//2048
	vNand->info.TotalBlocks = vNandChipInfo.TotalBlocks;			//128 * 8 * 4
	vNand->info.MaxBadBlockCount = vNandChipInfo.MaxBadBlockCount;	//20
	vNand->pt = &partition;
	vNand->info.hwSector = 512;
	vNand->info.startBlockID = 0;

	vNandChip = vzalloc(sizeof(struct vNand2K));
	if (vNandChip == NULL) {
		printk("alloc memory error func %s line %d \n", __FUNCTION__, __LINE__);
		return -1;
	}

	buf = kmalloc(vNandChipInfo.BytePerPage, GFP_KERNEL); //2048 Bytes
	if (buf == NULL)
	{
		printk("alloc memory error func %s line %d \n", __FUNCTION__, __LINE__);
		return -1;
	}
	memset(buf, 0xff, vNandChipInfo.BytePerPage);

	sprintf(ptname, "/data/local/tmp/%s.bin", "nand");
	filp = filp_open(ptname, O_RDWR, 0);
	if(IS_ERR(filp))
	{
		filp = filp_open(ptname, O_RDWR | O_CREAT, 0777);
		if(IS_ERR(filp))
		{
			printk("open '%s' error func %s line %d \n",
				   ptname, __FUNCTION__, __LINE__);
			return -1;
		}

		pos = 0;
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		for (i = 0; i < vNandChipInfo.PagePerBlock * vNandChipInfo.TotalBlocks; i ++) {
			cnt = vfs_write(filp, buf, vNandChipInfo.BytePerPage, &pos);
		}
		set_fs(old_fs);
		printk("... pos = %d\n", (int)pos);
	}

	vNandChip->filp = filp;
	vNandChip->pagebuf = buf;
	vNandChip->nand = &vNandChipInfo;

	for (n = 0; n < partition.ptcount; n++){
		partition.ppt[n].prData = vNandChip;
	}

	printk("vNandChip %p\n", vNandChip);
	printk("vNandChip->nand %p\n", vNandChip->nand);

	for (i = 0; i < MAXALOWBADBLOCK; i++) {
		bblocks[i].blockid = i;
		bblocks[i].isbad = 0;
	}

	return 0;
}

static size_t page2offset(struct Nand2K *vNand,int pageid,int start)
{
	return pageid * vNand->BytePerPage + start * vNand->BytePerPage * vNand->PagePerBlock;
}

static size_t block2offset(struct Nand2K *vNand,int blockid,int start)
{
	return (blockid + start) * vNand->BytePerPage * vNand->PagePerBlock;
}

static int em_vNand_PageRead(void *pt,int pageid, int offsetbyte, int bytecount, void * data){

	struct vNand2K *p = (struct vNand2K *)PPARTITION(pt)->prData;
	int startblock = PPARTITION(pt)->startblockID;
	loff_t pos;
	mm_segment_t old_fs;
	int cnt;

	pos = page2offset(p->nand,pageid,startblock) + offsetbyte,SEEK_SET;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	cnt = vfs_read(p->filp, data, bytecount, &pos);
	set_fs(old_fs);

	return cnt;
}

static int em_vNand_PageWrite(void *pt,int pageid, int offsetbyte, int bytecount, void* data) {
	struct vNand2K *p = (struct vNand2K *)PPARTITION(pt)->prData;
	int startblock = PPARTITION(pt)->startblockID;
	loff_t pos;
	mm_segment_t old_fs;
	int cnt;

	pos = page2offset(p->nand,pageid,startblock) + offsetbyte,SEEK_SET;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	cnt = vfs_write(p->filp, data, bytecount, &pos);
	set_fs(old_fs);

	return cnt;
}

static int em_vNand_MultiPageRead(void *pt,PageList* pl) {
	struct vNand2K *p = (struct vNand2K *)PPARTITION(pt)->prData;
	int startblock = PPARTITION(pt)->startblockID;
	struct singlelist *sg;
	loff_t pos;
	mm_segment_t old_fs;

   	do {
		if(pl->startPageID == -1)
			return -1;

		pos = page2offset(p->nand,pl->startPageID,startblock) + pl->OffsetBytes,SEEK_SET;

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		pl->retVal = vfs_read(p->filp, pl->pData, pl->Bytes, &pos);
		set_fs(old_fs);

		if(pl->retVal <= 0)
		{
			printk("error::");
			return -1;
		}

		sg = (pl->head).next;
		if(sg == NULL)
			break;

		pl = singlelist_entry(sg,PageList,head);
	} while (pl);

	return 0;
}

static int em_vNand_MultiPageWrite(void *pt,PageList* pl) {
	struct vNand2K *p = (struct vNand2K *)PPARTITION(pt)->prData;
	struct singlelist *sg;
	int startblock = PPARTITION(pt)->startblockID;
	loff_t pos;
	mm_segment_t old_fs;

   	do {
		pos = page2offset(p->nand,pl->startPageID,startblock) + pl->OffsetBytes,SEEK_SET;

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		pl->retVal = vfs_write(p->filp, pl->pData, pl->Bytes, &pos);
		set_fs(old_fs);

		if(pl->retVal <= 0)
		{
			printk("error::");
			return -1;
		}

		sg = (pl->head).next;
		if(sg == NULL)
			break;

		pl = singlelist_entry(sg,PageList,head);
	} while (pl);

	return 0;
}

static int em_vNand_MultiBlockErase (void *pt,BlockList* pl ){
	struct vNand2K *p = (struct vNand2K *)PPARTITION(pt)->prData;
	int i,j;
	int ret = -1;
	struct singlelist *sg;
	int startblock = PPARTITION(pt)->startblockID;
	loff_t pos;
	mm_segment_t old_fs;

	memset(p->pagebuf, 0xff, p->nand->BytePerPage);
	do{
		pos = block2offset(p->nand,pl->startBlock,startblock),SEEK_SET;

		old_fs = get_fs();
		set_fs(KERNEL_DS);
		for(i = 0; i < pl->BlockCount; i++){
			for(j = 0; j < p->nand->PagePerBlock; j++){
				ret = vfs_write(p->filp, p->pagebuf, p->nand->BytePerPage, &pos);
				pl->retVal = 0;
				if(ret <= 0){
					pl->retVal = -1;
					break;
				}
			}
			if(pl->retVal < 0) {
				set_fs(old_fs);
				return -1;
			}
		}
		set_fs(old_fs);

		sg = (pl->head).next;
		if(sg == NULL)
			break;

		pl = singlelist_entry(sg,BlockList,head);
	} while(pl);

	return ret;
}

static int em_vNand_IsBadBlock (void *pt,int blockid ){
	int i = 0;
	struct vNand2K *p;
	int startblock;
	loff_t pos;
	mm_segment_t old_fs;

	for (i = 0; i < MAXALOWBADBLOCK; i++) {
		if(bblocks[i].blockid == blockid)
			return bblocks[i].isbad;
	}

	p = (struct vNand2K *)PPARTITION(pt)->prData;
	startblock = PPARTITION(pt)->startblockID;

	pos = block2offset(p->nand,blockid,startblock),SEEK_SET;

	old_fs = get_fs();
	set_fs(KERNEL_DS);
	if(vfs_read(p->filp, p->pagebuf, p->nand->BytePerPage, &pos) <= 0) {
		set_fs(old_fs);
		return -1;
	}
	set_fs(old_fs);

	if(p->pagebuf[p->nand->BytePerPage / 4 - 1] == 0)
		return -1;

	return 0;
}

static int em_vNand_MarkBadBlock (void *pt,unsigned int blockid ){
	int i = 0;
	loff_t pos;
	struct vNand2K *p;
	int startblock;
	mm_segment_t old_fs;

	for (i = 0; i < MAXALOWBADBLOCK; i++) {
		if(bblocks[i].blockid == blockid) {
			bblocks[i].isbad = 1;
			break;
		}
	}

//#if 0
	if (0) {
		p = (struct vNand2K *)PPARTITION(pt)->prData;
		startblock = PPARTITION(pt)->startblockID;

		pos = block2offset(p->nand,blockid,startblock),SEEK_SET;
		memset(p->pagebuf,0,p->nand->BytePerPage);
		old_fs = get_fs();
		set_fs(KERNEL_DS);
		if(vfs_write(p->filp, p->pagebuf, p->nand->BytePerPage, &pos) <= 0) {
			set_fs(old_fs);
			return -1;
		}
		set_fs(old_fs);
	}
//#endif

	return 0;
}

static int em_vNand_DeInitNand (void *vd){
	VNandManager* vNand = vd;
	struct vNand2K *p = (struct vNand2K *)PPARTITION(&vNand->pt->ppt[0])->prData;

	filp_close(p->filp, NULL);
	kfree(p->pagebuf);
	vfree(p);

	return 0;
}

NandInterface em_nand_ops = {
	.iInitNand = em_vNand_InitNand,
	.iPageRead = em_vNand_PageRead,
	.iPageWrite = em_vNand_PageWrite,
	.iMultiPageRead = em_vNand_MultiPageRead,
	.iMultiPageWrite = em_vNand_MultiPageWrite,
	.iMultiBlockErase = em_vNand_MultiBlockErase,
	.iIsBadBlock = em_vNand_IsBadBlock,
	.iDeInitNand = em_vNand_DeInitNand,
	.iMarkBadBlock = em_vNand_MarkBadBlock,
};

/*#################################################################*\
 *# bus
\*#################################################################*/
static ssize_t dbg_show(struct bus_type *bus, char *buf)
{
	printk("vnand dbg_show!\n");
	return 0;
}

static ssize_t dbg_store(struct bus_type *bus, const char *buf, size_t count)
{
	printk("vnand dbg_store, Register_NandDriver!\n");
	Register_NandDriver(&em_nand_ops);
	return count;
}

static struct bus_attribute bus_attr;
static struct bus_type vnand_bus = {
	.name		= "vnandb",
};

int __init ND_Init(void) {
	int ret;

	if (bus_register(&vnand_bus)) {
		printk("ERROR(os_vnand): bus_register error!\n");
		return -1;
	}

	bus_attr.show = dbg_show;
	bus_attr.store = dbg_store;
	sysfs_attr_init(&bus_attr.attr);
	bus_attr.attr.name = "dbg";
	bus_attr.attr.mode = S_IRUGO | S_IWUSR;
	ret = bus_create_file(&vnand_bus, &bus_attr);
	if (ret)
		printk("WARNING(os_vnand): bus_create_file error!\n");

	return 0;
}

void __exit ND_Exit(void) {
	bus_remove_file(&vnand_bus, &bus_attr);
}

module_init(ND_Init);
module_exit(ND_Exit);
MODULE_LICENSE("GPL");
