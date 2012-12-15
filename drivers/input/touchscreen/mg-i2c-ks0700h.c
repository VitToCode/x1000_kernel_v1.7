/*
 * drivers/input/touchscreen/mg-i2c-ks0700h.c
 *
 * driver for KS0700h ---- a magnetic touch screen
 *
 * Copyright (C) 2012 Fighter Sun<wanmyqawdr@126.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/tsc.h>

#define MG_DRIVER_NAME  "mg-i2c-ts"

#define COORD_INTERPRET(MSB_BYTE, LSB_BYTE) \
        (MSB_BYTE << 8 | LSB_BYTE)

#define CAP_X_CORD              14000
#define CAP_Y_CORD              8200
#define DIG_MAX_P               0xff

#define BUF_SIZE                8

enum mg_capac_report {
	MG_MODE = 0x0,
	MG_CONTACT_ID,
	MG_STATUS,
	MG_POS_X_LOW,
	MG_POS_X_HI,
	MG_POS_Y_LOW,
	MG_POS_Y_HI,
	MG_POINTS,

	MG_MODE2,
	MG_CONTACT_ID2,
	MG_STATUS2,
	MG_POS_X_LOW2,
	MG_POS_X_HI2,
	MG_POS_Y_LOW2,
	MG_POS_Y_HI2,

	MG_POINTS2,
};

enum mg_dig_report {
	MG_DIG_MODE = 0x0,
	MG_DIG_STATUS, MG_DIG_X_LOW, MG_DIG_X_HI, MG_DIG_Y_LOW, MG_DIG_Y_HI,
	/* Z represents pressure value */
	MG_DIG_Z_LOW, MG_DIG_Z_HI,
};

enum mg_dig_state {
	MG_OUT_RANG = 0,
	MG_IN_RANG = 0x10,
	MG_TIP_SWITCH = 0x11,
	MG_BA_SWITCH = 0x12,
	MG_RIGHT_BTN = 0x13,
};

static int prev_s = -1;
static int current_s = -1;
static int prev_p = -1;
static int current_p = -1;
static int flag_menu = -1;
static int flag_home = -1;
static int flag_back = -1;

struct mg_data {
	uint32_t x, y, w, p, id;
	struct i2c_client *client;
	struct input_dev *dig_dev;
	int irq;
	int ss;
	struct workqueue_struct *i2c_workqueue;
	struct work_struct work;
	struct regulator *power;

	struct jztsc_platform_data *pdata;
};

static struct i2c_driver mg_driver;

static irqreturn_t mg_irq(int irq, void *_mg) {
	struct mg_data *mg = _mg;

	if (!work_pending(&mg->work)) {
		disable_irq_nosync(mg->irq);
		queue_work(mg->i2c_workqueue, &mg->work);
	}

	return IRQ_HANDLED;
}

static void report_value(struct mg_data *mg) {
	input_mt_slot(mg->dig_dev, 0);
	input_mt_report_slot_state(mg->dig_dev, MT_TOOL_FINGER, true);
	input_report_abs(mg->dig_dev, ABS_MT_TOUCH_MAJOR, 1);
	input_report_abs(mg->dig_dev, ABS_MT_POSITION_X, mg->x);
	input_report_abs(mg->dig_dev, ABS_MT_POSITION_Y, mg->y);
	input_report_abs(mg->dig_dev, ABS_MT_PRESSURE, mg->p >> 2);

	input_sync(mg->dig_dev);
}

static void report_menu(struct mg_data *mg) {
	//TODO: Fill me in
}

static void report_home(struct mg_data *mg) {
	//TODO: Fill me in
}

static void report_back(struct mg_data *mg) {
	//TODO: Fill me in
}

static void report_release(struct mg_data *mg) {
	input_mt_slot(mg->dig_dev, 0);
	input_mt_report_slot_state(mg->dig_dev, MT_TOOL_FINGER, false);

	input_sync(mg->dig_dev);
}

static inline void remap_to_view_size(struct mg_data *mg) {
	mg->x = mg->x * mg->pdata->x_max / CAP_X_CORD;
	mg->y = mg->y * mg->pdata->y_max / CAP_Y_CORD;
}

static inline void mg_process_data(struct mg_data *mg) {
	static int changed;
	static int saved_x;
	static int saved_y;

	prev_s = current_s;
	current_s = mg->w;
	prev_p = current_p;
	current_p = mg->p;

	remap_to_view_size(mg);

	switch (current_s) {
	case MG_OUT_RANG: //0x0
		if (flag_menu == 1) {
			flag_menu = 0;
			report_menu(mg);
		} else if (flag_home == 1) {
			flag_home = 0;
			report_home(mg);
		} else if (flag_back == 1) {
			flag_back = 0;
			report_back(mg);
		} else {
			if (changed) {
				report_release(mg);
				changed = 0;
			}
		}

		break;
	case MG_IN_RANG: //0x10
		if (flag_menu == 1) {
			flag_menu = 0;
			report_menu(mg);
		} else if (flag_back == 1) {
			flag_back = 0;
			report_back(mg);
		} else {
			if (changed) {
				report_release(mg);
				changed = 0;
			}
		}
		break;
	case MG_TIP_SWITCH: //0x11
		if (saved_x != mg->x && saved_y != mg->y) {
			report_value(mg);
			saved_x = mg->x;
			saved_y = mg->y;
			changed = 1;
		}
		break;
	case MG_BA_SWITCH: //0x12
		if (prev_s == MG_IN_RANG) {
			flag_menu = 1;
		}
		break;
	case MG_RIGHT_BTN: //0x13
		if (flag_home == 1) {
			flag_home = 0;
			report_home(mg);
		} else {
			if (prev_s == MG_IN_RANG) {
				flag_back = 1;
			} else if (prev_s == MG_RIGHT_BTN) {
				if (prev_p == 0 && current_p > 0) {
					flag_home = 1;
					flag_back = 0;
				}
			}
		}
		break;
	default:
		break;
	}
}

