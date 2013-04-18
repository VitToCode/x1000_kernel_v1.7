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
#include <linux/platform_device.h>

#include <linux/usb/composite.h>
#include <linux/usb/rawbulk.h>

#ifdef DEBUG
#define ldbg(f, a...) printk(KERN_DEBUG "%s - " f "\n", __func__, ##a)
#else
#define ldbg(...) {}
#endif

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

#define function_to_rawbulk(f) container_of(f, struct rawbulk_function, function)

static void simple_setup_complete(struct usb_ep *ep,
		struct usb_request *req) {
}

static void rawbulk_function_unbind(struct usb_configuration *c, struct
		usb_function *f) {
	struct rawbulk_function *fn = function_to_rawbulk(f);

	printk(KERN_DEBUG "%s - unbind %s.\n", __func__, fn->longname);
	rawbulk_unbind_transfer(fn->transfer);
}

static int rawbulk_function_setalt(struct usb_function *f, unsigned intf,
		unsigned alt) {
	struct rawbulk_function *fn = function_to_rawbulk(f);
	fn->activated = 1;
	schedule_work(&fn->activator);
	return 0;
}

static void rawbulk_function_disable(struct usb_function *f) {
	struct rawbulk_function *fn = function_to_rawbulk(f);
	fn->activated = 0;
	schedule_work(&fn->activator);
}

static struct usb_request_wrapper *get_wrapper(struct usb_request *req) {
	if (!req->buf)
		return NULL;
	return container_of(req->buf, struct usb_request_wrapper, buffer);
}

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

#if 0
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
#endif

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


static void rawbulk_destory_function(struct rawbulk_function *fn) {
	if (!fn)
		return;
	wake_lock_destroy(&fn->keep_awake);
	kfree(fn);
}

struct rawbulk_function *rawbulk_init(struct usb_composite_dev *cdev,char *name)
{
	struct rawbulk_function *fn;

	fn = kzalloc(sizeof *fn, GFP_KERNEL);
	if (IS_ERR(fn))
		return NULL;

	/* init default features of rawbulk functions */
	strcpy(fn->longname,"rawbulk bypass test");
	strcpy(fn->name,name);
	fn->string_defs[0].s = "None";
	fn->nups = 1;
	fn->ndowns = 1;
	fn->upsz = PAGE_SIZE;
	fn->downsz = PAGE_SIZE;
	fn->splitsz = PAGE_SIZE;
	fn->autoreconn = true;
	fn->pushable = false;

	/* init descriptors */
	init_interface_desc(&fn->interface);

	init_endpoint_desc(&fn->fs_bulkin_endpoint, 1, 64);
	init_endpoint_desc(&fn->fs_bulkout_endpoint, 0, 64);

	init_endpoint_desc(&fn->hs_bulkin_endpoint, 1, 64);
	init_endpoint_desc(&fn->hs_bulkout_endpoint, 0, 512);

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

	/* init function callbacks */
	fn->function.strings = fn->strings;
	fn->function.descriptors = fn->fs_descs;
	fn->function.hs_descriptors = fn->hs_descs;

	fn->transfer = rawbulk_transfer_get(name);
	if(!fn->transfer)
		return NULL;
	if (IS_ERR(fn->dev)) {
		kfree(fn);
		return NULL;
	}

	spin_lock_init(&fn->lock);
	wake_lock_init(&fn->keep_awake, WAKE_LOCK_SUSPEND, fn->longname);
	return fn;
}

void rawbulk_free(struct rawbulk_function *fn)
{
	rawbulk_destory_function(fn);
}

/*
 * ep0 ~ ep3: maxpacket = 512, high-speed
 * ep4: maxpacket = 64, full-speed
 * ep5: maxpacket = 8, full-speed
 */
