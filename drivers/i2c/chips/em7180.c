/**
 * drivers/i2c/chips/em7180.c
 *
 * Copyright(C)2014 Ingenic Semiconductor Co., LTD.
 * http://www.ingenic.cn
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License.
 */

#include <linux/gsensor.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <linux/linux_sensors.h>

#ifdef dbg
#undef dbg
#endif

#define DEBUG

#define REG_ALGORITHM_CONTROL	0x54
#define REG_HOST_CONTROL	0x34
#define REG_ACCEL_RATE		0x56
#define REG_CURR_ACCEL		0x46
#define REG_RANGE		0x65
#define REG_CURR_RANGE		0x45
#define REG_THRESHOLD_VALUE	0x60
#define REG_CURR_THRESHOLD	0x4F
#define REG_LEN_LOW		0x4B
#define REG_LEN_HIGH		0x4C
#define REG_MODE_REQ		0x5B
#define REG_CURR_MODE		0x4D
#define REG_BYTEREQ_LO		0x5C
#define REG_BYTEREQ_HI		0x5D
#define REG_BYTEREQRESP_LO	0x3A
#define REG_BYTEREQRESP_HI	0x3B
#define REG_BATCHBASE0		0x3C
#define REG_RESET		0x5F
#define REG_CURR_RESET		0x4E

/* A multiple of READ_BUF_SIZE  */
#define DATA_BUF_SIZE		(28 * 1024)
#define READ_BUF_SIZE		(14 * 1024)
#define BATCHBLOCKSIZE		8
#define ALGORITHM_CONTROL_VALUE 0x02
#define HOST_CONTROL_VALUE	0x01
#define BATCH_MODE_VALUE	1
#define UPDATE_MODE_VALUE	0
#define RESET_VALUE		1
#define PREP_RESET_VALUE	0

#define EM7180_NAME	"em7180"

