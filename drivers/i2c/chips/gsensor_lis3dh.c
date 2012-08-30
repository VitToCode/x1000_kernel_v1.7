#include        <linux/err.h>
#include        <linux/errno.h>
#include        <linux/delay.h>
#include        <linux/fs.h>
#include        <linux/i2c.h>
#include        <linux/input.h>  
#include        <linux/uaccess.h>
#include        <linux/workqueue.h>
#include        <linux/irq.h>
#include        <linux/gpio.h>
#include        <linux/interrupt.h>
#include        <linux/slab.h>
#include        <linux/miscdevice.h>
#include        <linux/linux_sensors.h>
#include        <linux/gsensor.h>
#include        <linux/hwmon-sysfs.h>
#include        "gsensor_lis3dh.h"

#define DEBUG   1


struct {
	unsigned int cutoff_ms;
	unsigned int mask;
} lis3dh_acc_odr_table[] = {
	{ 1,	ODR1250 },
	{ 3,	ODR400  },
	{ 10,	ODR200  },
	{ 20,	ODR100  },
	{ 100,	ODR50   },
	{ 300,	ODR50   },
	{ 500,	ODR10   },
	{ 1000, ODR1    },
};

struct lis3dh_acc_data {
	struct i2c_client *client;
	struct gsensor_platform_data *pdata;

	struct mutex	lock_rw;
	struct mutex	lock;
	struct delayed_work input_work;

	struct input_dev *input_dev;

	int hw_initialized;
	/* hw_working=-1 means not tested yet */
	int hw_working;
	atomic_t enabled;
	int is_suspend;
	u8 sensitivity;
	u8 resume_state[RESUME_ENTRIES];
	int irq;
	struct work_struct irq_work;
	struct workqueue_struct *irq_work_queue;
	
	struct miscdevice lis3dh_misc_device;

	struct regulator *power;
};

