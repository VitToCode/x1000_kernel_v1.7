/*
 * Rawbulk Gadget Function Driver from VIA Telecom
 *
 * Copyright (C) 2011 VIA Telecom, Inc.
 * Author: Karfield Chen (kfchen@via-telecom.com)
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

/* #define DEBUG */
/* #define VERBOSE_DEBUG */

#define DRIVER_AUTHOR   "Karfield Chen <kfchen@via-telecom.com>"
#define DRIVER_DESC     "Rawbulk Gadget - transport data from CP to Gadget"
#define DRIVER_VERSION  "1.0.1"
#define DRIVER_NAME     "usb_rawbulk"

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/err.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/wait.h>
#include <linux/types.h>
#include <linux/device.h>

#include <linux/usb/composite.h>
#include <linux/usb/rawbulk.h>

#ifdef DEBUG
#define ldbg(f, a...) printk(KERN_DEBUG "%s - " f "\n", __func__, ##a)
#else
#define ldbg(...) {}
#endif

/* sysfs attr idx assignment */
enum _attr_idx {
	ATTR_ENABLE = 0,            /** enable switch for Rawbulk */
#ifdef SUPPORT_LEGACY_CONTROL
	ATTR_ENABLE_C,              /** enable switch too, but for legacy */
#endif
	ATTR_AUTORECONN,            /** enable to rebind cp when it reconnect */
	ATTR_STATISTICS,            /** Rawbulk summary/statistics for one pipe */
	ATTR_NUPS,                  /** upstream transfers count */
	ATTR_NDOWNS,                /** downstram transfers count */
	ATTR_UPSIZE,                /** upstream buffer for each transaction */
	ATTR_DOWNSIZE,              /** downstram buffer for each transaction */
	ATTR_SPLIT,                 /** split option for downstream */
	ATTR_PUSHABLE,              /** set to use push-way for upstream */
	ATTR_DTR,                   /** DTR control, only for Data-Call port */
};

/* USB gadget framework is not strong enough, and not be compatiable with some
 * controller, such as musb.
 * in musb driver, the usb_request's member list is used internally, but in some
 * applications it used in function driver too. to avoid this, here we
 * re-wrap the usb_request */
struct usb_request_wrapper {
	struct list_head list;
	struct usb_request *request;
	int length;
	struct rawbulk_function *fn;
	char buffer[0];
};

static struct usb_request_wrapper *get_wrapper(struct usb_request *req) {
	if (!req->buf)
		return NULL;
	return container_of(req->buf, struct usb_request_wrapper, buffer);
}

struct rawbulk_function *prealloced_functions[_MAX_TID] = { NULL };
struct rawbulk_function *rawbulk_lookup_function(int transfer_id) {
	if (transfer_id >= 0 && transfer_id < _MAX_TID)
		return prealloced_functions[transfer_id];
	return NULL;
}
EXPORT_SYMBOL_GPL(rawbulk_lookup_function);

static inline int check_enable_state(struct rawbulk_function *fn) {
	int enab;
	unsigned long flags;
	spin_lock_irqsave(&fn->lock, flags);
	enab = fn->enable? 1: 0;
	spin_unlock_irqrestore(&fn->lock, flags);
	return enab;
}

int rawbulk_check_enable(struct rawbulk_function *fn) {
	return check_enable_state(fn);
}
EXPORT_SYMBOL_GPL(rawbulk_check_enable);


static inline void set_enable_state(struct rawbulk_function *fn, int enab) {
	unsigned long flags;
	spin_lock_irqsave(&fn->lock, flags);
	fn->enable = !!enab;
	spin_unlock_irqrestore(&fn->lock, flags);
}

void rawbulk_disable_function(struct rawbulk_function *fn) {
	set_enable_state(fn, 0);
}
EXPORT_SYMBOL_GPL(rawbulk_disable_function);


#define port_to_rawbulk(p) container_of(p, struct rawbulk_function, port)
#define function_to_rawbulk(f) container_of(f, struct rawbulk_function, function)

