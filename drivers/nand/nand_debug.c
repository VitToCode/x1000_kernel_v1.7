
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/fs.h>

#include "nand_api.h"
#include "nandinterface.h"
#include "nand_debug.h"
#include "nanddebug.h"

#define DBG_FUNC() //printk("##### nand char debug #####: func = %s \n", __func__)

struct nand_debug_ops{
	PPartArray ppa;
	NandInterface  *iface;
	struct NandInfo nand_info;
	unsigned short status;
};

enum nand_debug_ops_status{
	NAND_DRIVER_FREE,
	NAND_DRIVER_BUSY,
};

static dev_t ndev;
static struct cdev nand_debug_dev;
static struct class *nand_debug_class;
static struct nand_debug_ops *nand_ops;
extern void vNand_Lock(void);
extern void vNand_unLock(void);

static long nand_debug_unlocked_ioctl(struct file *fd, unsigned int cmd, unsigned long arg)
{
	int ret = 0,i=0;
	int partnum = 0;
	int ptcount = nand_ops->ppa.ptcount -1; //sub nand_badblock_partition
	struct nand_dug_msg *dug_msg = NULL;
	struct NandInfo *nand_info = &(nand_ops->nand_info);
	static unsigned char *databuf = NULL;
	BlockList bl;

	DBG_FUNC();

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
			vNand_Lock();
			ret = nand_ops->iface->iPageRead(&(nand_ops->ppa.ppt[partnum]), nand_info->id,
											 0, nand_info->bytes, databuf);
			vNand_unLock();
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
			vNand_Lock();
			ret = nand_ops->iface->iPageWrite(&(nand_ops->ppa.ppt[partnum]), nand_info->id,
											  0, nand_info->bytes, databuf);
			vNand_unLock();
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
			vNand_Lock();
			ret = nand_ops->iface->iMultiBlockErase(&(nand_ops->ppa.ppt[partnum]), &bl);
			vNand_unLock();
		}
		break;
	default:
		printk("nand_dug_driver: the parameter is wrong!\n");
		ret = -1;
		break;
	}

	return ret != 0 ? -EFAULT : 0 ;
}

static ssize_t nand_debug_write(struct file * fd, const char __user * pdata, size_t size, loff_t * pt)
{
	return size;
}

static int nand_debug_open(struct inode *pnode,struct file *fd)
{
	DBG_FUNC();
	if(nand_ops->status == NAND_DRIVER_FREE){
		nand_ops->status = NAND_DRIVER_BUSY;
		return 0;
	}
	return -EBUSY;
}

static int nand_debug_close(struct inode *pnode,struct file *fd)
{
	DBG_FUNC();
	nand_ops->status = NAND_DRIVER_FREE;
	return 0;
}

static const struct file_operations nand_debug_ops = {
	.owner	= THIS_MODULE,
	.open   = nand_debug_open,
	.release = nand_debug_close,
	.write   = nand_debug_write,
	.unlocked_ioctl	= nand_debug_unlocked_ioctl,
};

int Register_NandDebugDriver(unsigned int interface,unsigned int partarray)
{
	int ret;
	PPartArray *ppa = (PPartArray *)partarray;

	DBG_FUNC();
	nand_ops->iface = (NandInterface *)interface;
	nand_ops->ppa.ptcount = ppa->ptcount;
	nand_ops->ppa.ppt = ppa->ppt;

	ret = alloc_chrdev_region(&ndev,0,1,"nand_debug");
	if(ret < 0){
		ret = -1;
		printk("DEBUG : nand_debug_driver  alloc_chrdev_region\n");
		goto alloc_chrdev_failed;
	}
	cdev_init(&nand_debug_dev, &nand_debug_ops);
	ret = cdev_add(&nand_debug_dev,ndev,1);
	if(ret < 0){
		ret = -1;
		printk("DEBUG : nand_debug_driver cdev_add error\n");
		goto cdev_add_failed;
	}

	device_create(nand_debug_class,NULL,ndev,NULL,"nand_debug");

	printk("nand char register ok!!!\n");
	return 0;

cdev_add_failed:
	unregister_chrdev_region(ndev,1);
alloc_chrdev_failed:
	kfree(nand_ops);
	nand_ops = NULL;

	return ret;
}

static int __init nand_debug_init(void)
{
	int ret=0;

	DBG_FUNC();
	nand_ops = (struct nand_debug_ops *)kmalloc(sizeof(struct nand_debug_ops),GFP_KERNEL);
	if(!nand_ops){
		printk("DEBUG : nand_debug_kmalloc_failed\n");
		ret =-1;
		goto nand_debug_kmalloc_failed;
	}
	nand_ops->status = NAND_DRIVER_FREE;

	nand_debug_class = class_create(THIS_MODULE,"nand_debug_class");
	if(!nand_debug_class){
		printk("%s  %d\n",__func__,__LINE__);
		return -1;
	}

	return 0;

nand_debug_kmalloc_failed:
	return ret;
}

static void __exit nand_debug_exit(void)
{
	DBG_FUNC();
	kfree(nand_ops);
	cdev_del(&nand_debug_dev);
	unregister_chrdev_region(ndev,1);
}

module_init(nand_debug_init);
module_exit(nand_debug_exit);
MODULE_DESCRIPTION("JZ4780 debug Nand driver");
MODULE_AUTHOR(" ingenic ");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("20120623");
