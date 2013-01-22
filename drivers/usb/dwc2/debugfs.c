#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/ptrace.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/uaccess.h>

#include <linux/usb/ch9.h>

#include "core.h"
#include "gadget.h"
#include "debug.h"

#ifdef CONFIG_USB_DWC2_REQ_TRACER
struct req_trace_entry {
	u8	 epnum;
	u8	 is_in;
	u8	 stage;
	u8	 cpuid;
	void	*req;
	u32	 dma_addr;
	int	 len;
	int	 actual;
	int	 status;
	u64	 timestamp;
};

static struct req_trace_entry *req_trace_head = (struct req_trace_entry *)(192 * 1024 * 1024 | 0xa0000000);
static int req_trace_max_entry = ((256 - 192) * 1024 * 1024 / sizeof(struct req_trace_entry));

static int req_trace_curr_idx = 0;
static int req_trace_overflow = 0;
static int req_trace_enable = 0;

void dwc2_req_trace_record(struct dwc2_request *req, u8 stage) {
	struct req_trace_entry *entry;

	if (!req_trace_enable)
		return;

	entry = req_trace_head + req_trace_curr_idx;
	entry->epnum = req->dwc2_ep->number;
	entry->is_in = req->dwc2_ep->is_in;
	entry->stage = stage;
	entry->req = req;
	entry->dma_addr = req->next_dma_addr;
	entry->len = req->request.length;
	entry->actual = req->request.actual;
	entry->status = req->request.status;
	entry->cpuid = smp_processor_id();
	entry->timestamp = cpu_clock(entry->cpuid);

	req_trace_curr_idx++;
	if (req_trace_curr_idx >= req_trace_max_entry) {
		req_trace_curr_idx = 0;
		req_trace_overflow = 1;
	}
}

static void dwc2_req_trace_print_entry(struct req_trace_entry *entry) {
	printk("[%llu] CPU%d: ep%d-%s: req = 0x%p dma_addr = 0x%08x len = %d actual = %d status = %d\n",
		entry->timestamp, entry->cpuid,
		entry->epnum, entry->is_in ? "IN" : "OUT",
		entry->req, entry->dma_addr, entry->len, entry->actual, entry->status);
}

static void dwc2_req_trace_print(void) {
	int begin_idx = 0;
	int end_idx = req_trace_curr_idx;
	int num = end_idx;
	int i, idx;

	if (!req_trace_enable) {
		printk("===>request trace disabled\n");
		return;
	}

	if (req_trace_curr_idx == 0) {
		if (!req_trace_overflow) {
			printk("====>no trace entry!\n");
			return;
		}

		end_idx = req_trace_max_entry - 1;
	}

	if (req_trace_overflow) {
		begin_idx = req_trace_curr_idx;
		num = req_trace_max_entry;
	}

	idx = begin_idx;
	for (i = 0; i < num; i++) {
		dwc2_req_trace_print_entry(req_trace_head + idx);
		idx++;
		if (idx >= req_trace_max_entry)
			idx = 0;
	}
}

static int dwc2_req_trace_show(struct seq_file *s, void *unused)
{
	struct dwc2	*dwc = s->private;
	unsigned long	 flags;

	spin_lock_irqsave(&dwc->lock, flags);
	dwc2_req_trace_print();
	spin_unlock_irqrestore(&dwc->lock, flags);

	return 0;
}

static int dwc2_req_trace_open(struct inode *inode, struct file *file)
{
	return single_open(file, dwc2_req_trace_show, inode->i_private);
}

static ssize_t dwc2_req_trace_write(struct file *file,
			const char __user *ubuf, size_t count, loff_t *ppos)
{
	//struct seq_file	*s   = file->private_data;
	//struct dwc2	*dwc = s->private;
	char		 buf[32];

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	req_trace_curr_idx = 0;
	req_trace_overflow = 0;
	req_trace_enable = simple_strtol(buf, NULL, 0);

	return count;
}

static const struct file_operations dwc2_req_trace_fops = {
	.open			= dwc2_req_trace_open,
	.write			= dwc2_req_trace_write,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};
#endif	/* CONFIG_USB_DWC2_REQ_TRACER */

static int dwc2_mode_show(struct seq_file *s, void *unused)
{
#if 0
	struct dwc2		*dwc = s->private;
	unsigned long		flags;
	u32			reg;

	spin_lock_irqsave(&dwc->lock, flags);
	reg = readl(dwc->regs, DWC2_GCTL);
	spin_unlock_irqrestore(&dwc->lock, flags);

	switch (DWC2_GCTL_PRTCAP(reg)) {
	case DWC2_GCTL_PRTCAP_HOST:
		seq_printf(s, "host\n");
		break;
	case DWC2_GCTL_PRTCAP_DEVICE:
		seq_printf(s, "device\n");
		break;
	case DWC2_GCTL_PRTCAP_OTG:
		seq_printf(s, "OTG\n");
		break;
	default:
		seq_printf(s, "UNKNOWN %08x\n", DWC2_GCTL_PRTCAP(reg));
	}
#endif

	return 0;
}

static int dwc2_mode_open(struct inode *inode, struct file *file)
{
	return single_open(file, dwc2_mode_show, inode->i_private);
}

static ssize_t dwc2_mode_write(struct file *file,
			const char __user *ubuf, size_t count, loff_t *ppos)
{
#if 0
	struct seq_file		*s = file->private_data;
	struct dwc2		*dwc = s->private;
	unsigned long		flags;
	u32			mode = 0;
	char			buf[32];

	if (copy_from_user(&buf, ubuf, min_t(size_t, sizeof(buf) - 1, count)))
		return -EFAULT;

	if (!strncmp(buf, "host", 4))
		mode |= DWC2_GCTL_PRTCAP_HOST;

	if (!strncmp(buf, "device", 6))
		mode |= DWC2_GCTL_PRTCAP_DEVICE;

	if (!strncmp(buf, "otg", 3))
		mode |= DWC2_GCTL_PRTCAP_OTG;

	if (mode) {
		spin_lock_irqsave(&dwc->lock, flags);
		dwc2_set_mode(dwc, mode);
		spin_unlock_irqrestore(&dwc->lock, flags);
	}
#endif
	return count;
}

static const struct file_operations dwc2_mode_fops = {
	.open			= dwc2_mode_open,
	.write			= dwc2_mode_write,
	.read			= seq_read,
	.llseek			= seq_lseek,
	.release		= single_release,
};

int dwc2_debugfs_init(struct dwc2 *dwc)
{
#if 0
	struct dentry		*root;
	struct dentry		*file;
	int			ret;

	root = debugfs_create_dir(dev_name(dwc->dev), NULL);
	if (!root) {
		ret = -ENOMEM;
		goto err0;
	}

	dwc->root = root;

	file = debugfs_create_file("mode", S_IRUGO | S_IWUSR, root,
				dwc, &dwc2_mode_fops);
	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}

#ifdef CONFIG_USB_DWC2_REQ_TRACER
	file = debugfs_create_file("req_trace", S_IWUSR, root,
				dwc, &dwc2_req_trace_fops);

	if (!file) {
		ret = -ENOMEM;
		goto err1;
	}
#endif

	return 0;

err1:
	debugfs_remove_recursive(root);

err0:
	return ret;
#else
	return 0;
#endif
}

void dwc2_debugfs_exit(struct dwc2 *dwc)
{
#if 0
	debugfs_remove_recursive(dwc->root);
	dwc->root = NULL;
#endif
}
