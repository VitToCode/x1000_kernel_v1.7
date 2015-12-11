/*
 * Sample uevent report implementation
 *
 * Copyright (C) 2015-2017 jbxu <jbxu@ingenic.com>
 * Copyright (C) 2015 Novell Inc.
 *
 * Released under the GPL version 2 only.
 *
 */

#include <asm/irq.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <mach/fp8102_det.h>

int irqnum = 0;
struct uevent_platform_data* updata = NULL;
struct uevent_report * uinfo = NULL;
static struct kset *uevent_kset = NULL;
static struct work_struct uevent_wq = {0};

struct uevent_obj {
	struct kobject kobj;
	int val;
};
struct uevent_obj *uobj;

struct uevent_attribute {
	struct attribute attr;
	ssize_t (*show)(struct uenent_obj *uobj, struct uevent_attribute *attr, char *buf);
	ssize_t (*store)(struct uevent_obj *uobj, struct uevent_attribute *attr, const char *buf, size_t count);
};

static ssize_t uevent_attr_show(struct kobject *kobj,
			struct attribute *attr,
			char *buf)
{
	return 0;
}

static ssize_t uevent_attr_store(struct kobject *kobj,
			struct attribute *attr,
			const char *buf, size_t len)
{
	return 0;
}

static const struct sysfs_ops uevent_sysfs_ops = {
	.show = uevent_attr_show,
	.store = uevent_attr_store,
};

static void uevent_release(struct kobject *kobj)
{
	kfree(uobj);
}

static ssize_t uevent_show(struct uevent_obj *uevent_obj, struct uevent_attribute *attr,
			char *buf)
{
	/*do nothing*/
	return  0;
}

static ssize_t uevent_store(struct uvent_obj *uevent_obj, struct uevent_attribute *attr,
			 const char *buf, size_t count)
{
	/*do nothing*/
	return  0;
}

static struct uevent_attribute uevent_attribute =
	__ATTR(uevent, 0666, uevent_show, uevent_store);

static struct attribute *uevent_default_attrs[] = {
	&uevent_attribute.attr,
	NULL,
};

static struct kobj_type uevent_ktype = {
	.sysfs_ops = &uevent_sysfs_ops,
	.release = uevent_release,
	.default_attrs = uevent_default_attrs,
};

static void destroy_uevent_obj(struct uevent_obj *ub)
{
	kobject_put(&ub->kobj);
	kfree(&ub->kobj);
}

void uevent_do_work(unsigned long data){

	kobject_uevent_env(&uobj->kobj, KOBJ_CHANGE,updata->ur->report_string);
}

static interrupt_isr(int irq, void *data)
{
	schedule_work(&uevent_wq);
	return IRQ_HANDLED;
}

static int fp8102_det_probe( struct platform_device *pdev)
{
	int ret = 0;
	updata = pdev->dev.platform_data;
	if(!updata){
		printk("get uevent pdata failed \n");
		return -EINVAL;
	}

	uinfo = updata->ur;
	uevent_kset = kset_create_and_add("uevent_report", NULL, kernel_kobj);
	if (!uevent_kset){
		return -ENOMEM;
	}
	uobj = kzalloc(sizeof(*uobj), GFP_KERNEL);
	if (!uobj){
		goto kset_error;
	}

	INIT_WORK(&uevent_wq,uevent_do_work);

	uobj->kobj.kset = uevent_kset;
	ret = kobject_init_and_add(&uobj->kobj, &uevent_ktype, NULL, "%s", "uevent_report");
	if (ret){
		goto uevent_error;
	}

	ret = gpio_request(uinfo->pin,"uevent_report");
	if (ret){
		printk("gpio reuest for uevent report failed\n");
		goto uevent_error;
	}

	irqnum = gpio_to_irq(uinfo->pin);
	if (request_irq(irqnum, interrupt_isr, uinfo->irq_type, "uevent_report", NULL)) {
		printk(KERN_ERR "uevent.c: Can't allocate irq %d\n", irqnum);
		goto gpio_error;
	}

	return 0;

gpio_error:
	gpio_free(uinfo->pin);
uevent_error:
	destroy_workqueue(&uevent_wq);
	destroy_uevent_obj(uobj);
kset_error:
	kset_unregister(uevent_kset);
	return -EINVAL;
}

static int fp8102_det_remove(struct platform_device *dev)
{
	destroy_workqueue(&uevent_wq);
	destroy_uevent_obj(uobj);
	kset_unregister(uevent_kset);
	free_irq(irqnum,NULL);
	gpio_free(uinfo->pin );
	return 0;
}

static struct platform_driver fp8102_det_driver = {
	.driver = {
		.name = "fp8102_det",
		.owner = THIS_MODULE,
	},
	.probe = fp8102_det_probe,
	.remove = fp8102_det_remove,
};

static int __init fp8102_det_init(void)
{
	return platform_driver_register(&fp8102_det_driver);
}

static void __exit fp8102_det_exit(void)
{
	platform_driver_unregister(&fp8102_det_driver);
}

module_init(fp8102_det_init);
module_exit(fp8102_det_exit);

MODULE_DESCRIPTION("fp8102 charge status report");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("jbxu");