static int lis3dh_i2c_read(struct lis3dh_acc_data *acc,
		u8 * buf, int len)                                                                                         
{
	int err;
	int tries = 0;

	struct i2c_msg  msgs[] = {
		{
			.addr = acc->client->addr,
			.flags = 0 ,//acc->client->flags & I2C_M_TEN,
			.len = 1,
			.buf = buf,
		},
		{
			.addr = acc->client->addr,
			.flags = 1,//(acc->client->flags & I2C_M_TEN) | I2C_M_RD,
			.len = len,
			.buf = buf,
		},
	};

	do {
		err = i2c_transfer(acc->client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 2) && (++tries < I2C_RETRIES));

	if (err != 2) {
		dev_err(&acc->client->dev, "read transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lis3dh_i2c_write(struct lis3dh_acc_data *acc, u8 * buf, int len)
{
	int err;
	int tries = 0;

	struct i2c_msg msgs[] = {
		{
			.addr = acc->client->addr,
			.flags = acc->client->flags & I2C_M_TEN,
			.len = len + 1,
			.buf = buf,
		},
	};

	do {
		err = i2c_transfer(acc->client->adapter, msgs, 1);
		if (err != 1)
			msleep_interruptible(I2C_RETRY_DELAY);
	} while ((err != 1) && (++tries < I2C_RETRIES));

	if (err != 1) {
		dev_err(&acc->client->dev, "write transfer error\n");
		err = -EIO;
	} else {
		err = 0;
	}

	return err;
}

static int lis3dh_acc_i2c_read(struct lis3dh_acc_data *acc,u8 * buf, int len)
{
	int ret;
	mutex_lock(&acc->lock_rw);
	ret = lis3dh_i2c_read(acc,buf,len);
	mutex_unlock(&acc->lock_rw);
	return ret;
}
static int lis3dh_acc_i2c_write(struct lis3dh_acc_data *acc, u8 * buf, int len)
{
	int ret;
	mutex_lock(&acc->lock_rw);
	ret = lis3dh_i2c_write(acc,buf,len);
	mutex_unlock(&acc->lock_rw);
	return ret;
}
int lis3dh_acc_update_odr(struct lis3dh_acc_data *acc, int poll_interval_ms)
{                                                                                                                                          
	int err = -1;
	int i;
	u8 config[2];
	for(i = 0;i < ARRAY_SIZE(lis3dh_acc_odr_table);i++){
		config[1] = lis3dh_acc_odr_table[i].mask;
		if(poll_interval_ms < lis3dh_acc_odr_table[i].cutoff_ms){
			//  printk("conifg=%x  poll_interval_ms=%d cut=%d \n",
			//	config[1],poll_interval_ms,lis3dh_acc_odr_table[i].cutoff_ms);
			break;  
		}
	}
	config[1] |= LIS3DH_ACC_ENABLE_ALL_AXES;

	if (atomic_read(&acc->enabled)) {
		config[0] = CTRL_REG1;
		err = lis3dh_acc_i2c_write(acc, config, 1);
		if (err < 0)
			goto error;
		acc->resume_state[RES_CTRL_REG1] = config[1];
	}

	return err;   
error:
	dev_err(&acc->client->dev, "update odr failed 0x%x,0x%x: %d\n",
			config[0], config[1], err);

	return err;
}


static int lis3dh_acc_hw_init(struct lis3dh_acc_data *acc)
{
	int err = -1;
	u8 buf[7];

	printk(KERN_INFO "%s: hw init start\n", LIS3DH_ACC_DEV_NAME);
	buf[0] = WHO_AM_I;
	err = lis3dh_acc_i2c_read(acc, buf, 1);
	if (err < 0) {
		dev_warn(&acc->client->dev, "Error reading WHO_AM_I: is device "
				"available/working?\n");
		goto err_firstread;
	} else
		acc->hw_working = 1;
	if (buf[0] != WHOAMI_LIS3DH_ACC) {
		dev_err(&acc->client->dev,
				"device unknown. Expected: 0x%x,"
				" Replies: 0x%x\n", WHOAMI_LIS3DH_ACC, buf[0]);
		err = -1; /* choose the right coded error */
		goto err_unknown_device;
	}
	buf[0] = CTRL_REG1;
	buf[1] = acc->resume_state[RES_CTRL_REG1];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = TEMP_CFG_REG;
	buf[1] = acc->resume_state[RES_TEMP_CFG_REG];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = FIFO_CTRL_REG;
	buf[1] = acc->resume_state[RES_FIFO_CTRL_REG];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)                                                             
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | TT_THS);
	buf[1] = acc->resume_state[RES_TT_THS];
	buf[2] = acc->resume_state[RES_TT_LIM];
	buf[3] = acc->resume_state[RES_TT_TLAT];
	buf[4] = acc->resume_state[RES_TT_TW];
	err = lis3dh_acc_i2c_write(acc, buf, 4);
	if (err < 0)
		goto err_resume_state;
	buf[0] = TT_CFG;
	buf[1] = acc->resume_state[RES_TT_CFG];
	err = lis3dh_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | INT_THS1);
	buf[1] = acc->resume_state[RES_INT_THS1];
	buf[2] = acc->resume_state[RES_INT_DUR1];
	err = lis3dh_acc_i2c_write(acc, buf, 2);
	if (err < 0)
		goto err_resume_state;
	buf[0] = INT_CFG1;
	buf[1] = acc->resume_state[RES_INT_CFG1];
	err = lis3dh_acc_i2c_write(acc, buf, 1);

	if (err < 0)
		goto err_resume_state;
	buf[0] = (I2C_AUTO_INCREMENT | CTRL_REG2);
	buf[1] = acc->resume_state[RES_CTRL_REG2];
	buf[2] = acc->resume_state[RES_CTRL_REG3];
	buf[3] = acc->resume_state[RES_CTRL_REG4];
	buf[4] = acc->resume_state[RES_CTRL_REG5];
	buf[5] = acc->resume_state[RES_CTRL_REG6];
	err = lis3dh_acc_i2c_write(acc, buf, 5);
	if (err < 0)
		goto err_resume_state;
	mdelay(100);
	acc->hw_initialized = 1;
	printk(KERN_INFO "%s: hw init done\n", LIS3DH_ACC_DEV_NAME);
	return 0;
err_firstread:
	acc->hw_working = 0;
err_unknown_device:
err_resume_state:
	acc->hw_initialized = 0;
	dev_err(&acc->client->dev, "hw init error 0x%x,0x%x: %d\n", buf[0],
			buf[1], err);
	return err;
}