#ifdef DEBUG
#define dbg(fmt, args...) \
	printk("%s:%d: " fmt "\n", __func__, __LINE__, ##args)
#else
#define dbg(fmt, args...)
#endif

extern int pni_sentral_download_firmware_to_ram(struct i2c_client *client);

struct em7180_dev {
	struct i2c_client *i2c_dev;

	unsigned int irq_pin;
	int irq;

	struct completion done;
	struct miscdevice misc_device;
	struct regulator *power;
	struct task_struct *kthread;
	struct mutex lock;

	u8 *data_buf;
	u8 *producer_p;
	u8 *consumer_p;
	u8 *data_buf_base;
	u8 *data_buf_top;
	int buf_len;
};

struct accel_info_t {
	int threshold;
	int frequency;
	int ranges;
};

static irqreturn_t em7180_irq_handler(int irq, void *devid)
{
	struct em7180_dev *em7180 = (struct em7180_dev *)devid;

#ifdef DEBUG
	dbg();
	/* disable irq,To prevent the interruption of abnormal */
	disable_irq_nosync(em7180->irq);
#endif

	complete(&em7180->done);

	return IRQ_HANDLED;
}

static void em7180_write_buff(struct em7180_dev *em7180, int size)
{
	int byteNum = 0, byteReqResp = 0;
	u8 byteNum_Lo = 0, byteNum_Hi = 0, byteReqResp_Lo = 0, byteReqResp_Hi = 0;

	dbg("size = %d", size);
	while (size > byteNum) {
		byteNum_Lo = byteNum & 0xFF;
		byteNum_Hi = (byteNum & 0xFF00) >> 8;
		i2c_smbus_write_byte_data(em7180->i2c_dev, REG_BYTEREQ_LO, byteNum_Lo);
		i2c_smbus_write_byte_data(em7180->i2c_dev, REG_BYTEREQ_HI, byteNum_Hi);

#ifdef DEBUG
		if(byteNum % 14000 == 0) {
			dbg("byteNum = %d", byteNum);
		}
#endif

		udelay(1000);
//		mdelay(1);

		byteReqResp_Lo = i2c_smbus_read_byte_data(em7180->i2c_dev, REG_BYTEREQRESP_LO);
		byteReqResp_Hi = i2c_smbus_read_byte_data(em7180->i2c_dev, REG_BYTEREQRESP_HI);
		byteReqResp = (byteReqResp_Hi << 8) | byteReqResp_Lo;

		if (byteReqResp == byteNum) {
			i2c_smbus_read_i2c_block_data(em7180->i2c_dev, REG_BATCHBASE0, BATCHBLOCKSIZE, em7180->producer_p);
			byteNum += BATCHBLOCKSIZE;
			em7180->producer_p = em7180->producer_p + BATCHBLOCKSIZE;
		} else {
			dbg("Batch Data Error, wait longer please");
		}
	}
	dbg("em7180->producer_p = %p em7180->consumer_p = %p", em7180->producer_p, em7180->consumer_p);
}

static int em7180_read_buff(struct em7180_dev *em7180)
{
	int tmp = 10, ret = 0, status = 0;

	dbg("start read buff");
	i2c_smbus_write_byte_data(em7180->i2c_dev, REG_MODE_REQ, BATCH_MODE_VALUE);
	while (tmp--) {
		mdelay(5);
		status = i2c_smbus_read_byte_data(em7180->i2c_dev, REG_CURR_MODE);
		if (status == BATCH_MODE_VALUE) {
			ret = 1;
			break;
		}
	}
	if (ret == 0) {
		dbg("Fail to switch to BatchMode");
		return ret;
	}

	if (em7180->buf_len < DATA_BUF_SIZE) {
		if (em7180->producer_p >= em7180->consumer_p) {
			dbg();
			em7180_write_buff(em7180, READ_BUF_SIZE);
			if (em7180->producer_p == em7180->data_buf_top)
				em7180->producer_p = em7180->data_buf_base;

			em7180->buf_len += READ_BUF_SIZE;
		} else {
			if ((int)(em7180->consumer_p - em7180->producer_p) <= READ_BUF_SIZE) {
				dbg();
				em7180_write_buff(em7180, READ_BUF_SIZE);
				if (em7180->producer_p == em7180->data_buf_top)
					em7180->producer_p = em7180->data_buf_base;

				em7180->consumer_p = em7180->producer_p;
				em7180->buf_len = DATA_BUF_SIZE;
			} else {
				dbg();
				em7180_write_buff(em7180, READ_BUF_SIZE);

				em7180->buf_len += READ_BUF_SIZE;
			}
		}
	} else {
		dbg();
		em7180_write_buff(em7180, READ_BUF_SIZE);
		if (em7180->producer_p == em7180->data_buf_top)
			em7180->producer_p = em7180->data_buf_base;

		em7180->consumer_p = em7180->producer_p;
		em7180->buf_len = DATA_BUF_SIZE;
	}

	tmp = 10;
	ret = 0;

	i2c_smbus_write_byte_data(em7180->i2c_dev, REG_MODE_REQ, UPDATE_MODE_VALUE);
	while (tmp--) {
		mdelay(5);
		status = i2c_smbus_read_byte_data(em7180->i2c_dev, REG_CURR_MODE);
		if (status == UPDATE_MODE_VALUE) {
			ret = 1;
			dbg("Success to switch to StreamMode");
			break;
		}
	}
	if (ret == 0) {
		dbg("Fail to switch to StreamMode");
		return ret;
	}

	tmp = 10;
	ret = 0;

	i2c_smbus_write_byte_data(em7180->i2c_dev, REG_RESET, RESET_VALUE);
	while (tmp--) {
		mdelay(5);
		status = i2c_smbus_read_byte_data(em7180->i2c_dev, REG_CURR_RESET);
		if (status == RESET_VALUE) {
			ret = 1;
			dbg("Success to clear");
			break;
		}
	}
	if (ret == 0) {
		dbg("Fail to clear");
		return ret;
	}

	tmp = 10;
	ret = 0;

	i2c_smbus_write_byte_data(em7180->i2c_dev, REG_RESET, PREP_RESET_VALUE);
	while (tmp--) {
		mdelay(5);
		status = i2c_smbus_read_byte_data(em7180->i2c_dev, REG_CURR_RESET);
		if (status == PREP_RESET_VALUE) {
			ret = 1;
			dbg("Success to reset");
			break;
		}
	}
	if (ret == 0) {
		dbg("Fail to reset");
		return ret;
	}

	return 1;
}

static int em7180_daemon(void *d)
{
	struct em7180_dev *em7180 = d;
	int ret = 0;

#ifdef DEBUG
	u8 D0_Low, D0_High;
#endif

	while (!kthread_should_stop()) {

		dbg("----------------------------->start thread");

#ifdef DEBUG
		dbg();
		/* enable irq,To prevent the interruption of abnormal */
		enable_irq(em7180->irq);
#endif

		wait_for_completion_interruptible(&em7180->done);

		dbg("----------------------------->start irq");
		mutex_lock(&em7180->lock);
		ret = em7180_read_buff(em7180);
		mutex_unlock(&em7180->lock);

		if (!ret) {
			dbg("need to reset.");
		}

#ifdef DEBUG
		D0_Low = i2c_smbus_read_byte_data(em7180->i2c_dev, 0x4b);
		D0_High = i2c_smbus_read_byte_data(em7180->i2c_dev, 0x4c);
		dbg("D0_High = %x D0_Low = %x em7180->buf_len = %d", D0_High, D0_Low, em7180->buf_len);
#endif

		dbg("em7180->producer_p = %p em7180->consumer_p = %p em7180->buf_len = %d", em7180->producer_p, em7180->consumer_p, em7180->buf_len);
	}

	return 0;
}

static int em7180_open(struct inode *inode, struct file *filp)
{
#ifdef DEBUG
	struct miscdevice *dev = filp->private_data;
	struct em7180_dev *em7180 = container_of(dev, struct em7180_dev, misc_device);

	u8 D0_Low, D0_High;
	D0_Low = i2c_smbus_read_byte_data(em7180->i2c_dev, 0x4b);
	D0_High = i2c_smbus_read_byte_data(em7180->i2c_dev, 0x4c);
	dbg("D0_High = %x D0_Low = %x em7180->buf_len = %d", D0_High, D0_Low, em7180->buf_len);
#endif

	return 0;
}

static int em7180_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t em7180_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	struct miscdevice *dev = filp->private_data;
	struct em7180_dev *em7180 = container_of(dev, struct em7180_dev, misc_device);
	char *dst_buf = NULL;
	int len = 0, len_t = 0, ret = 0;

#ifdef DEBUG
	u8 D0_Low, D0_High;
	D0_Low = i2c_smbus_read_byte_data(em7180->i2c_dev, 0x4b);
	D0_High = i2c_smbus_read_byte_data(em7180->i2c_dev, 0x4c);
	dbg("D0_High = %x D0_Low = %x em7180->buf_len = %d", D0_High, D0_Low, em7180->buf_len);
#endif

	if (buf == NULL || count <= 0) {
		dbg("Invalid argument");
		return -EINVAL;
	}
	if (em7180->buf_len == 0) {
		return 0;
	}

	mutex_lock(&em7180->lock);
	dbg("em7180->producer_p = %p em7180->consumer_p = %p em7180->buf_len = %d", em7180->producer_p, em7180->consumer_p, em7180->buf_len);
	if (count > em7180->buf_len) {
		len_t = em7180->buf_len;
		ret = len_t;
		em7180->buf_len = 0;
	} else {
		len_t = count;
		ret = len_t;
		em7180->buf_len = em7180->buf_len - len_t;
	}

	if((int)(em7180->data_buf_top - em7180->consumer_p) < len_t) {
		dbg("buf = %p", buf);
		if (copy_to_user(buf, em7180->consumer_p, (int)(em7180->data_buf_top - em7180->consumer_p))) {
			ret = -EFAULT;
			goto errorcopy;
		}

		dst_buf = buf + (em7180->data_buf_top - em7180->consumer_p);
		len = len_t - (int)(em7180->data_buf_top - em7180->consumer_p);
		dbg("buf = %p dst_buf = %p len = %d em7180->data_buf_top = %p em7180->consumer_p = %p em7180->data_buf_top - em7180->consumer_p = %d", buf, dst_buf, len, em7180->data_buf_top, em7180->consumer_p, em7180->data_buf_top - em7180->consumer_p);
		if (copy_to_user(dst_buf, em7180->data_buf_base, len)) {
			ret = -EFAULT;
			goto errorcopy;
		}
		em7180->consumer_p = em7180->data_buf_base + len;
		dbg("em7180->producer_p = %p em7180->consumer_p = %p em7180->buf_len = %d len_t = %d", em7180->producer_p, em7180->consumer_p, em7180->buf_len,len_t);
	} else {
		if (copy_to_user(buf, em7180->consumer_p, len_t)) {
			ret = -EFAULT;
			goto errorcopy;
		}
		em7180->consumer_p += len_t;
		dbg("em7180->producer_p = %p em7180->consumer_p = %p em7180->buf_len = %d len_t = %d", em7180->producer_p, em7180->consumer_p, em7180->buf_len,len_t);
	}

errorcopy:
	mutex_unlock(&em7180->lock);
	return ret;
}

