/**
 * test.c
 * jbbi (jbbi@ingenic.cn)
 *
 **/
#include <linux/major.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kthread.h>
#include <linux/blkdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/blk_types.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/genhd.h>
#include <linux/hdreg.h>

#include "nandmanagerinterface.h"
#include "bufflistmanager.h"

#define DEBUG

#define BLOCK_NAME 		"nand_block"
#define MAX_SEG_CNT 	64
#define MAX_MINORS		255
#define DISK_MINORS		16

struct __partition_info {
	int context;
	LPartition *pt;
};

struct __nand_disk {
	struct singlelist list;
	struct gendisk *disk;
	struct request_queue *queue;
	struct request *req;
	struct task_struct *q_thread;
	struct semaphore thread_sem;
	struct device *dev;
	struct device_attribute dattr;
	struct __partition_info *pinfo;
	SectorList *sl;
	int sl_context;
	unsigned int sl_len;
	int sectorsize;
	unsigned int segmentsize;
	spinlock_t queue_lock;
};

struct __nand_block {
	char *name;
	int major;
	int pm_handler;
	struct singlelist disk_list;
};

static struct __nand_block nand_block;

/*#################################################################*\
 *# dump
\*#################################################################*/
#define DBG_FUNC()	//printk("##### nand block debug #####: func = %s \n", __func__)

#ifdef DEBUG
struct driver_attribute drv_attr;
static void nand_disk_start(int data);
static ssize_t dbg_show(struct device_driver *driver, char *buf)
{
	DBG_FUNC();

	return 0;
}

static ssize_t dbg_store(struct device_driver *driver, const char *buf, size_t count)
{
	DBG_FUNC();

	nand_disk_start(0);

	return count;
}
#endif

static inline void dump_sectorlist(SectorList *top)
{
	struct singlelist *plist = NULL;
	SectorList *tmp = NULL;

	if (top) {
		singlelist_for_each(plist, &top->head) {
			tmp = singlelist_entry(plist, SectorList, head);
			printk("dump SectorList: sl = %p\n", tmp);
		}
	}
}

/*#################################################################*\
 *# request
\*#################################################################*/

static struct __nand_disk * get_ndisk_form_queue(const struct request_queue *q)
{
	struct singlelist *plist = NULL;
	struct __nand_disk *ndisk = NULL;

	DBG_FUNC();

	singlelist_for_each(plist, nand_block.disk_list.next) {
		ndisk = singlelist_entry(plist, struct __nand_disk, list);
		if (ndisk->queue == q)
			return ndisk;
	}
	return NULL;
}

/**
 * map a request to SectorList,
 * return number of sl entries setup.
 * this function refer to blk_rq_map_sg()
 * in blk-merge.c
 **/
static int nand_rq_map_sl(struct request_queue *q,
						  struct request *req,
						  SectorList **sllist,
						  int bufflist_context,
						  int sectorsize)
{
	struct bio_vec *bvec, *bvprv = NULL;
	struct req_iterator iter;
	SectorList *sl = NULL;
	unsigned int nsegs, cluster;
	unsigned int startSector;

	DBG_FUNC();

	nsegs = 0;
	startSector = blk_rq_pos(req);
	cluster = blk_queue_cluster(q);
	rq_for_each_segment(bvec, req, iter) {
		int nbytes = bvec->bv_len;
		if (bvprv && cluster) {
			if ((sl->sectorCount * sectorsize + nbytes) > queue_max_segment_size(q))
				goto new_segment;
			if (!BIOVEC_PHYS_MERGEABLE(bvprv, bvec))
				goto new_segment;
			if (!BIOVEC_SEG_BOUNDARY(q, bvprv, bvec))
				goto new_segment;

			sl->sectorCount += nbytes / sectorsize;
		} else {
		new_segment:
			if (!sl) {
				sl = (SectorList *)BuffListManager_getTopNode(bufflist_context, sizeof(*sl));
				if (!sl) {
					printk("ERROR: BuffListManager_getTopNode error!\n");
					return -ENOMEM;
				}
				*sllist = sl;
			} else {
				sl = (SectorList *)BuffListManager_getNextNode(bufflist_context, (void *)sl, sizeof(*sl));
				if (!sl) {
					printk("ERROR: BuffListManager_getNextNode error!\n");
					BuffListManager_freeAllList(bufflist_context, (void **)sllist, sizeof(*sl));
					*sllist = NULL;
					return -ENOMEM;
				}
			}

			sl->startSector = startSector;
			sl->sectorCount = nbytes / sectorsize;
			sl->pData = page_address(bvec->bv_page) + bvec->bv_offset;

			startSector += sl->sectorCount;
			nsegs ++;
		}
		bvprv = bvec;
	}

	return nsegs;
}