static int lis3dh_acc_device_power_off(struct lis3dh_acc_data *acc)
{
	int err;

	if (acc->pdata->power_off) {
		acc->pdata->power_off();
		acc->hw_initialized = 0;
	}else{
		u8 buf[2] = { CTRL_REG1, LIS3DH_ACC_PM_OFF };
		err = lis3dh_acc_i2c_write(acc, buf, 1);
		if (err < 0){
			dev_err(&acc->client->dev,
					"soft power off failed: %d\n", err);
			return err;
		}
		if (acc->power){
			err = regulator_disable(acc->power);
			if (err < 0){
				dev_err(&acc->client->dev,
						"power_off regulator failed: %d\n", err);
				return err;
			}
		}
	}
	return 0;                 
}

static int lis3dh_acc_device_power_on(struct lis3dh_acc_data *acc)
{
	int err = -1;
	u8 buf[2];
	if (acc->pdata->power_on) {
		err = acc->pdata->power_on();
		if (err < 0) {
			dev_err(&acc->client->dev,
					"power_on failed: %d\n", err);
			return err;
		}
	}else{
		if (acc->power){
			err = regulator_enable(acc->power);
			if (err < 0){
				dev_err(&acc->client->dev,
						"power_on regulator failed: %d\n", err);
				return err;
			}
		}
		udelay(10);
		
		buf[0] = CTRL_REG1;
		buf[1] = LIS3DH_ACC_ENABLE_ALL_AXES;//acc->resume_state[RES_CTRL_REG1];
		err = lis3dh_acc_i2c_write(acc, buf, 1);
		if (err < 0){
			dev_err(&acc->client->dev,
					"power_on failed: %d\n", err);
			return err;
		}
		buf[0] = CTRL_REG4;
		buf[1] = 0x08;
		err = lis3dh_acc_i2c_write(acc, buf, 1);
		if (err < 0){
			dev_err(&acc->client->dev,
					"power_on failed: %d\n", err);
			return err;
		}

	}
	return 0;                 
}

static int lis3dh_acc_get_acceleration_data(struct lis3dh_acc_data *acc,
		int *xyz)
{
	int err = -1;
	u8 buf[1]={0};
	/* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	u8 acc_data[6];
	/* x,y,z hardware data */
	s16 hw_d[3] = { 0 };
	acc_data[0] = (I2C_AUTO_INCREMENT | AXISDATA_REG);
	err = lis3dh_acc_i2c_read(acc, acc_data, 6);
	if (err < 0)
		return err;

	hw_d[0] = (((s16) ((acc_data[1] << 8) | acc_data[0])) >> 4);
	hw_d[1] = (((s16) ((acc_data[3] << 8) | acc_data[2])) >> 4);
	hw_d[2] = (((s16) ((acc_data[5] << 8) | acc_data[4])) >> 4);

	hw_d[0] = hw_d[0] * acc->sensitivity;
	hw_d[1] = hw_d[1] * acc->sensitivity;
	hw_d[2] = hw_d[2] * acc->sensitivity;


	xyz[0] = ((acc->pdata->negate_x) ? (-hw_d[acc->pdata->axis_map_x])
			: (hw_d[acc->pdata->axis_map_x]));
	xyz[1] = ((acc->pdata->negate_y) ? (-hw_d[acc->pdata->axis_map_y])
			: (hw_d[acc->pdata->axis_map_y]));
	xyz[2] = ((acc->pdata->negate_z) ? (-hw_d[acc->pdata->axis_map_z])
			: (hw_d[acc->pdata->axis_map_z]));

	/*this is must be done in this mode*/
	buf[0] = INT_SRC1;
	err = lis3dh_acc_i2c_read(acc, buf, 1);
	//printk("--int source=%x\n",buf[0]);

	return err;
}

