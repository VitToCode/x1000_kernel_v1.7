
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/fs.h>

#include "nand_api.h"
#include "nandinterface.h"
#include "nand_char.h"
#include "nanddebug.h"

#define DBG_FUNC() printk("##### nand char debug #####: func = %s \n", __func__)

struct nand_char_ops{
	PPartArray ppa;
	NandInterface  *iface;
	struct NandInfo nand_info;
	unsigned short mode;
	unsigned short status;
};

enum nand_char_ops_mode{
	NAND_RECOVERY,
	NAND_DEBUG,
};

enum nand_char_ops_status{
	NAND_DRIVER_FREE,
	NAND_DRIVER_BUSY,
};

static dev_t ndev;
static struct cdev nand_char_dev;
static struct class *nand_char_class;
static struct nand_char_ops *nand_ops;
extern void vNand_Lock(void);
extern void vNand_unLock(void);

static long nand_char_unlocked_ioctl(struct file *fd, unsigned int cmd, unsigned long arg)
{
	int ret = 0,i=0;
	int partnum = 0;
	int ptcount = 0;
	struct nand_dug_msg *dug_msg = NULL;
	struct NandInfo *nand_info = &(nand_ops->nand_info);
	static unsigned char *databuf = NULL;
	BlockList bl;

	DBG_FUNC();
	vNand_Lock();
	if((nand_ops->mode == NAND_DEBUG)){
		ptcount = nand_ops->ppa.ptcount -1; //sub nand_badblock_partition
		switch(cmd){
		case CMD_GET_NAND_PTC:
			put_user(ptcount,(int *)arg);
			break;
		case CMD_GET_NAND_MSG:
			if(ptcount > 0){
				dug_msg = kmalloc(ptcount * sizeof(struct nand_dug_msg),GFP_KERNEL);
				if(dug_msg){
					for(i = 0; i < ptcount; i++){
						strcpy(dug_msg[i].name,nand_ops->ppa.ppt[i].name);
						dug_msg[i].byteperpage = nand_ops->ppa.ppt[i].byteperpage;
						dug_msg[i].pageperblock = nand_ops->ppa.ppt[i].pageperblock;
						dug_msg[i].totalblocks = nand_ops->ppa.ppt[i].totalblocks;
					}
					ret = copy_to_user((unsigned char *)arg ,(unsigned char *)dug_msg,
									   ptcount * sizeof(struct nand_dug_msg));
					kfree(dug_msg);
				}
			}
			break;
		case CMD_NAND_DUG_READ:
			ret = copy_from_user(nand_info, (unsigned char *)arg, sizeof(struct NandInfo));
			if(!ret){
				databuf =(unsigned char *)kmalloc(nand_info->bytes, GFP_KERNEL);
				if(!databuf) {
					ret = -1;
					break;
				}
				partnum = nand_info->partnum;
				ret = nand_ops->iface->iPageRead(&(nand_ops->ppa.ppt[partnum]), nand_info->id,
												 0, nand_info->bytes, databuf);
				if(ret == nand_info->bytes){
					copy_to_user(nand_info->data, databuf, nand_info->bytes);
					ret = 0;
				}
				else
					ret = -1;

				kfree(databuf);
			}
			break;
		case CMD_NAND_DUG_WRITE:
			ret = copy_from_user(nand_info, (unsigned char *)arg, sizeof(struct NandInfo));
			if(!ret){
				databuf =(unsigned char *)kmalloc(nand_info->bytes, GFP_KERNEL);
				if(!databuf) {
					ret = -1;
					break;
				}
				copy_from_user(databuf, nand_info->data, nand_info->bytes);
				partnum = nand_info->partnum;
				ret = nand_ops->iface->iPageWrite(&(nand_ops->ppa.ppt[partnum]), nand_info->id,
												  0, nand_info->bytes, databuf);
				if(ret == nand_info->bytes)
					ret = 0;
				else
					ret = -1;
				kfree(databuf);
			}
			break;
		case CMD_NAND_DUG_ERASE:
			ret = copy_from_user(nand_info, (unsigned char *)arg, sizeof(struct NandInfo));
			if(!ret){
				bl.startBlock = nand_info->id;
				bl.BlockCount = 1;
				bl.head.next = NULL;
				ret = nand_ops->iface->iMultiBlockErase(&(nand_ops->ppa.ppt[partnum]), &bl);
			}
			break;
		default:
			printk("nand_dug_driver: the parameter is wrong!\n");
			ret = -1;
			break;
		}
	}else{
		switch(cmd){
		case CMD_PARTITION_ERASE: {
			char ptname[128];
			int ptIndex = -1;
			ret = copy_from_user(&ptname, (unsigned char *)arg, 128);
			if (!ret) {
				for (i=0; i < nand_ops->ppa.ptcount; i++) {
					printk("match partition: ppt[%d] = [%s], ptname = [%s]\n", i, nand_ops->ppa.ppt[i].name, ptname);
					if (!strcmp(nand_ops->ppa.ppt[i].name, ptname)) {
						ptIndex = i;
						break;
					}
				}

				if (i == nand_ops->ppa.ptcount) {
					printk("ERROR: can't find partition %s\n", ptname);
					ret = -1;
					break;
				}

				printk("erase nand partition %s\n", nand_ops->ppa.ppt[ptIndex].name);
				for (i=0; i < nand_ops->ppa.ppt[ptIndex].totalblocks; i++){
					bl.startBlock = i;
					bl.BlockCount = 1;
					bl.head.next = NULL;
					ret = nand_ops->iface->iMultiBlockErase(&(nand_ops->ppa.ppt[ptIndex]),&bl);
					if (ret != 0) {
						ret = nand_ops->iface->iMarkBadBlock(&(nand_ops->ppa.ppt[ptIndex]), bl.startBlock);
						if (ret != 0) {
							printk("%s: line:%d, nand mark badblock error, blockID = %d\n",
								   __func__, __LINE__, bl.startBlock);
						}
					}
				}
			}
			break;
		}
		case CMD_ERASE_ALL: {
			int j = 0;
			for (i = 0; i < nand_ops->ppa.ptcount; i ++) {
				printk("erase nand partition %s\n", nand_ops->ppa.ppt[i].name);
				for (j = 0; j < nand_ops->ppa.ppt[i].totalblocks; j++) {
					bl.startBlock = j;
					bl.BlockCount = 1;
					bl.head.next = NULL;
					ret = nand_ops->iface->iMultiBlockErase(&(nand_ops->ppa.ppt[i]), &bl);
					if (ret != 0) {
						ret = nand_ops->iface->iMarkBadBlock(&(nand_ops->ppa.ppt[i]), bl.startBlock);
						if (ret != 0) {
							printk("%s: line:%d, nand mark badblock error, blockID = %d\n",
								   __func__, __LINE__, bl.startBlock);
						}
					}
				}
			}
			break;
		}
		default:
			printk("nand_dug_driver: the parameter is wrong!\n");
			ret = -1;
			break;
		}
	}
	vNand_unLock();
	return ret != 0 ? -EFAULT : 0 ;
}