/* thread to handle request */
static int handle_req_thread(void *data)
{
	int err = 0;
	int ret = 0;
	struct request *req = NULL;
	struct __nand_disk *ndisk = NULL;
	struct request_queue *q = (struct request_queue *)data;

	ndisk = get_ndisk_form_queue(q);
	if (!ndisk || !ndisk->pinfo) {
		printk("can not get ndisk, ndisk = %p\n", ndisk);
		return -ENODEV;
	}

	down(&ndisk->thread_sem);
	while(1) {
		/* set thread state */
		spin_lock_irq(q->queue_lock);
		set_current_state(TASK_INTERRUPTIBLE);
		req = blk_fetch_request(q);
		ndisk->req = req;
		spin_unlock_irq(q->queue_lock);

		if (!req) {
			if (kthread_should_stop()) {
				set_current_state(TASK_RUNNING);
				break;
			}
			up(&ndisk->thread_sem);
			schedule();
			down(&ndisk->thread_sem);
			continue;
		}

		set_current_state(TASK_RUNNING);

	    while(req) {
			printk("%s: req = %p, start sector = %d, total = %d, buffer = %p\n",
				   (rq_data_dir(req) == READ)? "READ":"WRITE",
				   req, (int)blk_rq_pos(req), (int)blk_rq_sectors(req), req->buffer);

			/* make SectorList from request */
			ndisk->sl = NULL;
			ndisk->sl_len = nand_rq_map_sl(q, req, &ndisk->sl, ndisk->sl_context, ndisk->sectorsize);
			if (!ndisk->sl || (ndisk->sl_len < 0)) {
				printk("nand_rq_map_sl error, ndisk->sl_len = %d, ndisk->sl = %p\n",
					   ndisk->sl_len, ndisk->sl);
				break;
			}

			//dump_sectorlist(ndisk->sl);

			/* nandmanager read || write */
			if (rq_data_dir(req) == READ)
				ret = NandManger_read(ndisk->pinfo->context, ndisk->sl);
			else
				ret = NandManger_write(ndisk->pinfo->context, ndisk->sl);

			BuffListManager_freeAllList(ndisk->sl_context, (void **)(&ndisk->sl), sizeof(*ndisk->sl));

			if (ret < 0) {
				printk("NandManger_read/write error!\n");
				break;
			}

			if (!__blk_end_request(req, err, blk_rq_bytes(req)))
				break;
		}
	}
	up(&ndisk->thread_sem);

	return 0;
}

static void do_nand_request(struct request_queue *q)
{
	struct __nand_disk *ndisk = NULL;
	DBG_FUNC();

	ndisk = get_ndisk_form_queue(q);
	if (!ndisk || !ndisk->pinfo) {
		printk("can not get ndisk, ndisk = %p\n", ndisk);
		return;
	}

	/* wake up the handle thread to process the request */
	if (!ndisk->req)
		wake_up_process(ndisk->q_thread);
}

/*#################################################################*\
 *# block_device_operations
\*#################################################################*/
static int nand_disk_open(struct block_device *bdev, fmode_t mode)
{
	DBG_FUNC();

	return 0;
}