static struct usb_request_wrapper *get_req(struct list_head *head, spinlock_t
		*lock) {
	unsigned long flags;
	struct usb_request_wrapper *req = NULL;
	spin_lock_irqsave(lock, flags);
	if (!list_empty(head)) {
		req = list_first_entry(head, struct usb_request_wrapper, list);
		list_del(&req->list);
	}
	spin_unlock_irqrestore(lock, flags);
	return req;
}

static void put_req(struct usb_request_wrapper *req, struct list_head *head,
		spinlock_t *lock) {
	unsigned long flags;
	spin_lock_irqsave(lock, flags);
	list_add_tail(&req->list, head);
	spin_unlock_irqrestore(lock, flags);
}

static void move_req(struct usb_request_wrapper *req, struct list_head *head,
		spinlock_t *lock) {
	unsigned long flags;
	spin_lock_irqsave(lock, flags);
	list_move_tail(&req->list, head);
	spin_unlock_irqrestore(lock, flags);
}

static void insert_req(struct usb_request_wrapper *req, struct list_head *head,
		spinlock_t *lock) {
	unsigned long flags;
	spin_lock_irqsave(lock, flags);
	list_add(&req->list, head);
	spin_unlock_irqrestore(lock, flags);
}

static int control_dtr(int set) {
	struct usb_ctrlrequest ctrl = {
		.bRequestType = USB_DIR_OUT | USB_TYPE_VENDOR, //0x40
		.bRequest = 0x01,
		.wLength = 0,
		.wIndex = 0,
	};

	ctrl.wValue = cpu_to_le16(!!set);
	return rawbulk_forward_ctrlrequest(&ctrl);
}

static void init_endpoint_desc(struct usb_endpoint_descriptor *epdesc, int in,
		int maxpacksize) {
	struct usb_endpoint_descriptor template = {
		.bLength =      USB_DT_ENDPOINT_SIZE,
		.bDescriptorType =  USB_DT_ENDPOINT,
		.bmAttributes =     USB_ENDPOINT_XFER_BULK,
	};

	*epdesc = template;
	if (in)
		epdesc->bEndpointAddress = USB_DIR_IN;
	else
		epdesc->bEndpointAddress = USB_DIR_OUT;
	epdesc->wMaxPacketSize = cpu_to_le16(maxpacksize);
}

static void init_interface_desc(struct usb_interface_descriptor *ifdesc) {
	struct usb_interface_descriptor template = {
		.bLength = USB_DT_INTERFACE_SIZE,
		.bDescriptorType = USB_DT_INTERFACE,
		.bInterfaceNumber = 0,
		.bAlternateSetting = 0,
		.bNumEndpoints = 2,
		.bInterfaceClass = 0xff,//USB_CLASS_VENDOR_SPEC,
		.bInterfaceSubClass = 0xff,
		.bInterfaceProtocol = 0xff,
		.iInterface = 0,
	};

	*ifdesc = template;
}

static inline void add_device_attr(struct rawbulk_function *fn, int n, const char
		*name, int mode) {
	if (n < MAX_ATTRIBUTES) {
		fn->attr[n].attr.name = name;
		fn->attr[n].attr.mode = mode;
	}
}

static int which_attr(struct rawbulk_function *fn, struct device_attribute
		*attr) {
	int n;
	for (n = 0; n < fn->max_attrs; n ++) {
		if (attr == &fn->attr[n])
			return n;
	}
	return -1;
}

static int rawbulk_create_files(struct rawbulk_function *fn) {
	int n, rc;
	for (n = 0; n < fn->max_attrs; n ++){
#ifdef SUPPORT_LEGACY_CONTROL
		if (n == ATTR_ENABLE_C)
			continue;
#endif
		rc = device_create_file(fn->dev, &fn->attr[n]);
		if (rc < 0) {
			while (--n >= 0) {
#ifdef SUPPORT_LEGACY_CONTROL
				if (n == ATTR_ENABLE_C)
					continue;
#endif
				device_remove_file(fn->dev, &fn->attr[n]);
			}
			return rc;
		}
	}
	return 0;
}

static void rawbulk_remove_files(struct rawbulk_function *fn) {
	int n = fn->max_attrs;
	while (--n >= 0) {
#ifdef SUPPORT_LEGACY_CONTROL
		if (n == ATTR_ENABLE_C)
			continue;
#endif
		device_remove_file(fn->dev, &fn->attr[n]);
	}
}