static ssize_t nand_char_write(struct file * fd, const char __user * pdata, size_t size, loff_t * pt)
{
	int copysize;
	char argbuf[128];
	char *cmd_install = "CMD_INSTALL_PARTITION";

	DBG_FUNC();
	if((nand_ops->mode == NAND_DEBUG)){
		printk("%s, line:%d, mode error, mode = %d\n", __func__, __LINE__, nand_ops->mode);
		return -EFAULT;
	}

	if (size > 128)
		copysize = 128;
	else
		copysize = size;

	if(copy_from_user(argbuf, pdata, copysize)) {
		printk("%s, line:%d, copy from user error, copysize = %d\n", __func__, __LINE__, copysize);
		return -EFAULT;
	}

	if (strcmp(argbuf, cmd_install)) {
		printk("%s, cmd error, cmd [%s] len[%d], need [%s] len[%d]\n",
			   __func__, argbuf, strlen(argbuf), cmd_install, strlen(cmd_install));
		return -EFAULT;
	}

	Register_NandDriver(nand_ops->iface);

	nand_ops->mode = NAND_DEBUG;

	printk("nand_manager install successful !!!\n");
	return size;
}

static int nand_char_open(struct inode *pnode,struct file *fd)
{
	DBG_FUNC();
	if(nand_ops->status == NAND_DRIVER_FREE){
		nand_ops->status = NAND_DRIVER_BUSY;
		return 0;
	}
	return -EBUSY;
}

static int nand_char_close(struct inode *pnode,struct file *fd)
{
	DBG_FUNC();
	nand_ops->status = NAND_DRIVER_FREE;
	return 0;
}

static const struct file_operations nand_char_ops = {
	.owner	= THIS_MODULE,
	.open   = nand_char_open,
	.release = nand_char_close,
	.write   = nand_char_write,
	.unlocked_ioctl	= nand_char_unlocked_ioctl,
};

int Register_CharNandDriver(unsigned int interface,unsigned int partarray)
{
	int ret;
	PPartArray *ppa = (PPartArray *)partarray;

	DBG_FUNC();
	nand_ops->iface = (NandInterface *)interface;
	nand_ops->ppa.ptcount = ppa->ptcount;
	nand_ops->ppa.ppt = ppa->ppt;

	ret = alloc_chrdev_region(&ndev,0,1,"nand_char");
	if(ret < 0){
		ret = -1;
		printk("DEBUG : nand_char_driver  alloc_chrdev_region\n");
		goto alloc_chrdev_failed;
	}
	cdev_init(&nand_char_dev, &nand_char_ops);
	ret = cdev_add(&nand_char_dev,ndev,1);
	if(ret < 0){
		ret = -1;
		printk("DEBUG : nand_char_driver cdev_add error\n");
		goto cdev_add_failed;
	}

	device_create(nand_char_class,NULL,ndev,NULL,"nand_char");

	printk("nand char register ok!!!\n");
	return 0;

cdev_add_failed:
	unregister_chrdev_region(ndev,1);
alloc_chrdev_failed:
	kfree(nand_ops);
	nand_ops = NULL;

	return ret;
}

static int __init nand_char_init(void)
{
	int ret=0;

	DBG_FUNC();
	nand_ops = (struct nand_char_ops *)kmalloc(sizeof(struct nand_char_ops),GFP_KERNEL);
	if(!nand_ops){
		printk("DEBUG : nand_char_kmalloc_failed\n");
		ret =-1;
		goto nand_char_kmalloc_failed;
	}
	nand_ops->mode = NAND_RECOVERY;
	nand_ops->status = NAND_DRIVER_FREE;

	nand_char_class = class_create(THIS_MODULE,"nand_char_class");
	if(!nand_char_class){
		printk("%s  %d\n",__func__,__LINE__);
		return -1;
	}

	return 0;

nand_char_kmalloc_failed:
	return ret;
}

static void __exit nand_char_exit(void)
{
	DBG_FUNC();
	kfree(nand_ops);
	cdev_del(&nand_char_dev);
	unregister_chrdev_region(ndev,1);
}

module_init(nand_char_init);
module_exit(nand_char_exit);
MODULE_DESCRIPTION("JZ4780 char Nand driver");
MODULE_AUTHOR(" ingenic ");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("20120623");