static void mg_i2c_work(struct work_struct *work) {
	int i = 0;
	struct mg_data *mg = container_of(work, struct mg_data, work);
	u_int8_t ret = 0;
	u8 read_buf[BUF_SIZE]; /* buffer for first point of multitouch */

	for (i = 0; i < BUF_SIZE; i++)
		read_buf[i] = 0;

	ret = i2c_smbus_read_i2c_block_data(mg->client, 0x0, BUF_SIZE, read_buf);

	if (ret < 0) {
		printk("Read error!!!!!\n");
		goto err_enable_irq;
	}

	if (read_buf[MG_MODE] >> 1) {
		mg->x = COORD_INTERPRET(read_buf[MG_DIG_X_HI], read_buf[MG_DIG_X_LOW]); //3,2
		mg->y = COORD_INTERPRET(read_buf[MG_DIG_Y_HI], read_buf[MG_DIG_Y_LOW]); //5,4
		mg->w = read_buf[MG_DIG_STATUS]; //1
		mg->p =
				(COORD_INTERPRET(read_buf[MG_DIG_Z_HI], read_buf[MG_DIG_Z_LOW]));

		mg_process_data(mg);
	}

err_enable_irq:
	enable_irq(mg->irq);
}

static int mg_probe(struct i2c_client *client, const struct i2c_device_id *ids) {
	struct jztsc_platform_data *pdata = client->dev.platform_data;
	struct mg_data *mg;
	struct input_dev *input_dig;
	int err = 0;

	mg = kzalloc(sizeof(struct mg_data), GFP_KERNEL);
	if (!mg)
		return -ENOMEM;

	mg->pdata = pdata;
	mg->power = regulator_get(&client->dev, "vtsc");
	if (IS_ERR(mg->power)) {
		err = -EIO;
		dev_err(&mg->client->dev, "Failed to request regulator\n");
		goto err_kfree;
	}
	regulator_enable(mg->power);

	mg->ss = pdata->gpio[0].num;
	err = gpio_request(mg->ss, "mg_ts_int");
	if (err) {
		err = -EIO;
		dev_err(&mg->client->dev, "Failed to request GPIO: %d\n", mg->ss);
		goto err_kfree;
	}
	gpio_direction_input(mg->ss);

	mg->client = client;
	i2c_set_clientdata(client, mg);

	input_dig = input_allocate_device();
	if (!input_dig) {
		err = -ENOMEM;
		dev_err(&mg->client->dev, "Failed to allocate input device.\n");
		goto err_gpio_free;
	}

	set_bit(EV_ABS, input_dig->evbit);
	set_bit(INPUT_PROP_DIRECT, input_dig->propbit);

	/* assigned with 2 even we only got 1 point, this may be a bug of input subsys */
	input_mt_init_slots(input_dig, 2);
	input_set_abs_params(input_dig, ABS_MT_POSITION_X, 0, pdata->x_max, 0, 0);
	input_set_abs_params(input_dig, ABS_MT_POSITION_Y, 0, pdata->y_max, 0, 0);
	input_set_abs_params(input_dig, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dig, ABS_MT_PRESSURE, 0, DIG_MAX_P, 0, 0);

	input_dig->name = MG_DRIVER_NAME;
	input_dig->id.bustype = BUS_I2C;

	err = input_register_device(input_dig);
	if (err) {
		dev_err(&mg->client->dev, "Failed to register input device.\n");
		goto err_regulator_put;
	}

	mg->dig_dev = input_dig;

	INIT_WORK(&mg->work, mg_i2c_work);
	mg->i2c_workqueue = create_singlethread_workqueue(dev_name(&client->dev));
	if (!mg->i2c_workqueue) {
		err = -ESRCH;
		goto err_input_unregister;
	}

	mg->irq = gpio_to_irq(mg->ss);
	err = request_irq(mg->irq, mg_irq, IRQF_TRIGGER_FALLING | IRQF_DISABLED,
			MG_DRIVER_NAME, mg);
	if (err) {
		dev_err(&mg->client->dev, "Failed to request irq.\n");
		goto err_input_unregister;
	}

	return 0;

err_input_unregister:
	input_unregister_device(mg->dig_dev);

err_gpio_free:
	gpio_free(mg->ss);

err_regulator_put:
	regulator_put(mg->power);

err_kfree:
	kfree(mg);

	return err;

}

static int __devexit mg_remove(struct i2c_client *client) {
	struct mg_data *mg = i2c_get_clientdata(client);

	free_irq(mg->irq, mg);
	input_unregister_device(mg->dig_dev);
	gpio_free(mg->ss);
	regulator_put(mg->power);
	kfree(mg);

	return 0;
}

static struct i2c_device_id mg_id_table[] = {
	{ MG_DRIVER_NAME, 0 },
	{ }
};

static struct i2c_driver mg_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = MG_DRIVER_NAME,
	},

	.id_table = mg_id_table,
	.probe = mg_probe,
	.remove = mg_remove,
};

static int __init mg_init(void) {
	return i2c_add_driver(&mg_driver);
}

static void mg_exit(void) {
	i2c_del_driver(&mg_driver);
}

module_init(mg_init);
module_exit(mg_exit);

MODULE_AUTHOR("Fighter Sun<wanmyqawdr@126.com>");