static long em7180_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = filp->private_data;
	struct em7180_dev *em7180 = container_of(dev, struct em7180_dev, misc_device);
	struct accel_info_t accel_info;
	int tmp = 10,ret = 0, status = 0;

	if (copy_from_user(&accel_info, (void __user *)arg, sizeof(accel_info))) {
		dbg("Copy_from_user failed");
		return -EFAULT;
	}

	switch (cmd) {
	case SENSOR_IOCTL_SET:
		ret = i2c_smbus_write_byte_data(em7180->i2c_dev, REG_ALGORITHM_CONTROL, ALGORITHM_CONTROL_VALUE);
		if (ret)
			return ret;

		ret = i2c_smbus_write_byte_data(em7180->i2c_dev, REG_HOST_CONTROL, HOST_CONTROL_VALUE);
		if (ret)
			return ret;

		if (accel_info.frequency < 20) {
			accel_info.frequency = 1;
		} else if (accel_info.frequency < 30) {
			accel_info.frequency = 2;
		} else if (accel_info.frequency < 40) {
			accel_info.frequency = 3;
		} else if (accel_info.frequency < 50) {
			accel_info.frequency = 4;
		} else if (accel_info.frequency < 60) {
			accel_info.frequency = 5;
		} else {
			accel_info.frequency = 6;
		}

		if (accel_info.ranges < 4) {
			accel_info.ranges = 2;
		} else if (accel_info.ranges < 8) {
			accel_info.ranges = 4;
		} else if (accel_info.ranges < 16) {
			accel_info.ranges = 8;
		} else {
			accel_info.ranges = 16;
		}

		if (accel_info.threshold < 0 || accel_info.threshold > 512)
			accel_info.threshold = 0x14;

		/*set_frequency*/
		ret = i2c_smbus_write_byte_data(em7180->i2c_dev, REG_ACCEL_RATE, accel_info.frequency);
		if (ret)
			return ret;

		tmp = 10;
		ret = 0;

		while (tmp--) {
			msleep(5);
			status = i2c_smbus_read_byte_data(em7180->i2c_dev, REG_CURR_ACCEL);
			if (status == accel_info.frequency) {
				dbg("Set accel value success! accel value = %x",status);
				ret = 1;
				break;
			}
		}
		if (ret == 0) {
			dbg("Set accel value failed! accel value = %x",status);
			return -EINVAL;
		}

		/*set_ranges*/
		ret = i2c_smbus_write_byte_data(em7180->i2c_dev, REG_RANGE, accel_info.ranges);
		if (ret)
			return ret;

		tmp = 10;
		ret = 0;

		while (tmp--) {
			msleep(5);
			status = i2c_smbus_read_byte_data(em7180->i2c_dev, REG_CURR_RANGE);
			if (status == accel_info.ranges) {
				dbg("Set ranges value success! ranges value = %x",status);
				ret = 1;
				break;
			}
		}
		if (ret == 0) {
			dbg("Set ranges value failed! ranges value = %x",status);
//			return -EINVAL;
		}

		/*set_threshold*/
		ret = i2c_smbus_write_byte_data(em7180->i2c_dev, REG_THRESHOLD_VALUE, accel_info.threshold);
		if (ret)
			return ret;

		tmp = 10;
		ret = 0;

		while (tmp--) {
			msleep(5);
			status = i2c_smbus_read_byte_data(em7180->i2c_dev, REG_CURR_THRESHOLD);
			if (status == accel_info.threshold) {
				dbg("Set threshold value success! threshold value = %x",status);
				wake_up_process(em7180->kthread);
				return 0;
			}
		}
		if (ret == 0) {
			dbg("Set threshold value failed! threshold value = %x",status);
			return -EINVAL;
		}
	default:
		return -EINVAL;
	}
}