#ifdef CONFIG_SENSORS_ORI
extern void orientation_report_values(int x,int y,int z); 
#endif
static void lis3dh_acc_report_values(struct lis3dh_acc_data *acc,
		int *xyz)
{
	//printk(KERN_INFO "%s read x=%d, y=%d, z=%d\n",LIS3DH_ACC_DEV_NAME, xyz[0], xyz[1], xyz[2]);

	input_report_abs(acc->input_dev, ABS_X, xyz[0]);
	input_report_abs(acc->input_dev, ABS_Y, xyz[1]);
	input_report_abs(acc->input_dev, ABS_Z, xyz[2]);
	input_sync(acc->input_dev);
#ifdef CONFIG_SENSORS_ORI
	orientation_report_values(xyz[0],xyz[1],xyz[2]); 
#endif
}


static int lis3dh_acc_enable(struct lis3dh_acc_data *acc)
{
	int err;
	u8 buf[2]={0};
	if ((acc->is_suspend == 0) && !atomic_cmpxchg(&acc->enabled, 0, 1)) {
		err = lis3dh_acc_device_power_on(acc);
		if (err < 0) {
			atomic_set(&acc->enabled, 0);
			return err;
		}
		enable_irq(acc->client->irq);

		buf[0] = INT_SRC1;
		err = lis3dh_acc_i2c_read(acc, buf, 1);

	}
	return 0;
}

static int lis3dh_acc_disable(struct lis3dh_acc_data *acc)
{
	if (atomic_cmpxchg(&acc->enabled, 1, 0)) {
		disable_irq_nosync(acc->client->irq);
		flush_workqueue(acc->irq_work_queue);
		lis3dh_acc_device_power_off(acc);
	}

	return 0;                                                   
}

struct linux_sensor_t hardware_data_lis3dh = {                                                                                             
	"lis3dh 3-axis Accelerometer",
	"ST sensor",
	SENSOR_TYPE_ACCELEROMETER,0,1024,1, 1, { }
};

static int lis3dh_acc_validate_pdata(struct lis3dh_acc_data *acc)
{
	acc->pdata->poll_interval = max(acc->pdata->poll_interval, 
			acc->pdata->min_interval);     

	if (acc->pdata->axis_map_x > 2 || 
			acc->pdata->axis_map_y > 2 ||  
			acc->pdata->axis_map_z > 2) {  
		dev_err(&acc->client->dev, "invalid axis_map value "
				"x:%u y:%u z%u\n", acc->pdata->axis_map_x,
				acc->pdata->axis_map_y, acc->pdata->axis_map_z);
		return -EINVAL;
	}
	/* Only allow 0 and 1 for negation boolean flag */
	if (acc->pdata->negate_x > 1 || acc->pdata->negate_y > 1
			|| acc->pdata->negate_z > 1) {
		dev_err(&acc->client->dev, "invalid negate value "
				"x:%u y:%u z:%u\n", acc->pdata->negate_x,
				acc->pdata->negate_y, acc->pdata->negate_z);
		return -EINVAL;
	}
	/* Enforce minimum polling interval */
	if (acc->pdata->poll_interval < acc->pdata->min_interval) {
		dev_err(&acc->client->dev, "minimum poll interval violated\n");
		return -EINVAL;                                         
	}

	return 0;
}


