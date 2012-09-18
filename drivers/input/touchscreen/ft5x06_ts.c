/* drivers/input/touchscreen/ft5x06_ts.c
 *
 * FocalTech ft5x06 TouchScreen driver.
 *
 * Copyright (c) 2010  Focal tech Ltd.
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

#include <linux/i2c.h>
#include <linux/input.h>
#include "ft5x06_ts.h"
#include <linux/earlysuspend.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/syscalls.h>
#include <linux/unistd.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/tsc.h>

#include <linux/interrupt.h>
struct ts_event {
	u16 au16_x[CFG_MAX_TOUCH_POINTS];	/*x coordinate */
	u16 au16_y[CFG_MAX_TOUCH_POINTS];	/*y coordinate */
	u8 au8_touch_event[CFG_MAX_TOUCH_POINTS];	/*touch event:
					0 -- down; 1-- contact; 2 -- contact */
	u8 au8_finger_id[CFG_MAX_TOUCH_POINTS];	/*touch ID */
	u16 pressure;
	u8 touch_point;
	u8 touch_num;
};

struct ft5x06_gpio {
	struct jztsc_pin *irq;
	struct jztsc_pin *wake;
};

struct ft5x06_ts_data {
	unsigned int irq;
	u16 x_max;
	u16 y_max;
	int is_suspend;
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct ts_event event;
	struct jztsc_platform_data *pdata;
	struct ft5x06_gpio gpio;
	struct regulator *power;
	struct early_suspend early_suspend;
	struct work_struct  work;
	struct workqueue_struct *workqueue;
};