static struct usb_ep *match_ep(struct usb_configuration *c,
		struct rawbulk_function *fn, int in) {
	struct usb_gadget *gadget = c->cdev->gadget;
	struct usb_ep *ep;
	struct usb_endpoint_descriptor *desc =
		(struct usb_endpoint_descriptor *)(in? fn->fs_descs[BULKIN_DESC]:
				fn->fs_descs[BULKOUT_DESC]);
#if 0
	/* assign ep4 to at or pcv channel etc. */
	if (fn->transfer_id > RAWBULK_TID_ETS) {
		ep = find_ep(gadget, in? "ep4in": "ep4out");
		if (ep && ep_matches (gadget, ep, desc))
			return ep;
	}
#endif
	return usb_ep_autoconfig(gadget, desc);
}

static void rawbulk_auto_reconnect(struct rawbulk_function *fn) {
	int rc;
	if (!fn || fn->autoreconn == 0)
		return;

	if (rawbulk_check_enable(fn) && fn->activated) {
		printk(KERN_DEBUG "start %s again automatically.\n", fn->longname);
		rc = rawbulk_start_transactions(fn->transfer, fn->nups,
				fn->ndowns, fn->upsz, fn->downsz, fn->splitsz, fn->pushable);
		if (rc < 0) {
			rawbulk_disable_function(fn);
		}
	}
}

int rawbulk_bind_config(struct rawbulk_function *fn,struct usb_configuration *c)
{
	int rc, ifnum;
	struct usb_gadget *gadget = c->cdev->gadget;
	struct usb_ep *ep_out, *ep_in;

	ifnum = 0;

	fn->interface.bInterfaceNumber = cpu_to_le16(ifnum);

	if (!(ep_out = match_ep(c, fn, 0))) {
		printk(KERN_DEBUG "unfortunately, we could not find endpoint(OUT) for %s",
				fn->longname);
		return -ENOMEM;
	}

	if (!(ep_in = match_ep(c, fn, 1))) {
		printk(KERN_DEBUG "unfortunately, we could not find endpoint(IN) for %s",
				fn->longname);
		return -ENOMEM;
	}

	ep_out->driver_data = fn;
	ep_in->driver_data = fn;
	fn->bulk_out = ep_out;
	fn->bulk_in = ep_in;

	fn->function.descriptors = usb_copy_descriptors(fn->fs_descs);
	if (unlikely(!fn->function.descriptors))
		return -ENOMEM;

	if (gadget_is_dualspeed(gadget)) {
		fn->hs_bulkin_endpoint.bEndpointAddress =
			fn->fs_bulkin_endpoint.bEndpointAddress;
		fn->hs_bulkout_endpoint.bEndpointAddress =
			fn->fs_bulkout_endpoint.bEndpointAddress;
		fn->function.hs_descriptors = usb_copy_descriptors(fn->hs_descs);
		if (unlikely(!fn->function.hs_descriptors)) {
			usb_free_descriptors(fn->function.descriptors);
			return -ENOMEM;
		}
	}

	fn->cdev = c->cdev;
	fn->activated = 0;

	/*
	   rc = rawbulk_register_tty(fn);
	   if (rc < 0)
	   printk(KERN_ERR "failed to register tty for %s\n", fn->longname);
	 */

	return rawbulk_bind_transfer(fn->transfer, &fn->function, ep_out, ep_in,
			rawbulk_auto_reconnect);
}

int rawbulk_function_setup(struct rawbulk_function *fn,
		struct usb_composite_dev *cdev,
		const struct usb_ctrlrequest *c)
{
	int rc;
	struct usb_request *req = cdev->req;

	if (rawbulk_transfer_state(fn->transfer) < 0)
		return -EOPNOTSUPP;

	printk(KERN_DEBUG "DUMP ctrl: bRequestType = %02X, bRequest = %02X, " \
			"wValue = %04X, wIndex = %04X, wLength = %d\n",
			c->bRequestType, c->bRequest, c->wValue, c->wIndex,
			c->wLength);

	if (!rawbulk_check_enable(fn))
		return -EOPNOTSUPP;
	
	rc = rawbulk_forward_ctrlrequest(fn->transfer,c);
	if (rc < 0) {
		printk(KERN_DEBUG "rawbulk forwarding failed %d\n", rc);
		return rc;
	}

	return 256 + 999;//USB_GADGET_DELAYED_STATUS;
}