int lis3dh_acc_update_g_range(struct lis3dh_acc_data *acc, u8 new_g_range)
{
	int err=-1;

	u8 sensitivity;
	u8 buf[2];
	u8 updated_val;
	u8 init_val;
	u8 new_val;
	u8 mask = LIS3DH_ACC_FS_MASK | HIGH_RESOLUTION;

	switch (new_g_range) {
		case GSENSOR_2G:
			new_g_range = LIS3DH_ACC_G_2G;
			sensitivity = SENSITIVITY_2G;
			break;
		case GSENSOR_4G:
			new_g_range = LIS3DH_ACC_G_4G;
			sensitivity = SENSITIVITY_4G;
			break;
		case GSENSOR_8G:
			new_g_range = LIS3DH_ACC_G_8G;
			sensitivity = SENSITIVITY_8G;
			break;
		case GSENSOR_16G:
			new_g_range = LIS3DH_ACC_G_16G;
			sensitivity = SENSITIVITY_16G;
			break;
		default:
			dev_err(&acc->client->dev, "invalid g range requested: %u\n",
					new_g_range);
			return -EINVAL;
	}

	if (atomic_read(&acc->enabled)) {
		/* Updates configuration register 4,
		 *                 * which contains g range setting */
		buf[0] = CTRL_REG4;
		err = lis3dh_acc_i2c_read(acc, buf, 1);
		if (err < 0)
			goto error;
		init_val = buf[0];
		acc->resume_state[RES_CTRL_REG4] = init_val;
		new_val = new_g_range | HIGH_RESOLUTION;
		updated_val = ((mask & new_val) | ((~mask) & init_val));
		buf[0] = CTRL_REG4;
		buf[1] = updated_val;
		err = lis3dh_acc_i2c_write(acc, buf, 1);
		if (err < 0)
			goto error;
		acc->resume_state[RES_CTRL_REG4] = updated_val;
		acc->sensitivity = sensitivity;
	}


	return err;

error:
	dev_err(&acc->client->dev, "update g range failed 0x%x,0x%x: %d\n",
			buf[0], buf[1], err);

	return err;
}