static int nand_disk_release(struct gendisk *disk, fmode_t mode)
{
	DBG_FUNC();

	return 0;
}

static int nand_disk_ioctl(struct block_device *bdev, fmode_t mode, unsigned cmd, unsigned long arg)
{
	DBG_FUNC();

	return 0;
}

static int nand_disk_compat_ioctl(struct block_device *bdev, fmode_t mode, unsigned cmd, unsigned long arg)
{
	DBG_FUNC();

	return 0;
}

static int nand_disk_direct_access(struct block_device *bdev, sector_t sector, void **kaddr, unsigned long *pfn)
{
	DBG_FUNC();

	return 0;
}

static int nand_disk_getgeo(struct block_device *bdev, struct hd_geometry *geo)
{
	DBG_FUNC();

	return 0;
}

static const struct block_device_operations nand_disk_fops =
{
	.owner					= THIS_MODULE,
	.open					= nand_disk_open,
	.release				= nand_disk_release,
	.ioctl					= nand_disk_ioctl,
	.compat_ioctl 			= nand_disk_compat_ioctl,
	.direct_access 			= nand_disk_direct_access,
	.getgeo					= nand_disk_getgeo,
};

/*#################################################################*\
 *# bus
\*#################################################################*/
static struct bus_type nand_block_bus = {
	.name		= "nbb",
};

/*#################################################################*\
 *# device_driver
\*#################################################################*/
static struct __nand_disk * get_ndisk_by_dev(const struct device *dev)
{
	struct singlelist *plist = NULL;
	struct __nand_disk *ndisk = NULL;

	DBG_FUNC();

	singlelist_for_each(plist, nand_block.disk_list.next) {
		ndisk = singlelist_entry(plist, struct __nand_disk, list);
		if (ndisk->dev == dev)
			return ndisk;
	}

	return NULL;
}

static int nand_block_probe(struct device *dev)
{
    int ret = -ENOMEM;
	static unsigned int cur_minor = 0;
	struct __nand_disk *ndisk = NULL;
	struct __partition_info *pinfo = (struct __partition_info *)dev->platform_data;

	DBG_FUNC();

	if (!pinfo || !pinfo->pt) {
		printk("ERROR(nand block): can not get partition info!\n");
		return -EFAULT;
	}

	if (cur_minor > MAX_MINORS - DISK_MINORS) {
		printk("ERROR(nand block): no enough minors left, can't create disk!\n");
		return -EFAULT;
	}

	ndisk = kzalloc(sizeof(struct __nand_disk), GFP_KERNEL);
	if (!ndisk) {
		printk("ERROR(nand block): alloc memory for ndisk error!\n");
		goto probe_err0;
	}

    ndisk->disk = alloc_disk(1);
    if (!ndisk->disk) {
		printk("ERROR(nand block): alloc_disk error!\n");
		goto probe_err1;
	}

	/* init queue */
	spin_lock_init(&ndisk->queue_lock);
	ndisk->queue = blk_init_queue(do_nand_request, &ndisk->queue_lock);
    if (!ndisk->queue) {
		printk("ERROR(nand block): blk_init_queue error!\n");
		goto probe_err2;
	}
	//blk_queue_max_segments(ndisk->queue, MAX_SEG_CNT);
	blk_queue_max_segment_size(ndisk->queue, pinfo->pt->segmentsize);

	ndisk->sl_context = BuffListManager_BuffList_Init();
	if (ndisk->sl_context == 0) {
		printk("ERROR(nand block): BuffListManager_BuffList_Init error!\n");
		goto probe_err3;
	}

	/* init ndisk */
	ndisk->sl_len = 0;
	ndisk->dev = dev;
	ndisk->pinfo = pinfo;
	ndisk->sectorsize = pinfo->pt->hwsector;
	ndisk->segmentsize = pinfo->pt->segmentsize;

    ndisk->disk->major = nand_block.major;
    ndisk->disk->first_minor = cur_minor;
	ndisk->disk->minors = DISK_MINORS;
    ndisk->disk->fops = &nand_disk_fops;
    sprintf(ndisk->disk->disk_name, "%s", pinfo->pt->name);
    ndisk->disk->queue = ndisk->queue;

	cur_minor += DISK_MINORS;

	/* add gendisk */
    add_disk(ndisk->disk);

	/* add ndisk to disk_list */
	singlelist_add(&nand_block.disk_list, &ndisk->list);

	/* init semaphore */
	sema_init(&ndisk->thread_sem, 1);

	/* create and start a thread to handle request */
	ndisk->q_thread = kthread_run(handle_req_thread,
								  (void *)ndisk->queue,
								  "%s_q_handler",
								  pinfo->pt->name);
	if (!ndisk->q_thread) {
		printk("ERROR(nand block): kthread_run error!\n");
		goto probe_err4;
	}

	/* set capacity */
	set_capacity(ndisk->disk, pinfo->pt->sectorCount);

	/* create file */
	sysfs_attr_init(&ndisk->dattr.attr);
	ndisk->dattr.attr.name = "info";
	ndisk->dattr.attr.mode = S_IRUGO | S_IWUSR;
	ret = device_create_file(ndisk->dev, &ndisk->dattr);
	if (ret)
		printk("WARNING(nand block): device_create_file error!\n");

    return 0;

probe_err4:
	del_gendisk(ndisk->disk);
	singlelist_del(&nand_block.disk_list, &ndisk->list);
	BuffListManager_BuffList_DeInit(ndisk->sl_context);
probe_err3:
	blk_cleanup_queue(ndisk->queue);
probe_err2:
	put_disk(ndisk->disk);
probe_err1:
	kfree(ndisk);
probe_err0:
	return ret;
}