/*
*ft5x06_i2c_Read-read data and write data by i2c
*@client: handle of i2c
*@writebuf: Data that will be written to the slave
*@writelen: How many bytes to write
*@readbuf: Where to store data read from slave
*@readlen: How many bytes to read
*
*Returns negative errno, else the number of messages executed
*
*
*/
static void ft5x06_ts_release(struct ft5x06_ts_data *data);
static void ft5x06_ts_reset(struct ft5x06_ts_data *ts);
int ft5x06_i2c_Read(struct i2c_client *client, char *writebuf,
		    int writelen, char *readbuf, int readlen)
{
	int ret;

	struct ft5x06_ts_data *ft5x06_ts = i2c_get_clientdata(client);
	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
			 },
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0){
			dev_err(&client->dev, "%s: -i2c read error.\n",
				__func__);
			ft5x06_ts_release(ft5x06_ts);
			ft5x06_ts_reset(ft5x06_ts);
		}
	} else {
		struct i2c_msg msgs[] = {
			{
			 .addr = client->addr,
			 .flags = I2C_M_RD,
			 .len = readlen,
			 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0){
			dev_err(&client->dev, "%s:i2c read error.\n", __func__);
			ft5x06_ts_reset(ft5x06_ts);
		}
	}
	return ret;
}
/*write data by i2c*/
int ft5x06_i2c_Write(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret;
	
	struct ft5x06_ts_data *ft5x06_ts = i2c_get_clientdata(client);
	struct i2c_msg msg[] = {
		{
		 .addr = client->addr,
		 .flags = 0,
		 .len = writelen,
		 .buf = writebuf,
		 },
	};

	ret = i2c_transfer(client->adapter, msg, 1);
	if (ret < 0){
		dev_err(&client->dev, "%s i2c write error.\n", __func__);
		ft5x06_ts_release(ft5x06_ts);
		ft5x06_ts_reset(ft5x06_ts);
	}
	return ret;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
static int ft5x06_set_reg(struct ft5x06_ts_data *ts, u8 addr, u8 para)
{
	u8 buf[3];
	int ret = -1;

	buf[0] = addr;
	buf[1] = para;
	ret = ft5x06_i2c_Write(ts->client, buf, 2);
	if (ret < 0) {
		pr_err("write reg failed! %#x ret: %d", buf[0], ret);
		return -1;
	}
	return 0;
}
#endif
/*release the point*/
static void ft5x06_ts_release(struct ft5x06_ts_data *data)
{
	/* input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR, 0); */
	input_mt_sync(data->input_dev);
	input_sync(data->input_dev);
}

/*Read touch point information when the interrupt  is asserted.*/
static int ft5x06_read_Touchdata(struct ft5x06_ts_data *data)
{
	struct ts_event *event = &data->event;
	u8 buf[POINT_READ_BUF] = { 0 };
	int ret = -1;
	int i = 0;
	u8 pointid = FT_MAX_ID;
	memset(event, 0, sizeof(struct ts_event));
	event->touch_point = 0;
	buf[0] = 0;
	ret = ft5x06_i2c_Read(data->client, buf, 1, buf, POINT_READ_BUF);
	if (ret < 0) {
		return ret;
	}
	event->touch_num = buf[2]&0x0f;
	if (event->touch_num == 0){
		ft5x06_ts_release(data);
		return 1;
	}

	for (i = 0; i < CFG_MAX_TOUCH_POINTS; i++) {
		pointid = (buf[FT_TOUCH_ID_POS + FT_TOUCH_STEP * i]) >> 4;
		if (pointid >= FT_MAX_ID)
			break;
		else
			event->touch_point++;
		event->au16_x[i] =
		    (s16) (buf[FT_TOUCH_X_H_POS + FT_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FT_TOUCH_X_L_POS + FT_TOUCH_STEP * i];
		event->au16_y[i] =
		    (s16) (buf[FT_TOUCH_Y_H_POS + FT_TOUCH_STEP * i] & 0x0F) <<
		    8 | (s16) buf[FT_TOUCH_Y_L_POS + FT_TOUCH_STEP * i];
		event->au8_touch_event[i] =
		    buf[FT_TOUCH_EVENT_POS + FT_TOUCH_STEP * i] >> 6;
		event->au8_finger_id[i] =
		    (buf[FT_TOUCH_ID_POS + FT_TOUCH_STEP * i]) >> 4;
	}

	event->pressure = FT_PRESS;

	return 0;
}
/*
 *report the point information
 */
static int ft5x06_report_value(struct ft5x06_ts_data *data)
{
	struct ts_event *event = &data->event;
	int i = 0;
	for (i = 0; i < event->touch_point; i++) {
		event->au16_x[i] = event->au16_x[i] * data->x_max / CFG_MAX_X;
		event->au16_y[i] = event->au16_y[i] * data->y_max / CFG_MAX_Y;
		if(event->au16_x[i] > data->x_max || event->au16_y[i] > data->y_max)
			continue;

#ifdef CONFIG_TSC_SWAP_XY
		tsc_swap_xy(&(event->au16_x[i]),&(event->au16_y[i]));
#endif

#ifdef CONFIG_TSC_SWAP_X
		tsc_swap_x(&(event->au16_x[i]),data->x_max);
#endif

#ifdef CONFIG_TSC_SWAP_Y
		tsc_swap_y(&(event->au16_y[i]),data->y_max);
#endif

		//printk("  (x,y)=(%d, %d)\n",event->au16_x[i],event->au16_y[i]);
		input_report_abs(data->input_dev, ABS_MT_POSITION_X,
				event->au16_x[i]);
		input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
				event->au16_y[i]);
		 input_report_abs(data->input_dev, ABS_MT_TRACKING_ID,
				event->au8_finger_id[i]);
		input_report_abs(data->input_dev,ABS_MT_TOUCH_MAJOR,
				event->pressure);
		input_report_abs(data->input_dev,ABS_MT_WIDTH_MAJOR,
				event->pressure);
		input_mt_sync(data->input_dev);
	}
	input_sync(data->input_dev);
	return 0;

}
static void tsc_work_handler(struct work_struct *work)
{
	struct ft5x06_ts_data *ft5x06_ts = container_of(work, struct ft5x06_ts_data,work);
	int ret = 0;
	ret = ft5x06_read_Touchdata(ft5x06_ts);
	if (ret == 0)
		ft5x06_report_value(ft5x06_ts);
	enable_irq(ft5x06_ts->client->irq);
}
/*The ft5x06 device will signal the host about TRIGGER_FALLING.
*Processed when the interrupt is asserted.
*/
static irqreturn_t ft5x06_ts_interrupt(int irq, void *dev_id)
{
	struct ft5x06_ts_data *ft5x06_ts = dev_id;
	disable_irq_nosync(ft5x06_ts->irq);

	if(ft5x06_ts->is_suspend == 1)
		return IRQ_HANDLED;

	if (!work_pending(&ft5x06_ts->work)) {
		queue_work(ft5x06_ts->workqueue, &ft5x06_ts->work);
        }

	return IRQ_HANDLED;
}

static void ft5x06_gpio_init(struct ft5x06_ts_data *ts, struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct jztsc_platform_data *pdata = dev->platform_data;

	ts->gpio.irq = &pdata->gpio[0];
	ts->gpio.wake = &pdata->gpio[1];

	if (gpio_request_one(ts->gpio.irq->num,
			     GPIOF_DIR_IN, "ft5x06_irq")) {
		dev_err(dev, "no irq pin available\n");
		ts->gpio.irq->num = -EBUSY;
	}
	if (gpio_request_one(ts->gpio.wake->num,
			     ts->gpio.wake->enable_level
			     ? GPIOF_OUT_INIT_LOW
			     : GPIOF_OUT_INIT_HIGH,
			     "ft5x06_shutdown")) {
		dev_err(dev, "no shutdown pin available\n");
		ts->gpio.wake->num = -EBUSY;
	}
}


static int ft5x06_ts_power_on(struct ft5x06_ts_data *ts)
{
	if (ts->power)
		return regulator_enable(ts->power);

	return 0;
}

static int ft5x06_ts_power_off(struct ft5x06_ts_data *ts)
{
	if (ts->power)
		return regulator_disable(ts->power);

	return 0;
}

static void ft5x06_ts_reset(struct ft5x06_ts_data *ts)
{
	set_pin_status(ts->gpio.wake, 1);
	set_pin_status(ts->gpio.wake, 0);
	msleep(10);
	set_pin_status(ts->gpio.wake, 1);
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ft5x06_ts_resume(struct early_suspend *handler);
static void ft5x06_ts_suspend(struct early_suspend *handler);
#endif

static int ft5x06_ts_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct jztsc_platform_data *pdata =
	    (struct jztsc_platform_data *)client->dev.platform_data;
	struct ft5x06_ts_data *ft5x06_ts;
	struct input_dev *input_dev;
	int err = 0;
	unsigned char uc_reg_value;
	unsigned char uc_reg_addr;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	ft5x06_ts = kzalloc(sizeof(struct ft5x06_ts_data), GFP_KERNEL);

	if (!ft5x06_ts) {
		err = -ENOMEM;
		goto exit_alloc_data_failed;
	}

	i2c_set_clientdata(client, ft5x06_ts);

	ft5x06_gpio_init(ft5x06_ts, client);

	client->dev.init_name=client->name;
	ft5x06_ts->power = regulator_get(&client->dev, "vtsc");
	if (IS_ERR(ft5x06_ts->power)) {
		dev_warn(&client->dev, "get regulator failed\n");
	}
	ft5x06_ts_power_on(ft5x06_ts);

	INIT_WORK(&ft5x06_ts->work, tsc_work_handler);
	ft5x06_ts->workqueue = create_singlethread_workqueue("ft5x06_tsc");

	client->irq = gpio_to_irq(ft5x06_ts->gpio.irq->num);
	err = request_irq(client->irq, ft5x06_ts_interrupt,
			    IRQF_TRIGGER_FALLING | IRQF_DISABLED,
			  "ft5x06_ts", ft5x06_ts);
	if (err < 0) {
		dev_err(&client->dev, "ft5x06_probe: request irq failed\n");
		goto exit_irq_request_failed;
	}

	ft5x06_ts->irq = client->irq;
	ft5x06_ts->client = client;
	ft5x06_ts->pdata = pdata;
	ft5x06_ts->x_max = pdata->x_max - 1;
	ft5x06_ts->y_max = pdata->y_max - 1;
	ft5x06_ts->is_suspend = 0;

	disable_irq(client->irq);

	input_dev = input_allocate_device();
	if (!input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev, "failed to allocate input device\n");
		goto exit_input_dev_alloc_failed;
	}

	ft5x06_ts->input_dev = input_dev;
	
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
	set_bit(ABS_MT_POSITION_X, input_dev->absbit);
	set_bit(ABS_MT_POSITION_Y, input_dev->absbit);
	set_bit(ABS_MT_TRACKING_ID,input_dev->absbit);
	set_bit(ABS_MT_TOUCH_MAJOR, input_dev->absbit);
	set_bit(ABS_MT_WIDTH_MAJOR, input_dev->absbit);

	input_set_abs_params(input_dev,
			ABS_MT_POSITION_X, 0, ft5x06_ts->x_max, 0, 0);
	input_set_abs_params(input_dev,
			ABS_MT_POSITION_Y, 0, ft5x06_ts->y_max, 0, 0);
	input_set_abs_params(input_dev,
			ABS_MT_TOUCH_MAJOR, 0, 250, 0, 0);
	input_set_abs_params(input_dev,
			ABS_MT_WIDTH_MAJOR, 0, 200, 0, 0);

	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);

	input_dev->name = FT5X06_NAME;
	err = input_register_device(input_dev);
	if (err) {
		dev_err(&client->dev,
			"ft5x06_ts_probe: failed to register input device: %s\n",
			dev_name(&client->dev));
		goto exit_input_register_device_failed;
	}
	
	/*make sure CTP already finish startup process */
	ft5x06_ts_reset(ft5x06_ts);
	msleep(100);