static int lis3dh_acc_input_init(struct lis3dh_acc_data *acc)
{
	int err;
	acc->input_dev = input_allocate_device();
	if (!acc->input_dev) {
		err = -ENOMEM;
		dev_err(&acc->client->dev, "input device allocation failed\n");
		goto err0;
	}

	acc->input_dev->name = "g_sensor";
	acc->input_dev->id.bustype = BUS_I2C;
	acc->input_dev->dev.parent = &acc->client->dev;

	input_set_drvdata(acc->input_dev, acc);

	set_bit(EV_ABS, acc->input_dev->evbit);
	input_set_abs_params(acc->input_dev, ABS_X, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(acc->input_dev, ABS_Y, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(acc->input_dev, ABS_Z, -G_MAX, G_MAX, FUZZ, FLAT);
	err = input_register_device(acc->input_dev);
	if (err) {
		dev_err(&acc->client->dev,
				"unable to register input device %s\n",
				acc->input_dev->name);
		goto err1;
	}

	return 0;

err1:
	input_free_device(acc->input_dev);
err0:
	return err;
}

static void lis3dh_acc_input_cleanup(struct lis3dh_acc_data *acc)                       
{
	input_unregister_device(acc->input_dev);
	input_free_device(acc->input_dev);
}


static int lis3dh_misc_open(struct inode *inode, struct file *file)
{
	int err;              
	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;   

	return 0;
}

long lis3dh_misc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int interval;
	
	struct miscdevice *dev = file->private_data;
	struct lis3dh_acc_data *lis3dh = container_of(dev, struct lis3dh_acc_data,lis3dh_misc_device);

	switch (cmd) {
		case SENSOR_IOCTL_GET_DELAY:
			interval = lis3dh->pdata->poll_interval;
			if (copy_to_user(argp, &interval, sizeof(interval)))
				return -EFAULT;
			break;
		case SENSOR_IOCTL_SET_DELAY:
			mutex_lock(&lis3dh->lock);
			if (copy_from_user(&interval, argp, sizeof(interval)))
				return -EFAULT;
			interval *= 10;  //for Sensor_new
			if (interval < lis3dh->pdata->min_interval )
				interval = lis3dh->pdata->min_interval;
			else if (interval > lis3dh->pdata->max_interval)
				interval = lis3dh->pdata->max_interval;
			lis3dh->pdata->poll_interval = interval;
			lis3dh_acc_update_odr(lis3dh, lis3dh->pdata->poll_interval);
			mutex_unlock(&lis3dh->lock);
			break;
		case SENSOR_IOCTL_SET_ACTIVE:
			mutex_lock(&lis3dh->lock);
			if (copy_from_user(&interval, argp, sizeof(interval)))
				return -EFAULT;
			if (interval > 1)
				return -EINVAL;
			if (interval){
				lis3dh_acc_enable(lis3dh);
				lis3dh_acc_update_odr(lis3dh, lis3dh->pdata->poll_interval);
			}else{
				lis3dh_acc_disable(lis3dh);
			}
			mutex_unlock(&lis3dh->lock);
			break;
		case SENSOR_IOCTL_GET_ACTIVE:
			interval = atomic_read(&lis3dh->enabled);
			if (copy_to_user(argp, &interval, sizeof(interval)))
				return -EINVAL;                                         
			break;
		case SENSOR_IOCTL_GET_DATA:
			if (copy_to_user(argp, &hardware_data_lis3dh, sizeof(hardware_data_lis3dh)))
				return -EINVAL;
			break;

		case SENSOR_IOCTL_GET_DATA_MAXRANGE:
			if (copy_to_user(argp, &lis3dh->pdata->g_range, sizeof(lis3dh->pdata->g_range)))
				return -EFAULT;
			break;

		case SENSOR_IOCTL_WAKE:
			input_event(lis3dh->input_dev, EV_SYN,SYN_CONFIG, 0);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}


static const struct file_operations lis3dh_misc_fops = {
	.owner = THIS_MODULE,
	.open = lis3dh_misc_open,
	.unlocked_ioctl = lis3dh_misc_ioctl,
};


static void lis3dh_acc_work(struct work_struct *work)
{
	struct lis3dh_acc_data *acc;   
	int xyz[3] = { 0 };
	int err;
	u8 buf[2]={0};
	acc =  container_of(work, struct lis3dh_acc_data, irq_work);

	buf[0] = STATUS_REG;
	err = lis3dh_acc_i2c_read(acc, buf, 1);
	if( (buf[0]& 0x08) ==0){
		enable_irq(acc->client->irq);
		return;
	}

	err = lis3dh_acc_get_acceleration_data(acc, xyz);
	if(err < 0 )
		dev_err(&acc->client->dev, "Acceleration data read failed\n");        
	else
		lis3dh_acc_report_values(acc, xyz);
	enable_irq(acc->client->irq);
}

static irqreturn_t lis3dh_acc_interrupt(int irq, void *dev_id)
{

	struct lis3dh_acc_data *acc = dev_id;
	disable_irq_nosync(acc->client->irq);
	if(acc->is_suspend == 1 ||atomic_read(&acc->enabled) == 0){
		return IRQ_HANDLED;
	}
	//     __gpio_ack_irq(acc->pdata->gpio_int);
	if(!work_pending(&acc->irq_work))
		queue_work(acc->irq_work_queue, &acc->irq_work);
	return IRQ_HANDLED;

}


static int lis3dh_acc_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{

	struct lis3dh_acc_data *acc;

	int err = -1;
	u8 buf[7]={0};

	printk("%s: probe start.\n", LIS3DH_ACC_DEV_NAME);

	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	acc = kzalloc(sizeof(struct lis3dh_acc_data), GFP_KERNEL);
	if (acc == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for module data: "
				"%d\n", err);
		goto exit_check_functionality_failed;
	}

	mutex_init(&acc->lock_rw);
	mutex_init(&acc->lock);

	acc->client = client;
	i2c_set_clientdata(client, acc);

	acc->pdata = kmalloc(sizeof(*acc->pdata), GFP_KERNEL);
	if (acc->pdata == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for pdata: %d\n",
				err);
		goto err_free;
	}

	memcpy(acc->pdata, client->dev.platform_data, sizeof(*acc->pdata));

	if (gpio_request_one(acc->pdata->gpio_int,
				GPIOF_DIR_IN, "gsensor lis3dh irq")) {
		dev_err(&client->dev, "no irq pin available\n");
		acc->pdata->gpio_int = -EBUSY;
	}

	err = lis3dh_acc_validate_pdata(acc);
	if (err < 0) {
		dev_err(&client->dev, "failed to validate platform data\n");
		goto exit_kfree_pdata;
	}
	
	client->dev.init_name=client->name;
	acc->power = regulator_get(&client->dev, "vgsensor");
	if (IS_ERR(acc->power)) {
		dev_warn(&client->dev, "get regulator failed\n");
	}
	if (acc->power){
		err = regulator_enable(acc->power);
		if (err < 0){
			dev_err(&acc->client->dev,
					"power_on regulator failed: %d\n", err);
			return err;
		}
	}
	udelay(100);

	/*--read id must add to load mma8452 or lis3dh--*/ 
	buf[0] = WHO_AM_I;
	err = lis3dh_acc_i2c_read(acc, buf, 1);
	if (err < 0 || buf[0] != WHOAMI_LIS3DH_ACC) {
		printk("ERROR: Can't load lis3dh g_sensor driver,may use mma8452 g_sensor driver\n");
		return -EINVAL;
	}
	printk("Gsensor is lis3dh\n");

	if (acc->pdata->init) {
		err = acc->pdata->init();
		if (err < 0) {
			dev_err(&client->dev, "init failed: %d\n", err);
			goto err_pdata_init;
		}
	}
	memset(acc->resume_state, 0, ARRAY_SIZE(acc->resume_state));


	acc->resume_state[RES_CTRL_REG1] = LIS3DH_ACC_ENABLE_ALL_AXES;//0X27
	acc->resume_state[RES_CTRL_REG2] = 0x00;
	acc->resume_state[RES_CTRL_REG3] = 0xC0;

	acc->resume_state[RES_CTRL_REG4] = 0x00;
	acc->resume_state[RES_CTRL_REG5] = 0x08;
	acc->resume_state[RES_CTRL_REG6] = 0x02;

	acc->resume_state[RES_TEMP_CFG_REG] = 0x00;
	acc->resume_state[RES_FIFO_CTRL_REG] = 0X00;

	acc->resume_state[RES_INT_CFG1] = 0x3F;
	acc->resume_state[RES_INT_THS1] = 0x00;
	acc->resume_state[RES_INT_DUR1] = 0x00;

	acc->resume_state[RES_TT_CFG] = 0x3F;
	acc->resume_state[RES_TT_THS] = 0x00;
	acc->resume_state[RES_TT_LIM] = 0x00;
	acc->resume_state[RES_TT_TLAT] = 0x00;
	acc->resume_state[RES_TT_TW] = 0x00;

	lis3dh_acc_device_power_off(acc);
	udelay(100);
	err = lis3dh_acc_device_power_on(acc);
	udelay(100);

	if (err < 0) {
		dev_err(&client->dev, "power on failed: %d\n", err);
		goto err_pdata_init;
	}
	atomic_set(&acc->enabled, 1);

	err = lis3dh_acc_hw_init(acc);
	if (err < 0) {
		lis3dh_acc_device_power_off(acc);
		return err;
	}

	err = lis3dh_acc_update_g_range(acc, acc->pdata->g_range);
	if (err < 0) {
		dev_err(&client->dev, "update_g_range failed\n");
		goto  err_power_off;
	}

	err = lis3dh_acc_update_odr(acc, acc->pdata->poll_interval);
	if (err < 0) {
		dev_err(&client->dev, "update_odr failed\n");
		goto  err_power_off;
	}
	err = lis3dh_acc_input_init(acc);
	if (err < 0) {
		dev_err(&client->dev, "input init failed\n");
		goto err_power_off;
	}

	acc->lis3dh_misc_device.minor = MISC_DYNAMIC_MINOR,
	acc->lis3dh_misc_device.name =  "g_sensor",
	acc->lis3dh_misc_device.fops = &lis3dh_misc_fops,
	misc_register(&acc->lis3dh_misc_device);

	lis3dh_acc_device_power_off(acc);
	/* As default, do not report information */
	atomic_set(&acc->enabled, 0);  
	acc->is_suspend = 0;
	INIT_WORK(&acc->irq_work, lis3dh_acc_work);

	acc->irq_work_queue = create_singlethread_workqueue("lis3dh_acc");

	if(!(acc->irq_work_queue)){   
		err = -ESRCH; 
		printk("creating workqueue failed\n");
	}                     
	client->irq = gpio_to_irq(acc->pdata->gpio_int);
	err = request_irq(client->irq, lis3dh_acc_interrupt,
			IRQF_TRIGGER_FALLING | IRQF_DISABLED,
			"lis3dh_acc", acc);
	if (err < 0)
		printk("err == %d \n",err); 

	disable_irq_nosync(acc->client->irq);

	printk(KERN_INFO "%s: %s has set irq to irq: %d "
			"mapped on gpio:%d\n",
			LIS3DH_ACC_DEV_NAME, __func__, acc->client->irq,
			acc->pdata->gpio_int);

	dev_info(&client->dev, "%s: probed\n", LIS3DH_ACC_DEV_NAME);
	return 0;

err_power_off:
	lis3dh_acc_device_power_off(acc);
err_pdata_init:
	if (acc->pdata->exit)
		acc->pdata->exit();
exit_kfree_pdata:
	kfree(acc->pdata);
err_free:
	kfree(acc);
exit_check_functionality_failed:
	printk(KERN_ERR "%s: Driver Init failed\n", LIS3DH_ACC_DEV_NAME);
	return err;
}