struct file_operations em7180_fops = {
	.owner = THIS_MODULE,
	.open = em7180_open,
	.read = em7180_read,
	.release = em7180_release,
	.unlocked_ioctl = em7180_ioctl,
};

static int em7180_probe(struct i2c_client *i2c_dev, const struct i2c_device_id *id)
{
	struct em7180_dev *em7180;
	int ret = 0;

	em7180 = kzalloc(sizeof(struct em7180_dev), GFP_KERNEL);
	if (!em7180) {
		dev_err(&i2c_dev->dev, "Failed to allocate driver structre");
		ret = -ENOMEM;
		goto err_free;
	}

	em7180->power = regulator_get(NULL, "vcc_sensor1v8");
	if (!IS_ERR(em7180->power)) {
		regulator_enable(em7180->power);
	} else {
		dev_err(&i2c_dev->dev, "failed to get regulator vcc_sensor1v8");
		ret = -EINVAL;
		goto err_regulator;
	}

	em7180->irq_pin = GPIO_PA(15);

	ret = gpio_request(em7180->irq_pin, "em7180_irq_pin");
	if (ret) {
		dev_err(&i2c_dev->dev, "GPIO request failed");
		goto err_regulator;
	}

	ret = gpio_direction_input(em7180->irq_pin);
	if (ret < 0) {
		dev_err(&i2c_dev->dev, "unable to set GPIO direction, err=%d", ret);
		goto err_gpio;
	}

	ret = em7180->irq = gpio_to_irq(em7180->irq_pin);
	if (ret < 0) {
		dev_err(&i2c_dev->dev, "cannot find IRQ");
		goto err_gpio;
	}

	mutex_init(&em7180->lock);
	init_completion(&em7180->done);

	ret = request_irq(em7180->irq, em7180_irq_handler, IRQF_TRIGGER_RISING, i2c_dev->name, em7180);
	if (ret != 0) {
		dev_err(&i2c_dev->dev, "cannot claim IRQ %d", em7180->irq);
		goto err_irq;
	}
	disable_irq(em7180->irq);

	em7180->data_buf = (u8 *)__get_free_pages(GFP_KERNEL, get_order(DATA_BUF_SIZE));
	if (em7180->data_buf == NULL) {
		dev_err(&i2c_dev->dev, "malloc em7180 data buf error");
		ret = -ENOMEM;
		goto err_buf;
	}

	em7180->data_buf_base = em7180->data_buf;
	em7180->data_buf_top = em7180->data_buf + DATA_BUF_SIZE;
	em7180->producer_p = em7180->data_buf;
	em7180->consumer_p = em7180->data_buf;
	em7180->buf_len = 0;
	dbg("em7180->data_buf_base = %p em7180->data_buf_top = %p em7180->producer_p = %p em7180->consumer_p = %p em7180->buf_len = %d",em7180->data_buf_base,em7180->data_buf_top,em7180->producer_p,em7180->consumer_p,em7180->buf_len);

	em7180->kthread = kthread_create(em7180_daemon, em7180, "em7180_daemon");
	if (IS_ERR(em7180->kthread)) {
		ret = -ENOMEM;
		goto err_buf;
	}

	em7180->i2c_dev = i2c_dev;
	em7180->misc_device.minor = MISC_DYNAMIC_MINOR;
	em7180->misc_device.name = EM7180_NAME;
	em7180->misc_device.fops = &em7180_fops;

	misc_register(&em7180->misc_device);
	if (ret < 0) {
		dev_err(&i2c_dev->dev, "misc_register failed");
		goto err_register_misc;
	}

	pni_sentral_download_firmware_to_ram(em7180->i2c_dev);

	i2c_set_clientdata(i2c_dev, em7180);

	return 0;

err_register_misc:
	misc_deregister(&em7180->misc_device);
err_buf :
	kfree(em7180->data_buf);
err_irq :
	free_irq(em7180->irq, em7180);
err_gpio :
	gpio_free(em7180->irq_pin);
err_regulator :
	regulator_put(em7180->power);
err_free :
	kfree(i2c_dev);

	return ret;
}

static int __devexit em7180_remove(struct i2c_client *i2c_dev)
{
	struct em7180_dev *em7180 = i2c_get_clientdata(i2c_dev);

	misc_deregister(&em7180->misc_device);
	kfree(em7180->data_buf);
	free_irq(em7180->irq, em7180);
	gpio_free(em7180->irq_pin);
	regulator_put(em7180->power);
	kfree(em7180);

	return 0;
}

static const struct i2c_device_id em7180_i2c_id[] = {
	{EM7180_NAME, 0},
	{}
};

static struct i2c_driver em7180_driver = {
	.driver = {
		.name  = EM7180_NAME,
		.owner = THIS_MODULE,
	},
	.probe	       = em7180_probe,
	.remove        = __devexit_p(em7180_remove),
	.id_table      = em7180_i2c_id,
};

static int __init em7180_init(void)
{
	return i2c_add_driver(&em7180_driver);
}
module_init(em7180_init);

static void __exit em7180_exit(void)
{
	i2c_del_driver(&em7180_driver);
}
module_exit(em7180_exit);

MODULE_ALIAS("i2c:em7180");
MODULE_AUTHOR("Bomyy Liu<ybliu_hf@ingenic.cn>");
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("JZ m200 sentral driver");