static int nand_block_remove(struct device *dev)
{
	struct __nand_disk *ndisk = get_ndisk_by_dev(dev);

	DBG_FUNC();

	if (ndisk) {
		BuffListManager_BuffList_DeInit(ndisk->sl_context);
		del_gendisk(ndisk->disk);
		blk_cleanup_queue(ndisk->queue);
		put_disk(ndisk->disk);
		kfree(dev);
	}

	return 0;
}

static void nand_block_shutdown(struct device *dev)
{
	unsigned long flags;
	struct __nand_disk *ndisk = get_ndisk_by_dev(dev);
	struct request_queue *q = ndisk ? ndisk->queue : NULL;

	DBG_FUNC();

	if (q) {
		spin_lock_irqsave(q->queue_lock, flags);
		blk_stop_queue(q);
		spin_unlock_irqrestore(q->queue_lock, flags);
	}
}

static int nand_block_suspend(struct device *dev, pm_message_t state)
{
	unsigned long flags;
	struct __nand_disk *ndisk = get_ndisk_by_dev(dev);
	struct request_queue *q = ndisk ? ndisk->queue : NULL;

	DBG_FUNC();

	if (q) {
		spin_lock_irqsave(q->queue_lock, flags);
		blk_stop_queue(q);
		spin_unlock_irqrestore(q->queue_lock, flags);

		down(&ndisk->thread_sem);
	}

	return 0;
}

static int nand_block_resume(struct device *dev)
{
	unsigned long flags;
	struct __nand_disk *ndisk = get_ndisk_by_dev(dev);
	struct request_queue *q = ndisk ? ndisk->queue : NULL;

	DBG_FUNC();

	if (q) {
		up(&ndisk->thread_sem);

		spin_lock_irqsave(q->queue_lock, flags);
		blk_start_queue(q);
		spin_unlock_irqrestore(q->queue_lock, flags);
	}

	return 0;
}

struct device_driver nand_block_driver = {
	.name 		= "nand_block",
	.bus		= &nand_block_bus,
	.probe 		= nand_block_probe,
	.remove 	= nand_block_remove,
	.shutdown 	= nand_block_shutdown,
	.suspend 	= nand_block_suspend,
	.resume 	= nand_block_resume,
};

