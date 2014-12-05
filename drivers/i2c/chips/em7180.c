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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/gsensor.h>
#include <linux/i2c.h>
#include <linux/spi/spi.h>
#include <linux/debugfs.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <linux/linux_sensors.h>
#include <linux/i2c/em7180.h>

struct em7180_dev {
	struct i2c_client *i2c_dev;

	u32 irq_pin;
	int irq;
	int irq_flag;
	int wakeup;

	struct completion done;
	struct miscdevice misc_device;
	struct regulator *power;
	struct task_struct *kthread;
	struct mutex lock;
	struct accel_info_t accel_info;

	u8 *data_buf;
	u8 *producer;
	u8 *consumer;
	u8 *buf_base;
	u8 *buf_top;
	u32 buf_len;
};

static void em7180_write_buff(struct em7180_dev *em7180, int size)
{
	int byteNum = 0, byteReqResp = 0;
	u8 byteNum_Lo = 0, byteNum_Hi = 0,
		byteReqResp_Lo = 0, byteReqResp_Hi = 0;

	dbg("size = %d", size);
	while (size > byteNum) {
		byteNum_Lo = byteNum & 0xFF;
		byteNum_Hi = (byteNum & 0xFF00) >> 8;
		i2c_smbus_write_byte_data(em7180->i2c_dev,
					  REG_BYTEREQ_LO,
					  byteNum_Lo);
		i2c_smbus_write_byte_data(em7180->i2c_dev,
					  REG_BYTEREQ_HI,
					  byteNum_Hi);

#ifdef EM7180_DEBUG
		if(0 == byteNum % 14000)
			dbg("byteNum = %d", byteNum);
#endif

		udelay(500);

		byteReqResp_Lo = i2c_smbus_read_byte_data(em7180->i2c_dev,
							  REG_BYTEREQRESP_LO);
		byteReqResp_Hi = i2c_smbus_read_byte_data(em7180->i2c_dev,
							  REG_BYTEREQRESP_HI);
		byteReqResp = (byteReqResp_Hi << 8) | byteReqResp_Lo;

		if (byteNum == byteReqResp) {
			i2c_smbus_read_i2c_block_data(em7180->i2c_dev,
						      REG_BATCHBASE0,
						      BATCHBLOCKSIZE,
						      em7180->producer);
			byteNum += BATCHBLOCKSIZE;
			em7180->producer = em7180->producer + BATCHBLOCKSIZE;
		} else {
			dbg("Batch Data Error, wait longer please");
		}
	}
	dbg("em7180->producer = %p em7180->consumer = %p",
	    em7180->producer, em7180->consumer);
}

static int em7180_clear_irq(struct em7180_dev *em7180)
{
	int tmp = 10, ret = 0, status = 0;

	i2c_smbus_write_byte_data(em7180->i2c_dev, REG_MODE_REQ,
				  UPDATE_MODE_VALUE);
	while (tmp--) {
		mdelay(5);
		status = i2c_smbus_read_byte_data(em7180->i2c_dev,
						  REG_CURR_MODE);
		if (UPDATE_MODE_VALUE == status) {
			ret = 1;
			dbg("Success to switch to StreamMode");
			break;
		}
	}
	if (0 == ret) {
		dbg("Fail to switch to StreamMode");
		return ret;
	}

	tmp = 10;
	ret = 0;

	i2c_smbus_write_byte_data(em7180->i2c_dev, REG_RESET_DATA,
				  RESET_DATA_VALUE);
	while (tmp--) {
		mdelay(5);
		status = i2c_smbus_read_byte_data(em7180->i2c_dev,
						  REG_CURR_RESET);
		if (RESET_DATA_VALUE == status) {
			ret = 1;
			dbg("Success to clear");
			break;
		}
	}
	if (0 == ret) {
		dbg("Fail to clear");
		return ret;
	}

	tmp = 10;
	ret = 0;

	i2c_smbus_write_byte_data(em7180->i2c_dev, REG_RESET_DATA,
				  PREP_RESET_VALUE);
	while (tmp--) {
		mdelay(5);
		status = i2c_smbus_read_byte_data(em7180->i2c_dev,
						  REG_CURR_RESET);
		if (PREP_RESET_VALUE == status) {
			ret = 1;
			dbg("Success to reset");
			break;
		}
	}
	if (0 == ret) {
		dbg("Fail to reset");
	}
	return ret;
}

