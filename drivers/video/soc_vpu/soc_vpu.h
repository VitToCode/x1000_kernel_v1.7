#ifndef __CHANNEL_H__
#define __CHANNEL_H__

#include <linux/types.h>
#include <linux/completion.h>
#include <linux/spinlock.h>
#include <linux/miscdevice.h>

#include "channel_vpu.h"

extern int vpu_register(struct list_head *vlist);
extern int vpu_unregister(struct list_head *vlist);

enum channel_phase {
	INIT_CHANNEL	= -1,
	REQUEST_CHANNEL = 0,
	FLUSH_CACHE,
	RUN_CHANNEL,
	RELEASE_CHANNEL,
	CHANNEL_FINISHED,
};

struct channel_tlb_vaddrmanager {
	struct list_head vaddr_entry;
	unsigned int vaddr;
	unsigned int size;
};

struct channel_tlb_pidmanager {
	pid_t pid;
	unsigned int tlbbase;
	struct list_head vaddr_list;
};


struct channel_list {
	struct list_head		list;
	enum channel_phase		phase;
	spinlock_t			slock;
	struct task_struct		*task;		/* current task structure */
	bool				tlb_flag;
	struct channel_tlb_pidmanager	*tlb_pidmanager;
};

struct free_channel_list {
	struct list_head	fclist_head;
	struct completion	cdone;		/* channel complete */
	spinlock_t		slock;		/* done lock */
};

enum vpu_phase {
	INIT_VPU = -1,
	REQUEST_VPU = 0,
	SET_VPU_PARAM,
	RUN_VPU,
	RELASE_VPU,
	VPU_FINISHED,
};

struct vpu_list {
	struct list_head	list;		/* the list of this struct list */
	struct list_head	*vlist;		/* the list of vpu list to be register */
	enum vpu_phase		phase;
	int			user_cnt;	/* the child vpu use times */
	struct task_struct	*task;		/* current task structure */
	spinlock_t		slock;
};

struct free_vpu_list {
	struct list_head	fvlist_head;
	struct completion	vdone;		/* channel complete */
	spinlock_t		slock;		/* done lock */
};

struct soc_channel {
	struct list_head	*fclist_head;	/* free channel list head */
	struct list_head	*fvlist_head;	/* free vpu list head */
	struct miscdevice	mdev;		/* miscdevice */
	spinlock_t		cnt_slock;		/* done lock */
	int			user_cnt;
};

struct vpu_ops {
	struct module *owner;
	long (*open)(struct device *dev);
	long (*release)(struct device *dev);
	long (*start_vpu)(struct device *dev, const struct channel_node * const cnode);
	long (*wait_complete)(struct device *dev, struct channel_node * const cnode);
	long (*reset)(struct device *dev);
	long (*suspend)(struct device *dev, pm_message_t state, const int use_count);
	long (*resume)(struct device *dev, const int use_count);
};

struct vpu {
	struct list_head	vlist;
	int			id;
	struct device		*dev;
	struct vpu_ops		*ops;
};

#endif //__CHANNEL_H__