/******************************************************************************/
static struct class *rawbulk_class;

static struct _function_init_stuff {
	const char *longname;
	const char *shortname;
	const char *iString;
	unsigned int nups;
	unsigned int ndowns;
	unsigned int upsz;
	unsigned int downsz;
	unsigned int splitsz;
	bool autoreconn;
	bool pushable;
};

static struct _function_init_stuff _init_params[] = {
	{"rawbulk-modem", "data", "Modem Port", 16, 16, PAGE_SIZE, PAGE_SIZE, PAGE_SIZE, true, false },
	{"rawbulk-ets", "ets", "ETS Port", 4, 1, PAGE_SIZE, PAGE_SIZE, PAGE_SIZE, true, false },
	{"rawbulk-at", "atc", "AT Channel", 1, 1, PAGE_SIZE, PAGE_SIZE, PAGE_SIZE, true, false },
	{"rawbulk-pcv", "pcv", "PCM Voice", 1, 1, PAGE_SIZE, PAGE_SIZE, PAGE_SIZE, true, false },
	{"rawbulk-gps", "gps", "LBS GPS Port", 1, 1, PAGE_SIZE, PAGE_SIZE, PAGE_SIZE, true, false },
	{ }, /* End of configurations */
};

static __init struct rawbulk_function *rawbulk_alloc_function(int transfer_id) {
	int rc;
	struct rawbulk_function *fn;

	if (transfer_id == _MAX_TID)
		return NULL;

	fn = kzalloc(sizeof *fn, GFP_KERNEL);
	if (IS_ERR(fn))
		return NULL;

	/* init default features of rawbulk functions */
	fn->longname = _init_params[transfer_id].longname;
	fn->shortname = _init_params[transfer_id].shortname;
	fn->string_defs[0].s = _init_params[transfer_id].iString;
	fn->nups = _init_params[transfer_id].nups;
	fn->ndowns = _init_params[transfer_id].ndowns;
	fn->upsz = _init_params[transfer_id].upsz;
	fn->downsz = _init_params[transfer_id].downsz;
	fn->splitsz = _init_params[transfer_id].splitsz;
	fn->autoreconn = _init_params[transfer_id].autoreconn;
	fn->pushable = _init_params[transfer_id].pushable;

	fn->tty_minor = -1;
	/* init descriptors */
	init_interface_desc(&fn->interface);
	init_endpoint_desc(&fn->fs_bulkin_endpoint, 1, 64);
	init_endpoint_desc(&fn->fs_bulkout_endpoint, 0, 64);
	if (transfer_id < RAWBULK_TID_AT) {
		init_endpoint_desc(&fn->hs_bulkin_endpoint, 1, 64);
		init_endpoint_desc(&fn->hs_bulkout_endpoint, 0, 512);
	} else {
		init_endpoint_desc(&fn->hs_bulkin_endpoint, 1, 64);
		init_endpoint_desc(&fn->hs_bulkout_endpoint, 0, 64);
	}

	fn->fs_descs[INTF_DESC] = (struct usb_descriptor_header *) &fn->interface;
	fn->fs_descs[BULKIN_DESC] = (struct usb_descriptor_header *) &fn->fs_bulkin_endpoint;
	fn->fs_descs[BULKOUT_DESC] = (struct usb_descriptor_header *) &fn->fs_bulkout_endpoint;

	fn->hs_descs[INTF_DESC] = (struct usb_descriptor_header *) &fn->interface;
	fn->hs_descs[BULKIN_DESC] = (struct usb_descriptor_header *) &fn->hs_bulkin_endpoint;
	fn->hs_descs[BULKOUT_DESC] = (struct usb_descriptor_header *) &fn->hs_bulkout_endpoint;

	fn->string_table.language = 0x0409;
	fn->string_table.strings = fn->string_defs;
	fn->strings[0] = &fn->string_table;
	fn->strings[1] = NULL;

	fn->transfer_id = transfer_id;

	/* init function callbacks */
	fn->function.strings = fn->strings;
	fn->function.descriptors = fn->fs_descs;
	fn->function.hs_descriptors = fn->hs_descs;

	/* init device attributes */
	add_device_attr(fn, ATTR_ENABLE, "enable", 0755);
#ifdef SUPPORT_LEGACY_CONTROL
	add_device_attr(fn, ATTR_ENABLE_C, fn->shortname, 0755);
#endif
	add_device_attr(fn, ATTR_AUTORECONN, "autoreconn", 0755);
	add_device_attr(fn, ATTR_STATISTICS, "statistics", 0644);
	add_device_attr(fn, ATTR_NUPS, "nups", 0755);
	add_device_attr(fn, ATTR_NDOWNS, "ndowns", 0755);
	add_device_attr(fn, ATTR_UPSIZE, "ups_size", 0755);
	add_device_attr(fn, ATTR_DOWNSIZE, "downs_size", 0755);
	add_device_attr(fn, ATTR_SPLIT, "split_size", 0755);
	add_device_attr(fn, ATTR_PUSHABLE, "pushable", 0755);

	fn->max_attrs = ATTR_PUSHABLE + 1;

	if (transfer_id == RAWBULK_TID_MODEM) {
		add_device_attr(fn, ATTR_DTR, "dtr", 0222);
		fn->max_attrs ++;
	}

	fn->dev = device_create(rawbulk_class, NULL, MKDEV(0,
				fn->transfer_id), NULL, fn->shortname);
	if (IS_ERR(fn->dev)) {
		kfree(fn);
		return NULL;
	}

	rc = rawbulk_create_files(fn);
	if (rc < 0) {
		device_destroy(rawbulk_class, fn->dev->devt);
		kfree(fn);
		return NULL;
	}

	spin_lock_init(&fn->lock);
	wake_lock_init(&fn->keep_awake, WAKE_LOCK_SUSPEND, fn->longname);
	return fn;
}