static int em7180_read_buff(struct em7180_dev *em7180)
{
	int tmp = 10, ret = 0, status = 0;

	dbg("start read buff");
	i2c_smbus_write_byte_data(em7180->i2c_dev, REG_MODE_REQ,
				  BATCH_MODE_VALUE);
	while (tmp--) {
		mdelay(5);
		status = i2c_smbus_read_byte_data(em7180->i2c_dev,
						  REG_CURR_MODE);
		if (BATCH_MODE_VALUE == status) {
			ret = 1;
			break;
		}
	}
	if (0 == ret) {
		dbg("Fail to switch to BatchMode");
		return ret;
	}

	/* Read data from em7180 */
	if (em7180->buf_len < DATA_BUF_SIZE) {
		if (em7180->producer >= em7180->consumer) {
			dbg("+++++++++++++++++++++++++++++++++++++++++++test0");

			em7180_write_buff(em7180, READ_BUF_SIZE);
			if (em7180->buf_top == em7180->producer)
				em7180->producer = em7180->buf_base;

			em7180->buf_len += READ_BUF_SIZE;
		} else {
			if ((int)(em7180->consumer - em7180->producer)
			    <= READ_BUF_SIZE) {
				dbg("-----------------------------------test1");

				em7180_write_buff(em7180, READ_BUF_SIZE);
				if (em7180->buf_top == em7180->producer)
					em7180->producer = em7180->buf_base;

				em7180->consumer = em7180->producer;
				em7180->buf_len = DATA_BUF_SIZE;
			} else {
				dbg("***********************************test2");

				em7180_write_buff(em7180, READ_BUF_SIZE);

				em7180->buf_len += READ_BUF_SIZE;
			}
		}
	} else {
		dbg("///////////////////////////////////////////////////test3");

		em7180_write_buff(em7180, READ_BUF_SIZE);
		if (em7180->buf_top == em7180->producer)
			em7180->producer = em7180->buf_base;

		em7180->consumer = em7180->producer;
		em7180->buf_len = DATA_BUF_SIZE;
	}

	ret = em7180_clear_irq(em7180);

	return ret;
}

static int em7180_open(struct inode *inode, struct file *filp)
{
#ifdef EM7180_DEBUG
	struct miscdevice *dev = filp->private_data;
	struct em7180_dev *em7180 =
		container_of(dev, struct em7180_dev, misc_device);

	u8 D0_Low = 0, D0_High = 0, D0_tmp = 0;
	D0_Low = i2c_smbus_read_byte_data(em7180->i2c_dev, REG_LEN_LOW);
	D0_High = i2c_smbus_read_byte_data(em7180->i2c_dev, REG_LEN_HIGH);
	D0_tmp = i2c_smbus_read_byte_data(em7180->i2c_dev, 0x3c);
	dbg("D0_High = %x D0_Low = %x D0_tmp = %x em7180->buf_len = %d",
	    D0_High, D0_Low, D0_tmp, em7180->buf_len);
#endif

	return 0;
}

static int em7180_release(struct inode *inode, struct file *file)
{
	return 0;
}