#ifdef CONFIG_HAS_EARLYSUSPEND
	ft5x06_ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ft5x06_ts->early_suspend.suspend = ft5x06_ts_suspend;
	ft5x06_ts->early_suspend.resume	= ft5x06_ts_resume;
	register_early_suspend(&ft5x06_ts->early_suspend);
#endif
	
	/*get some register information */
	uc_reg_addr = FT5X06_REG_FW_VER;
	err = ft5x06_i2c_Read(client, &uc_reg_addr, 1, &uc_reg_value, 1);
	if(err < 0){
		printk("ft5x06_ts  probe failed\n");
		goto exit_input_register_device_failed;
	}
	dev_dbg(&client->dev, "[FTS] Firmware version = 0x%x\n", uc_reg_value);
	
	uc_reg_addr = FT5X06_REG_POINT_RATE;
	err = ft5x06_i2c_Read(client, &uc_reg_addr, 1, &uc_reg_value, 1);
	if(err < 0){
		printk("ft5x06_ts  probe failed\n");
		goto exit_input_register_device_failed;
	}
	dev_dbg(&client->dev, "[FTS] report rate is %dHz.\n",
		uc_reg_value * 10);

	uc_reg_addr = FT5X06_REG_THGROUP;
	ft5x06_i2c_Read(client, &uc_reg_addr, 1, &uc_reg_value, 1);
	if(err < 0){
		printk("ft5x06_ts  probe failed\n");
		goto exit_input_register_device_failed;
	}
	dev_dbg(&client->dev, "[FTS] touch threshold is %d.\n",
		uc_reg_value * 4);
	enable_irq(client->irq);
	return 0;