/*#################################################################*\
 *# start
\*#################################################################*/
static void nand_disk_start(int data)
{
	int ret = -EFAULT;
	int context = 0;
	LPartition *phead = NULL;
	LPartition *pt = NULL;
	struct device *dev = NULL;
	struct singlelist *plist = NULL;
	struct __partition_info *pinfo = NULL;

	DBG_FUNC();

	if (NandManger_getPartition(nand_block.pm_handler, &phead) || (!phead)) {
		printk("get NandManger partition error! phead = %p\n", phead);
		return;
	}

	singlelist_for_each(plist, &phead->head) {
		pt = singlelist_entry(plist, LPartition, head);
		if ((context = NandManger_open(nand_block.pm_handler, pt->name, pt->mode)) == 0) {
			printk("can not open NandManger %s, mode = %d\n",
				   pt->name, pt->mode);
			return;
		}

		/* init dev */
		dev = kzalloc(sizeof(struct device), GFP_KERNEL);
		if (!dev) {
			printk("can not alloc memory for device!\n");
			goto start_err0;
		}

		pinfo = kzalloc(sizeof(struct __partition_info), GFP_KERNEL);
		if (!pinfo) {
			printk("can not alloc memory for pinfo!\n");
			goto start_err1;
		}

		pinfo->context = context;
		pinfo->pt = pt;

		device_initialize(dev);
		dev->bus = &nand_block_bus;
		dev->platform_data = pinfo;
		dev_set_name(dev, "%s", pt->name);

		/* register dev */
		ret = device_add(dev);
		if (ret < 0) {
			printk("device_add error!\n");
			ret = -EFAULT;
			goto start_err2;
		}
	}

	return;

start_err2:
	kfree(pinfo);
start_err1:
	kfree(dev);
start_err0:
	NandManger_close(context);
	return;
}

/*#################################################################*\
 *# init our deinit
\*#################################################################*/
static int __init nand_block_init(void)
{
	int ret = -EBUSY;

	DBG_FUNC();

	nand_block.name = BLOCK_NAME;
	nand_block.disk_list.next = NULL;

	nand_block.major = register_blkdev(0, nand_block.name);
	if (nand_block.major <= 0) {
		printk("ERROR(nand block): register_blkdev error!\n");
		goto out_block;
	}

	if (bus_register(&nand_block_bus)) {
		printk("ERROR(nand block): bus_register error!\n");
		goto out_bus;
	}

	if (driver_register(&nand_block_driver)) {
		printk("ERROR(nand block): driver_register error!\n");
		goto out_driver;
	}

#ifdef DEBUG
	drv_attr.show = dbg_show;
	drv_attr.store = dbg_store;
	sysfs_attr_init(&drv_attr.attr);
	drv_attr.attr.name = "dbg";
	drv_attr.attr.mode = S_IRUGO | S_IWUSR;
	ret = driver_create_file(&nand_block_driver, &drv_attr);
	if (ret)
		printk("WARNING(nand block): driver_create_file error!\n");
#endif

	if (((nand_block.pm_handler = NandManger_Init())) == 0) {
		printk("ERROR(nand block): NandManger_Init error!\n");
		goto out_init;
	}

	NandManger_startNotify(nand_block.pm_handler, nand_disk_start, 0);

	return 0;

out_init:
	driver_unregister(&nand_block_driver);
out_driver:
	bus_unregister(&nand_block_bus);
out_bus:
    unregister_blkdev(nand_block.major, nand_block.name);
out_block:
	return ret;
}

static void __exit nand_block_exit(void)
{
	DBG_FUNC();

	NandManger_Deinit(nand_block.pm_handler);
	driver_unregister(&nand_block_driver);
	bus_unregister(&nand_block_bus);
	unregister_blkdev(nand_block.major, nand_block.name);
}

module_init(nand_block_init);
module_exit(nand_block_exit);
MODULE_LICENSE("GPL");