static ssize_t
em7180_read(struct file *filp, char *buf, size_t count, loff_t *f_pos)
{
	struct miscdevice *dev = filp->private_data;
	struct em7180_dev *em7180 =
		container_of(dev, struct em7180_dev, misc_device);

	char *dst_buf = NULL;
	int len = 0, len_t = 0, ret = 0;

#ifdef EM7180_DEBUG

	u8 D0_Low = 0, D0_High = 0;
	D0_Low = i2c_smbus_read_byte_data(em7180->i2c_dev, REG_LEN_LOW);
	D0_High = i2c_smbus_read_byte_data(em7180->i2c_dev, REG_LEN_HIGH);

	dbg("D0_High = %x D0_Low = %x  em7180->buf_len = %d irq_flag = %d",
	    D0_High, D0_Low, em7180->buf_len, em7180->irq_flag);
#endif

	if (NULL == buf || count <= 0) {
		dbg("Invalid argument");
		return -EINVAL;
	}
	if (0 == em7180->buf_len) {
		return 0;
	}

	mutex_lock(&em7180->lock);
	dbg("em7180->producer = %p em7180->consumer = %p em7180->buf_len = %d",
	    em7180->producer, em7180->consumer, em7180->buf_len);
	if (count > em7180->buf_len) {
		len_t = em7180->buf_len;
		ret = len_t;
		em7180->buf_len = 0;
	} else {
		len_t = count;
		ret = len_t;
		em7180->buf_len = em7180->buf_len - len_t;
	}

	if((int)(em7180->buf_top - em7180->consumer) < len_t) {
		dbg("buf = %p", buf);
		if (copy_to_user(buf, em7180->consumer,
				 (int)(em7180->buf_top - em7180->consumer))) {
			ret = -EFAULT;
			goto errorcopy;
		}

		dst_buf = buf + (em7180->buf_top - em7180->consumer);
		len = len_t - (int)(em7180->buf_top - em7180->consumer);
		dbg("buf = %p dst_buf = %p len = %d  consumer = %p "
		    "buf_top - consumer = %d",
		    buf, dst_buf, len, em7180->consumer,
		    em7180->buf_top - em7180->consumer);
		if (copy_to_user(dst_buf, em7180->buf_base, len)) {
			ret = -EFAULT;
			goto errorcopy;
		}
		em7180->consumer = em7180->buf_base + len;
		dbg("em7180->producer = %p em7180->consumer = %p "
		    "em7180->buf_len = %d len_t = %d",
		    em7180->producer, em7180->consumer, em7180->buf_len,len_t);
	} else {
		if (copy_to_user(buf, em7180->consumer, len_t)) {
			ret = -EFAULT;
			goto errorcopy;
		}
		em7180->consumer += len_t;
		dbg("em7180->producer = %p em7180->consumer = %p "
		    "em7180->buf_len = %d len_t = %d",
		    em7180->producer, em7180->consumer, em7180->buf_len,len_t);
	}

errorcopy:
	mutex_unlock(&em7180->lock);

	return ret;
}

