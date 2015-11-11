/*
 * linux/drivers/mfd/act8600-core.c - mfd core driver PMU ACT8600
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 * Written by Large Dipper <ykli@ingenic.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/axp173-private.h>
#include <linux/delay.h>
#include <jz_notifier.h>
#include <linux/regulator/consumer.h>

int axp173_read_reg(struct i2c_client *client,
		             unsigned char reg,
					 unsigned char *val)
{
	i2c_master_send(client, &reg, 1);

	return i2c_master_recv(client, val, 1);
}
EXPORT_SYMBOL_GPL(axp173_read_reg);

int axp173_write_reg(struct i2c_client *client,
		      unsigned char reg, unsigned char val)
{
	unsigned char msg[2];

	memcpy(&msg[0], &reg, 1);
	memcpy(&msg[1], &val, 1);

	return i2c_master_send(client, msg, 2);
}
EXPORT_SYMBOL_GPL(axp173_write_reg);

#define POWER_REG_NAME	"vpower"

int axp173_power_off(void)
{
	int ret = 0;
        struct regulator *power = NULL;
        local_irq_disable();

        power = regulator_get(NULL, POWER_REG_NAME);

        if (IS_ERR(power)) {
                printk("Power regulator %s get error\n", POWER_REG_NAME);
                ret = -EINVAL;
        }

        ret = regulator_force_disable(power);
        mdelay(200);

	return ret;
}

static int axp173_register_reset_notifier(struct jz_notifier *nb)
{
        return jz_notifier_register(nb,NOTEFY_PROI_HIGH);
}

static int axp173_reset_notifier_handler(struct jz_notifier *nb,void* data)
{
        int ret;
        printk("WARNNING:system will power off!\n");
        ret = axp173_power_off();
        if (ret < 0)
                printk("axp173_power_off failed \n");
        return ret;
}

static struct mfd_cell axp173_cells[] = {
	{
		.id = 0,
		.name = "axp173-regulator",
	},
	{
		.id = 1,
		.name = "axp173-charger",
	},
};

static int axp173_probe(struct i2c_client *client,const struct i2c_device_id *id)
{
	struct axp173 *axp173;
	int ret;
#if 0
    unsigned char d[8] = {0};
	unsigned char regs[8]={0};

	regs[0]=0x40;
	regs[1]=0x41;
	regs[2]=0x42;
	regs[3]=0x43;
	regs[4]=0x44;
	regs[5]=0x45;
	regs[6]=0x46;
	regs[7]=0x47;

    printk("axp173 i2c probe clinet->addr:%x\n",client->addr);

    axp173_read_reg(client, regs[0],&d[0]);
    axp173_read_reg(client, regs[1],&d[1]);
    axp173_read_reg(client, regs[2],&d[2]);
    axp173_read_reg(client, regs[3],&d[3]);
    axp173_read_reg(client, regs[4],&d[4]);
    axp173_read_reg(client, regs[5],&d[5]);
    axp173_read_reg(client, regs[6],&d[6]);
    axp173_read_reg(client, regs[7],&d[7]);


	printk("******************************************\n");
	printk("reg 40 0x%x\n",d[0]);
	printk("reg 41 0x%x\n",d[1]);
	printk("reg 42 0x%x\n",d[2]);
	printk("reg 43 0x%x\n",d[3]);
	printk("reg 44 0x%x\n",d[4]);
	printk("reg 45 0x%x\n",d[5]);
	printk("reg 46 0x%x\n",d[6]);
	printk("reg 47 0x%x\n",d[7]);
	printk("******************************************\n");
#endif
	axp173 = kzalloc(sizeof(struct axp173), GFP_KERNEL);
	if (axp173 == NULL) {
		dev_err(&client->dev, "device alloc error\n");
		return -ENOMEM;
	}
	axp173->client = client;
	axp173->dev = &client->dev;
	i2c_set_clientdata(client, axp173);

        axp173->axp173_notifier.jz_notify = axp173_reset_notifier_handler;
        axp173->axp173_notifier.level = NOTEFY_PROI_NORMAL;
        axp173->axp173_notifier.msg = JZ_POST_HIBERNATION;

	ret = mfd_add_devices(axp173->dev, -1,
			      axp173_cells, ARRAY_SIZE(axp173_cells),
			      NULL, 0);
        if (ret) {
                dev_err(&client->dev, "add devices failed: %d\n", ret);
                goto err_add_devs;
        }

        ret = axp173_register_reset_notifier(&(axp173->axp173_notifier));
        if (ret) {
                printk("axp173_register_reset_notifier failed\n");
                goto err_add_notifier;
        }

#if 0
	if(ret)
		printk("add axp173-regulator device  failed\n");
	else
		printk("add axp173-regulator device  successed\n");
#endif
	return 0;

err_add_notifier:
err_add_devs:
	kfree(axp173);
	return ret;
}

static int axp173_remove(struct i2c_client *client)
{
	struct axp173  *axp173 = i2c_get_clientdata(client);

	mfd_remove_devices(axp173->dev);
	kfree(axp173);
	return 0;
}

static const struct i2c_device_id axp173_id[] = {
	{"axp173", 0 },
};

static struct i2c_driver axp173_pmu_driver = {
	.probe		= axp173_probe,
	.remove		= axp173_remove,
	.id_table	= axp173_id,
	.driver = {
		.name	= "axp173",
		.owner	= THIS_MODULE,
	},
};

static int __devinit axp173_pmu_init(void)
{
	return i2c_add_driver(&axp173_pmu_driver);
}

static void __exit axp173_pmu_exit(void)
{
	i2c_del_driver(&axp173_pmu_driver);
}

subsys_initcall_sync(axp173_pmu_init);
module_exit(axp173_pmu_exit);

MODULE_DESCRIPTION("axp173 PMU mfd Driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("JBXU <jbxu@ingenic.cn>");