static void rawbulk_destory_function(struct rawbulk_function *fn) {
	if (!fn)
		return;
	wake_lock_destroy(&fn->keep_awake);
	rawbulk_remove_files(fn);
	device_destroy(rawbulk_class, fn->dev->devt);
	kfree(fn);
}

#ifdef SUPPORT_LEGACY_CONTROL
static struct attribute *legacy_sysfs[_MAX_TID + 1] = { NULL };
static struct attribute_group legacy_sysfs_group = {
	.attrs = legacy_sysfs,
};
struct kobject *legacy_sysfs_stuff;
#endif /* SUPPORT_LEGACY_CONTROL */

static int __init init(void) {
	int n;
	int rc = 0;

	printk(KERN_INFO "rawbulk functions init.\n");

	rawbulk_class = class_create(THIS_MODULE, "usb_rawbulk");
	if (IS_ERR(rawbulk_class))
		return PTR_ERR(rawbulk_class);

	for (n = 0; n < _MAX_TID; n ++) {
		struct rawbulk_function *fn = rawbulk_alloc_function(n);
		if (IS_ERR(fn)) {
			while (n --)
				rawbulk_destory_function(prealloced_functions[n]);
			rc = PTR_ERR(fn);
			break;
		}
		prealloced_functions[n] = fn;
#ifdef SUPPORT_LEGACY_CONTROL
		legacy_sysfs[n] = &fn->attr[ATTR_ENABLE_C].attr;
#endif
	}

	if (rc < 0) {
		class_destroy(rawbulk_class);
		return rc;
	}

#ifdef SUPPORT_LEGACY_CONTROL
	/* make compatiable with old bypass sysfs access */
	legacy_sysfs_stuff = kobject_create_and_add("usb_bypass", NULL);
	if (legacy_sysfs_stuff) {
		rc = sysfs_create_group(legacy_sysfs_stuff, &legacy_sysfs_group);
		if (rc < 0)
			printk(KERN_ERR "failed to create legacy bypass sys-stuff, but continue...\n");
	}
#endif /* SUPPORT_LEGACY_CONTROL */

	return 0;
}

module_init(init);

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_VERSION(DRIVER_VERSION);
MODULE_LICENSE("GPL");