exit_input_register_device_failed:
	input_free_device(input_dev);

exit_input_dev_alloc_failed:
	free_irq(client->irq, ft5x06_ts);
	gpio_free(ft5x06_ts->gpio.wake->num);

exit_irq_request_failed:
	i2c_set_clientdata(client, NULL);
	kfree(ft5x06_ts);

exit_alloc_data_failed:
exit_check_functionality_failed:
	return err;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void ft5x06_ts_suspend(struct early_suspend *handler)
{
	int ret = 0;
	struct ft5x06_ts_data *ts;
	ts = container_of(handler, struct ft5x06_ts_data,
						early_suspend);
	ts->is_suspend = 1;
	disable_irq(ts->client->irq);
	flush_workqueue(ts->workqueue);
	ret = cancel_work_sync(&ts->work);	
	if (ret) {
		enable_irq(ts->client->irq);
	}
	ft5x06_set_reg(ts, FT5X06_REG_PMODE, PMODE_HIBERNATE);
	dev_dbg(&ts->client->dev, "[FTS]ft5x06 suspend\n");
}

static void ft5x06_ts_resume(struct early_suspend *handler)
{
	struct ft5x06_ts_data *ts = container_of(handler, struct ft5x06_ts_data,
						early_suspend);

	ft5x06_ts_reset(ts);
	ts->is_suspend = 0;

	enable_irq(ts->client->irq);
}
#endif

static int __devexit ft5x06_ts_remove(struct i2c_client *client)
{
	struct ft5x06_ts_data *ft5x06_ts;
	ft5x06_ts = i2c_get_clientdata(client);
	input_unregister_device(ft5x06_ts->input_dev);
	gpio_free(ft5x06_ts->gpio.wake->num);
	free_irq(client->irq, ft5x06_ts);
	kfree(ft5x06_ts);
	i2c_set_clientdata(client, NULL);
	ft5x06_ts_power_off(ft5x06_ts);
	return 0;
}

static const struct i2c_device_id ft5x06_ts_id[] = {
	{FT5X06_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, ft5x06_ts_id);

static struct i2c_driver ft5x06_ts_driver = {
	.probe = ft5x06_ts_probe,
	.remove = __devexit_p(ft5x06_ts_remove),
	.id_table = ft5x06_ts_id,
	.driver = {
		   .name = FT5X06_NAME,
		   .owner = THIS_MODULE,
		   },
};

static int __init ft5x06_ts_init(void)
{
	int ret;
	ret = i2c_add_driver(&ft5x06_ts_driver);
	if (ret) {
		printk(KERN_WARNING "Adding ft5x06 driver failed "
		       "(errno = %d)\n", ret);
	} else {
		pr_info("Successfully added driver %s\n",
			ft5x06_ts_driver.driver.name);
	}
	return ret;
}

static void __exit ft5x06_ts_exit(void)
{
	i2c_del_driver(&ft5x06_ts_driver);
}

module_init(ft5x06_ts_init);
module_exit(ft5x06_ts_exit);

MODULE_AUTHOR("<bcjia>");
MODULE_DESCRIPTION("FocalTech ft5x06 TouchScreen driver");
MODULE_LICENSE("GPL");
