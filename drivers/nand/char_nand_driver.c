#define LINUX_KERNEL

#include "inc/nand_api.h"
#include "manager/inc/vNand.h"
#include "inc/vnandinfo.h"
#include "inc/nandinterface.h"
#include "inc/char_nand_driver.h"
#include "manager/inc/nanddebug.h"
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/fs.h>

struct char_nand_ops{
	VNandManager *vm;
	PPartArray ppa;
	NandInterface  *iface;
	struct NandInfo nand_info;
	unsigned short model;
	unsigned short status;
};
enum char_nand_ops_model{
	NAND_RECOVERY,
	NAND_DEBUG,
};
enum char_nand_ops_status{
	NAND_DRIVER_FREE,
	NAND_DRIVER_BUSY,
};
//static unsigned char *cmdbuf[3]={"CMD_INSTALL_PARTITION","CMD_NAND_ERASE1","CMD_NAND_ERASE2"};
static unsigned char argbuf[30];
extern struct vnand_operater v_nand_ops;  // manage/vnand/vnand.c
static struct cdev char_nand_dev;
static dev_t ndev;
static struct class *char_nand_class;

static struct char_nand_ops *nand_ops;
static unsigned char *databuf = NULL;  //use for nand_read and nand_write

static  long char_nand_unlocked_ioctl(struct file *fd, unsigned int cmd, unsigned long arg)
{
	int ret = 0,i=0;
	int partnum = 0;
	int ptcount = 0;
	struct nand_dug_msg *dug_msg = NULL;
	struct NandInfo *nand_info = &(nand_ops->nand_info);
	BlockList bl;

	if((nand_ops->model == NAND_DEBUG)){
		ptcount = nand_ops->vm->pt->ptcount -1; //sub nand_badblock_partition
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
					partnum = nand_info->partnum;
					ret = VN_OPERATOR(PageRead,&(nand_ops->ppa.ppt[partnum]),nand_info->id,0,nand_info->bytes,databuf);
					if(ret == nand_info->bytes){
						copy_to_user(nand_info->data, databuf, nand_info->bytes);
						ret = 0;
					}
					else
						ret = -1;
				}
				break;
			case CMD_NAND_DUG_WRITE:
				ret = copy_from_user(nand_info, (unsigned char *)arg, sizeof(struct NandInfo));
				if(!ret){
					copy_from_user(databuf, nand_info->data, nand_info->bytes);
					partnum = nand_info->partnum;
					ret = VN_OPERATOR(PageWrite,&(nand_ops->ppa.ppt[partnum]),nand_info->id,0,nand_info->bytes,databuf);
					if(ret == nand_info->bytes)
						ret = 0;
					else
						ret = -1;
				}
				break;
			case CMD_NAND_DUG_ERASE:
				ret = copy_from_user(nand_info, (unsigned char *)arg, sizeof(struct NandInfo));
				if(!ret){
					bl.startBlock = nand_info->id;
					bl.BlockCount = 1;
					bl.head.next = NULL;
					ret = VN_OPERATOR(MultiBlockErase,&(nand_ops->ppa.ppt[partnum]),&bl);
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
				for (i=0; i < nand_ops->ppa.ptcount; i ++) {
					printk("match partition: ppt[%d] = %s, ptname = %s\n", i, nand_ops->ppa.ppt[i].name, ptname);
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

				for (i=0; i < nand_ops->ppa.ppt[ptIndex].totalblocks; i++){
					bl.startBlock = i;
					bl.BlockCount = 1;
					bl.head.next = NULL;
					nand_ops->iface->iMultiBlockErase(&(nand_ops->ppa.ppt[ptIndex]),&bl);
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

	return ret != 0 ? -EFAULT : 0 ;
}

static ssize_t char_nand_write(struct file * fd, const char __user * pdata, size_t size, loff_t * pt)
{
	int ret = -1;

	if((nand_ops->model == NAND_DEBUG)){
		return -EFAULT;
	}
	memset(argbuf,0,30);
	if(copy_from_user(argbuf,pdata,size))
		return -EFAULT;
	if(strcmp(argbuf,"CMD_INSTALL_PARTITION"))
		return -EFAULT;
	nand_ops->model = NAND_DEBUG;
	Register_NandDriver(nand_ops->iface);
	nand_ops->vm =(VNandManager *)kmalloc(sizeof(VNandManager),GFP_KERNEL);
	if(!nand_ops->vm){
		ret =-1;
		goto nand_vm_kmalloc_failed;
	}
	ret = VN_OPERATOR(InitNand,nand_ops->vm);
	if(ret){
		ret = -1;
		goto nand_init_vm_failed;
	}
	databuf =(unsigned char *)kmalloc(nand_ops->vm->info.BytePerPage,GFP_KERNEL);
	if(!databuf){
		ret = -1;
		goto nand_kmalloc_databuf_failed;
	}
	printk("nand_manager install successful !!!\n");
	return size;
nand_init_vm_failed:
nand_kmalloc_databuf_failed:
	kfree(nand_ops->vm);
nand_vm_kmalloc_failed:
	return 0;
}
static int char_nand_open(struct inode *pnode,struct file *fd)
{
	if(nand_ops->status == NAND_DRIVER_FREE){
		nand_ops->status = NAND_DRIVER_BUSY;
		return 0;
	}
	return -EBUSY;
}
static int char_nand_close(struct inode *pnode,struct file *fd)
{
	nand_ops->status = NAND_DRIVER_FREE;
	return 0;
}
static const struct file_operations char_nand_driver_ops = {
	.owner	= THIS_MODULE,
	.open   = char_nand_open,
	.release = char_nand_close,
	.write   = char_nand_write,
	.unlocked_ioctl	= char_nand_unlocked_ioctl,
};

int Register_CharNandDriver(unsigned int interface,unsigned int partarray)
{
	PPartArray *ppa = (PPartArray *)partarray;
	if(!nand_ops)
		return -1;
	nand_ops->iface = (NandInterface *)interface;
	nand_ops->ppa.ptcount = ppa->ptcount;
	nand_ops->ppa.ppt = ppa->ppt;
	return 0;
}
static int __init char_nand_init(void)
{
	int ret=0;
	nand_ops = (struct char_nand_ops *)kmalloc(sizeof(struct char_nand_ops),GFP_KERNEL);
	if(!nand_ops){
		ret =-1;
		goto char_nand_kmalloc_failed;
	}
	nand_ops->model = NAND_RECOVERY;
	nand_ops->status = NAND_DRIVER_FREE;

	char_nand_class = class_create(THIS_MODULE,"char_nand_class");
	if(!char_nand_class){
		printk("%s  %d\n",__func__,__LINE__);
		return -1;
	}
	ret = alloc_chrdev_region(&ndev,0,1,"char_nand_dev");
	if(ret < 0){
		ret = -1;
		goto char_nand_alloc_chrdev_failed;
	}
	cdev_init(&char_nand_dev,&char_nand_driver_ops);
	ret = cdev_add(&char_nand_dev,ndev,1);
	if(ret < 0){
		ret = -1;
		goto char_nand_cdev_add_failed;
	}

	device_create(char_nand_class,NULL,ndev,NULL,"char_nand_dev");

	return 0;

char_nand_cdev_add_failed:
	unregister_chrdev_region(ndev,1);
char_nand_alloc_chrdev_failed:
	kfree(nand_ops);
	nand_ops = NULL;
char_nand_kmalloc_failed:
	return ret;
}

static void __exit char_nand_exit(void)
{
	kfree(nand_ops->vm);
	kfree(databuf);
	kfree(nand_ops);

	cdev_del(&char_nand_dev);
	unregister_chrdev_region(ndev,1);
}


module_init(char_nand_init);
module_exit(char_nand_exit);
MODULE_DESCRIPTION("JZ4780 char Nand driver");
MODULE_AUTHOR(" ingenic ");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("20120623");