static int __devexit lis3dh_acc_remove(struct i2c_client *client)
{
	struct lis3dh_acc_data *acc = i2c_get_clientdata(client);

	if(acc->pdata->gpio_int >= 0){
		free_irq(acc->irq, acc);
		gpio_free(acc->pdata->gpio_int);
		destroy_workqueue(acc->irq_work_queue);
	}

	lis3dh_acc_input_cleanup(acc);
	lis3dh_acc_device_power_off(acc);
	//remove_sysfs_interfaces(&client->dev);

	if (acc->pdata->exit)
		acc->pdata->exit();
	kfree(acc->pdata);
	kfree(acc);

	return 0;
}

#ifdef CONFIG_PM
static int lis3dh_acc_resume(struct i2c_client *client)
{
	struct lis3dh_acc_data *acc = i2c_get_clientdata(client);
	mutex_lock(&acc->lock);
	acc->is_suspend = 0;
	lis3dh_acc_enable(acc);
	mutex_unlock(&acc->lock);
	enable_irq(client->irq);
	return 0;
}

static int lis3dh_acc_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct lis3dh_acc_data *acc = i2c_get_clientdata(client);
	acc->is_suspend = 1;
	disable_irq_nosync(client->irq);
	return lis3dh_acc_disable(acc);
}

