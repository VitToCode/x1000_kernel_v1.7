#define LINUX_KERNEL

#include "inc/nand_api.h"
#include "manager/inc/vNand.h"
#include "inc/vnandinfo.h"
#include "inc/nand_driver_debug.h"
#include "manager/inc/nanddebug.h"
#include <linux/cdev.h>
#include <linux/module.h>
#include <linux/fs.h>


extern struct vnand_operater v_nand_ops;  // manage/vnand/vnand.c
static struct cdev nand_dug_dev;
static dev_t ndev;

static VNandManager *nand_dug_vnandmanager;
static PPartition *nand_pt;
static struct NandInfo nand_ops_info;
static unsigned char *databuf = NULL;  //use for nand_read and nand_write

static  long nand_dug_unlocked_ioctl(struct file *fd, unsigned int cmd, unsigned long arg)
{
        int ret = 0,i=0;
        int partnum = 0; 
        int ptcount = nand_dug_vnandmanager->pt->ptcount -1; //sub nand_badblock_partition
        struct nand_dug_msg *dug_msg = NULL;
        BlockList bl;

        switch(cmd){
                case GET_NAND_PTC:
                        put_user(ptcount,(int *)arg);
                        break;
                case GET_NAND_MSG:
                        if(ptcount > 0){
                                dug_msg = kmalloc(ptcount * sizeof(struct nand_dug_msg),GFP_KERNEL);
                                if(dug_msg){
                                        for(i = 0; i < ptcount; i++){
                                                strcpy(dug_msg[i].name,nand_pt[i].name);
                                                dug_msg[i].byteperpage = nand_pt[i].byteperpage;
                                                dug_msg[i].pageperblock = nand_pt[i].pageperblock;
                                                dug_msg[i].totalblocks = nand_pt[i].totalblocks;
                                        }
                                        ret = copy_to_user((unsigned char *)arg ,(unsigned char *)dug_msg, 
                                                                                ptcount * sizeof(struct nand_dug_msg));
                                        kfree(dug_msg);
                                }
                        }
                        break;
                case NAND_DUG_READ:
                        ret = copy_from_user(&nand_ops_info, (unsigned char *)arg, sizeof(struct NandInfo));
                        if(!ret){
                                partnum = nand_ops_info.partnum;
	                        ret = VN_OPERATOR(PageRead,&nand_pt[partnum],nand_ops_info.id,0,nand_ops_info.bytes,databuf);
                                if(ret == nand_ops_info.bytes){
                                        copy_to_user(nand_ops_info.data, databuf, nand_ops_info.bytes);
                                        ret = 0;
                                }
                                else
                                        ret = -1;
                        }
                        break;
                case NAND_DUG_WRITE:
                        ret = copy_from_user(&nand_ops_info, (unsigned char *)arg, sizeof(struct NandInfo));
                        if(!ret){
                                copy_from_user(databuf, nand_ops_info.data, nand_ops_info.bytes);
                                partnum = nand_ops_info.partnum;
	                        ret = VN_OPERATOR(PageWrite,&nand_pt[partnum],nand_ops_info.id,0,nand_ops_info.bytes,databuf);
                                if(ret == nand_ops_info.bytes)
                                        ret = 0;
                                else
                                        ret = -1;
                        }
                        break;
                case NAND_DUG_ERASE:
                        ret = copy_from_user(&nand_ops_info, (unsigned char *)arg, sizeof(struct NandInfo));
                        if(!ret){
                                bl.startBlock = nand_ops_info.id;
                                bl.BlockCount = 1;
                                bl.head.next = NULL;
	                        ret = VN_OPERATOR(MultiBlockErase,&nand_pt[partnum],&bl);
                                if(ret == nand_ops_info.bytes)
                                        ret = 0;
                                else
                                        ret = -1;
                        }
                        break;
                default:
                        printk("nand_dug_driver: the parameter is wrong!\n");
                        ret = -1;
                        break;
        }
        return ret != 0 ? -1 : 0 ;
}
static const struct file_operations nand_dug_ops = {
	.owner	= THIS_MODULE,
	.unlocked_ioctl	= nand_dug_unlocked_ioctl,
};

static int __init nand_dug_init(void)
{
        int ret=0;
        nand_dug_vnandmanager =(VNandManager *)kmalloc(sizeof(VNandManager),GFP_KERNEL);
        if(!nand_dug_vnandmanager){
                ret =-1;
                goto nand_dug_kmalloc_failed;
        }
        ret =VN_OPERATOR(InitNand,nand_dug_vnandmanager);
       if(ret != 0){
               ret = -1;
               goto nand_dug_initnand_failed;
       }
         nand_pt = nand_dug_vnandmanager->pt->ppt;

       databuf =(unsigned char *)kmalloc(nand_dug_vnandmanager->info.BytePerPage,GFP_KERNEL);
       if(!databuf){
               ret = -1;
               goto nand_dug_kmalloc_databuf_failed;
       }
       ret = alloc_chrdev_region(&ndev,0,1,"nand_debug_dev");
       if(ret < 0){
               ret = -1;
               goto nand_dug_alloc_chrdev_failed;
       }
       cdev_init(&nand_dug_dev,&nand_dug_ops);
       ret = cdev_add(&nand_dug_dev,ndev,1);
       if(ret < 0){
               ret = -1;
               goto nand_dug_cdev_add_failed;
       }
       return 0;

nand_dug_cdev_add_failed:
       unregister_chrdev_region(ndev,1);
nand_dug_alloc_chrdev_failed:
       kfree(databuf);
       databuf = NULL;
nand_dug_kmalloc_databuf_failed:
nand_dug_initnand_failed:
       kfree(nand_dug_vnandmanager);
       nand_dug_vnandmanager=NULL;
nand_dug_kmalloc_failed:
       return ret;
}

static void __exit nand_dug_exit(void)
{
        kfree(nand_dug_vnandmanager);
        kfree(databuf);

        cdev_del(&nand_dug_dev);
        unregister_chrdev_region(ndev,1);
}


module_init(nand_dug_init);
module_exit(nand_dug_exit);
MODULE_DESCRIPTION("JZ4780 Nand dug driver");
MODULE_AUTHOR(" ingenic ");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("20120623");