static int
em7180_set_parameters(struct em7180_dev *em7180, struct accel_info_t accel_info)
{
	int tmp = 10, ret = 1, status = 0, frequency_t = 0;

	ret = i2c_smbus_write_byte_data(em7180->i2c_dev,
					REG_ALGORITHM_CONTROL,
					ALGORITHM_CONTROL_VALUE);
	if (ret)
		return ret;

	ret = i2c_smbus_write_byte_data(em7180->i2c_dev,
					REG_HOST_CONTROL,
					HOST_CONTROL_VALUE);
	if (ret)
		return ret;

	if (accel_info.frequency < 32) {
		accel_info.frequency = 1;
		frequency_t = 2;
	} else if (accel_info.frequency < 64) {
		accel_info.frequency = 2;
		frequency_t = 4;
	} else {
		accel_info.frequency = 3;
		frequency_t = 8;
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

	if (accel_info.threshold < 0) {
		accel_info.threshold = 0;
	} else if(accel_info.threshold > 512) {
		accel_info.threshold = 20;
	}

	dbg("accel_info : frequency = %d ranges = %d threshold = %d",
	    accel_info.frequency, accel_info.ranges, accel_info.threshold);

	/*set_frequency*/
	ret = i2c_smbus_write_byte_data(em7180->i2c_dev,
					REG_ACCEL_RATE,
					accel_info.frequency);
	if (ret)
		return ret;

	tmp = 10;

	while (tmp--) {
		msleep(5);
		status = i2c_smbus_read_byte_data(em7180->i2c_dev,
						  REG_CURR_ACCEL);
		if (frequency_t == status) {
			dbg("Set accel value success! accel value = %d",status);
			accel_info.frequency = status;
			ret = 1;
			break;
		}
	}
	if (0 == ret) {
		dbg("Set accel value failed! accel value = %d",status);
		return -EINVAL;
	}

	/*set_ranges*/
	ret = i2c_smbus_write_byte_data(em7180->i2c_dev,
					REG_RANGE,
					accel_info.ranges);
	if (ret)
		return ret;

	tmp = 10;

	while (tmp--) {
		msleep(5);
		status = i2c_smbus_read_byte_data(em7180->i2c_dev,
						  REG_CURR_RANGE);
		if (accel_info.ranges == status) {
			dbg("Set ranges value success! ranges value = %x",
			    status);
			ret = 1;
			break;
		}
	}
	if (0 == ret) {
		dbg("Set ranges value failed! ranges value = %x",status);
			return -EINVAL;
	}

	/*set_threshold*/
	ret = i2c_smbus_write_byte_data(em7180->i2c_dev,
					REG_THRESHOLD_VALUE,
					accel_info.threshold);
	if (ret)
		return ret;

	tmp = 10;

	while (tmp--) {
		msleep(5);
		status = i2c_smbus_read_byte_data(em7180->i2c_dev,
						  REG_CURR_THRESHOLD);
		if (accel_info.threshold == status) {
			dbg("Set threshold value success! threshold value = %d",
			    status);
			wake_up_process(em7180->kthread);
			return ret;
		}
	}

	dbg("Set threshold value failed! threshold value = %d",status);
	return -EINVAL;
}

static long
em7180_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct miscdevice *dev = filp->private_data;
	struct em7180_dev *em7180 =
		container_of(dev, struct em7180_dev, misc_device);

	int ret = 0;

	if (copy_from_user(&(em7180->accel_info),
			   (void __user *)arg,
			   sizeof(struct accel_info_t))) {
		dbg("Copy_from_user failed");
		return -EFAULT;
	}

	switch (cmd) {
	case SENSOR_IOCTL_SET:
		dbg("accel_info : frequency = %p ranges = %p threshold = %p",
		    &em7180->accel_info.frequency, &em7180->accel_info.ranges,
		    &em7180->accel_info.threshold);
		ret = em7180_set_parameters(em7180, em7180->accel_info);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int em7180_daemon(void *d)
{
	struct em7180_dev *em7180 = d;
	int ret = 0;
	u8 D0_Low = 0, D0_High = 0;
	u16 D0_All = 0;

	while (!kthread_should_stop()) {
		dbg("----------------------------->start thread");

		/* enable irq,To prevent the interruption of abnormal */
		enable_irq(em7180->irq);
		wait_for_completion_interruptible(&em7180->done);

		/* When em7180->irq_flag == 1 and
		   buff is not full clear the first interrupt */
		D0_Low = i2c_smbus_read_byte_data(em7180->i2c_dev,
						  REG_LEN_LOW);
		D0_High = i2c_smbus_read_byte_data(em7180->i2c_dev,
						   REG_LEN_HIGH);
		D0_All = (D0_High << 8) | D0_Low;
		dbg("D0_High = %x D0_Low = %x D0_All = %d em7180->buf_len = %d",
		    D0_High, D0_Low, D0_All, em7180->buf_len);

		if (1 == em7180->irq_flag && D0_All != READ_BUF_SIZE) {
			em7180_clear_irq(em7180);
		} else {
			dbg("----------------------------->start irq");
			mutex_lock(&em7180->lock);
			ret = em7180_read_buff(em7180);
			mutex_unlock(&em7180->lock);

			if (!ret) {
				i2c_smbus_write_byte_data(em7180->i2c_dev,
							  REG_RESET,
							  RESET_VALUE);

				pni_sentral_download_firmware_to_ram(em7180->i2c_dev);

				em7180_set_parameters(em7180,
						      em7180->accel_info);
			}

#ifdef EM7180_DEBUG
			D0_Low = i2c_smbus_read_byte_data(em7180->i2c_dev,
							  REG_LEN_LOW);
			D0_High = i2c_smbus_read_byte_data(em7180->i2c_dev,
							   REG_LEN_HIGH);
			dbg("D0_High = %x D0_Low = %x em7180->buf_len = %d",
			    D0_High, D0_Low, em7180->buf_len);
#endif

			dbg("em7180->producer = %p consumer = %p buf_len = %d",
			    em7180->producer, em7180->consumer,
			    em7180->buf_len);
		}
		em7180->irq_flag = 0;
	}

	return 0;
}

static irqreturn_t em7180_irq_handler(int irq, void *devid)
{
	struct em7180_dev *em7180 = (struct em7180_dev *)devid;

	dbg("****************************************************************");
	/* disable irq,To prevent the interruption of abnormal */
	disable_irq_nosync(em7180->irq);

	complete(&em7180->done);

	return IRQ_HANDLED;
}

struct file_operations em7180_fops = {
	.owner = THIS_MODULE,
	.open = em7180_open,
	.read = em7180_read,
	.release = em7180_release,
	.unlocked_ioctl = em7180_ioctl,
};

static int
em7180_probe(struct i2c_client *i2c_dev, const struct i2c_device_id *id)
{
	int ret = 0;
	struct em7180_dev *em7180 = NULL;
	struct em7180_platform_data *pdata =
	    (struct em7180_platform_data *)i2c_dev->dev.platform_data;

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
	em7180->wakeup = pdata->wakeup;;

	/* In order to clear the first interrupt */
	em7180->irq_flag = 1;

	ret = gpio_request(em7180->irq_pin, "em7180_irq_pin");
	if (ret) {
		dev_err(&i2c_dev->dev, "GPIO request failed");
		goto err_regulator;
	}

	ret = gpio_direction_input(em7180->irq_pin);
	if (ret < 0) {
		dev_err(&i2c_dev->dev,
			"unable to set GPIO direction,err=%d", ret);
		goto err_gpio;
	}

	ret = em7180->irq = gpio_to_irq(em7180->irq_pin);
	if (ret < 0) {
		dev_err(&i2c_dev->dev, "cannot find IRQ");
		goto err_gpio;
	}

	em7180->i2c_dev = i2c_dev;
	pni_sentral_download_firmware_to_ram(em7180->i2c_dev);

	mutex_init(&em7180->lock);
	init_completion(&em7180->done);

	ret = request_irq(em7180->irq, em7180_irq_handler,
			  IRQF_TRIGGER_HIGH, i2c_dev->name, em7180);
	if (ret != 0) {
		dev_err(&i2c_dev->dev, "cannot claim IRQ %d", em7180->irq);
		goto err_irq;
	}
	disable_irq_nosync(em7180->irq);

	em7180->data_buf = (u8 *)__get_free_pages(GFP_KERNEL,
						  get_order(DATA_BUF_SIZE));
	if (NULL == em7180->data_buf) {
		dev_err(&i2c_dev->dev, "malloc em7180 data buf error");
		ret = -ENOMEM;
		goto err_buf;
	}

	em7180->buf_base = em7180->data_buf;
	em7180->buf_top = em7180->data_buf + DATA_BUF_SIZE;
	em7180->producer = em7180->data_buf;
	em7180->consumer = em7180->data_buf;
	em7180->buf_len = 0;
	dbg("buf_base = %p buf_top = %p producer = %p consumer = %p ",
	    em7180->buf_base, em7180->buf_top,
	    em7180->producer, em7180->consumer);
	dbg();

	em7180->kthread = kthread_create(em7180_daemon, em7180,
					 "em7180_daemon");
	dbg();

	if (IS_ERR(em7180->kthread)) {
		ret = -ENOMEM;
		goto err_buf;
	}

	em7180->misc_device.minor = MISC_DYNAMIC_MINOR;
	em7180->misc_device.name = EM7180_NAME;
	em7180->misc_device.fops = &em7180_fops;

	misc_register(&em7180->misc_device);
	if (ret < 0) {
		dev_err(&i2c_dev->dev, "misc_register failed");
		goto err_register_misc;
	}

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

static int em7180_suspend(struct i2c_client *i2c_dev, pm_message_t state)
{
	struct em7180_dev *em7180 = i2c_get_clientdata(i2c_dev);

	if (em7180->wakeup)
		enable_irq_wake(em7180->irq);

	return 0;
}

static int em7180_resume(struct i2c_client *client)
{
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
	.probe		= em7180_probe,
	.remove	= __devexit_p(em7180_remove),
	.id_table	= em7180_i2c_id,
	.suspend	= em7180_suspend,
	.resume	= em7180_resume,
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