#else
#define lis3dh_acc_suspend      NULL
#define lis3dh_acc_resume       NULL
#endif

static const struct i2c_device_id lis3dh_acc_id[]
= { { LIS3DH_ACC_DEV_NAME, 0 }, { }, };

MODULE_DEVICE_TABLE(i2c, lis3dh_acc_id);

static struct i2c_driver lis3dh_acc_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = LIS3DH_ACC_DEV_NAME,
	},
	.probe = lis3dh_acc_probe,
	.remove = __devexit_p(lis3dh_acc_remove),
	.suspend = lis3dh_acc_suspend,
	.resume = lis3dh_acc_resume,
	.id_table = lis3dh_acc_id,
};

static int __init lis3dh_acc_init(void)
{
	printk(KERN_INFO "%s accelerometer driver: init\n",
			LIS3DH_ACC_DEV_NAME);
	return i2c_add_driver(&lis3dh_acc_driver);
}

static void __exit lis3dh_acc_exit(void)
{
#ifdef DEBUG
	printk(KERN_INFO "%s accelerometer driver exit\n",
			LIS3DH_ACC_DEV_NAME);
#endif /* DEBUG */
	i2c_del_driver(&lis3dh_acc_driver);
	return;
}

module_init(lis3dh_acc_init);
module_exit(lis3dh_acc_exit);

MODULE_DESCRIPTION("lis3dh digital accelerometer sysfs driver");
MODULE_AUTHOR("Matteo Dameno, Carmine Iascone, STMicroelectronics");
MODULE_LICENSE("GPL");

