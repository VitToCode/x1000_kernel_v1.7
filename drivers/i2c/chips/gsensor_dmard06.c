#include        <linux/err.h>
#include        <linux/errno.h>
#include        <linux/delay.h>
#include        <linux/fs.h>
#include        <linux/i2c.h>
#include        <linux/input.h>
#include        <linux/uaccess.h>
#include        <linux/workqueue.h>
#include	    <linux/earlysuspend.h>
#include        <linux/irq.h>
#include        <linux/gpio.h>
#include        <linux/interrupt.h>
#include        <linux/slab.h>
#include        <linux/miscdevice.h>
#include        <linux/linux_sensors.h>
#include        <linux/gsensor.h>
#include        <linux/hwmon-sysfs.h>
#include        "gsensor_dmard06.h"
//#define DEBUG
#ifdef DEBUG
	#define dprintk(x...)	do{printk("~~~~~%s~~~~~\n",__FUNCTION__);printk(x);}while(0)
#else
	#define dprintk(x...)
#endif
#define SENSOR_DATA_SIZE 3

#define SAMPLE_COUNT (1)
#if DMT_DEBUG_DATA
#define IN_FUNC_MSG printk(KERN_INFO "@DMT@ In %s\n", __func__)
#define PRINT_X_Y_Z(x, y, z) printk(KERN_INFO "@DMT@ X/Y/Z axis: %04d , %04d , %04d\n", (x), (y), (z))
#define PRINT_OFFSET(x, y, z) printk(KERN_INFO "@offset@  X/Y/Z axis: %04d , %04d , %04d\n",offset.x,offset.y,offset.z);
#else
#define IN_FUNC_MSG
#define PRINT_X_Y_Z(x, y, z)
#define PRINT_OFFSET(x, y, z)
#endif
static struct jz_sync_time_info *pdata;
struct dmard06_acc_data *dmard06_acc;
struct {
	unsigned int cutoff_ms;
	unsigned int mask;
} dmard06_acc_odr_table[] = {
	{ 1,	ODR1250 },
	{ 3,	ODR400  },
	{ 10,	ODR200  },
	{ 20,	ODR100  },
	{ 100,	ODR50   },
	{ 300,	ODR25   },
	{ 500,	ODR10   },
	{ 1000, ODR1    },
};

struct dmard06_acc_data {
	struct i2c_client *client;
	struct gsensor_platform_data *pdata;

	struct mutex	lock_rw;
	struct mutex	lock;
	struct delayed_work input_work;
	struct delayed_work dmard06_acc_delayed_work;

	struct input_dev *input_dev;

	int hw_initialized;
	/* hw_working=-1 means not tested yet */
	int hw_working;
	atomic_t enabled;
	atomic_t regulator_enabled;
	int power_tag;
	int is_suspend;
	u8 sensitivity;
	u8 resume_state[RESUME_ENTRIES];
	int irq;
	struct work_struct irq_work;
	struct workqueue_struct *irq_work_queue;
	struct workqueue_struct *work_queue;
	struct early_suspend early_suspend;

	struct miscdevice dmard06_misc_device;

	struct regulator *power;
};

static int dmard06_i2c_read(struct dmard06_acc_data *acc,
		u8 * buf, int len)
{
	int err;
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

	err = i2c_transfer(acc->client->adapter, msgs, 2);
	if (err < 0) {
		dev_err(&acc->client->dev, "gsensor dmard06 i2c read error\n");
	}
	return err;
}

static int dmard06_i2c_write(struct dmard06_acc_data *acc, u8 * buf, int len)
{
	int err;
	struct i2c_msg msgs[] = {
		{
			.addr = acc->client->addr,
			.flags = acc->client->flags & I2C_M_TEN,
			.len = len + 1,
			.buf = buf,
		},
	};

	err = i2c_transfer(acc->client->adapter, msgs, 1);
	if (err < 0)
		dev_err(&acc->client->dev, "gsensor dmard06 i2c write error\n");

	return err;
}

static int dmard06_acc_i2c_read(struct dmard06_acc_data *acc,u8 * buf, int len)
{
	int ret;
	mutex_lock(&acc->lock_rw);
	ret = dmard06_i2c_read(acc,buf,len);
	mutex_unlock(&acc->lock_rw);
	return ret;
}
static int dmard06_acc_i2c_write(struct dmard06_acc_data *acc, u8 * buf, int len)
{
	int ret;
	mutex_lock(&acc->lock_rw);
	ret = dmard06_i2c_write(acc,buf,len);
	mutex_unlock(&acc->lock_rw);
	return ret;
}
int dmard06_acc_update_odr(struct dmard06_acc_data *acc, int poll_interval_ms)
{
	int err = -1;
	int i;
	u8 config[2];
	u8 config1[2];

	for(i = 0;i < ARRAY_SIZE(dmard06_acc_odr_table);i++){
		config[1] = dmard06_acc_odr_table[i].mask;
		if(poll_interval_ms < dmard06_acc_odr_table[i].cutoff_ms){
			//  dprintk("conifg=%x  poll_interval_ms=%d cut=%d \n",
			//	config[1],poll_interval_ms,dmard06_acc_odr_table[i].cutoff_ms);
			break;
		}
	}

#if 1
	switch (config[1])
	{
		case ODR10:	config1[1] = 0x45;break;//INT_DUR1 register set to 0x2d irq rate is:11Hz
		case ODR25:	config1[1] = 0x20;break;//set to 0x0e irq rate:23Hz
		case ODR50:	config1[1] = 0x10;break;//set to 0x06 irq rate:42Hz
		default:	config1[1] = 0x45;break;//default situation set to 0x1D:irq rate:11Hz
	}
#endif

#ifdef CONFIG_SMP
	config[1] = 0x70;
#else
	config[1] = 0x50;
#endif
	config[1] |= DMARD06_ACC_ENABLE_ALL;
	if (atomic_read(&acc->enabled)) {
		config[0] = CTRL_REG1;
		err = dmard06_acc_i2c_write(acc, config, 1);
		if (err < 0)
			goto error;
		acc->resume_state[RES_CTRL_REG1] = config[1];
	}
	if (atomic_read(&acc->enabled)) {
		config1[0] = INT_DUR1;
		err = dmard06_acc_i2c_write(acc, config1, 1);
		if (err < 0)
			goto error;
		acc->resume_state[RES_INT_DUR1] = config1[1];
	}
	return err;
error:
	dev_err(&acc->client->dev, "update odr failed 0x%x,0x%x: %d\n",
			config[0], config[1], err);

	return err;
}


static int dmard06_acc_hw_init(struct dmard06_acc_data *acc)
{
	int err = -1;
	u8 buf[7];

//	dprintk(KERN_INFO "%s: hw init start\n", DMARD06_ACC_DEV_NAME);
	buf[0] = WHO_AM_I;
	err = dmard06_acc_i2c_read(acc, buf, 1);
	if (err < 0) {
		dev_warn(&acc->client->dev, "Error reading WHO_AM_I: is device "
				"available/working?\n");
		goto err_firstread;
	} else
		acc->hw_working = 1;
	if (buf[0] != WHOAMI_DMARD06_ACC) {
		dev_err(&acc->client->dev,
				"device unknown. Expected: 0x%x,"
				" Replies: 0x%x\n", WHOAMI_DMARD06_ACC, buf[0]);
		err = -1; /* choose the right coded error */
		goto err_unknown_device;
	}


	buf[0] = CTRL_REG1;
	buf[1] = acc->resume_state[RES_CTRL_REG1];
	err = dmard06_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = TEMP_CFG_REG;
	buf[1] = acc->resume_state[RES_TEMP_CFG_REG];
	err = dmard06_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = FIFO_CTRL_REG;
	buf[1] = acc->resume_state[RES_FIFO_CTRL_REG];
	err = dmard06_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;

	buf[0] = (I2C_AUTO_INCREMENT | TT_THS);
	buf[1] = acc->resume_state[RES_TT_THS];
	buf[2] = acc->resume_state[RES_TT_LIM];
	buf[3] = acc->resume_state[RES_TT_TLAT];
	buf[4] = acc->resume_state[RES_TT_TW];
	err = dmard06_acc_i2c_write(acc, buf, 4);
	if (err < 0)
		goto err_resume_state;

	buf[0] = TT_CFG;
	buf[1] = acc->resume_state[RES_TT_CFG];
	err = dmard06_acc_i2c_write(acc, buf, 1);
	if (err < 0)
		goto err_resume_state;


	buf[0] = (I2C_AUTO_INCREMENT | INT_THS1);
	buf[1] = acc->resume_state[RES_INT_THS1];
	buf[2] = acc->resume_state[RES_INT_DUR1];
	err = dmard06_acc_i2c_write(acc, buf, 2);

	if (err < 0)
		goto err_resume_state;
	buf[0] = INT_CFG1;
	buf[1] = acc->resume_state[RES_INT_CFG1];
	err = dmard06_acc_i2c_write(acc, buf, 1);

	if (err < 0)
		goto err_resume_state;
	buf[0] = (I2C_AUTO_INCREMENT | CTRL_REG2);
	buf[1] = acc->resume_state[RES_CTRL_REG2];
	buf[2] = acc->resume_state[RES_CTRL_REG3];
	buf[3] = acc->resume_state[RES_CTRL_REG4];
	buf[4] = acc->resume_state[RES_CTRL_REG5];
	buf[5] = acc->resume_state[RES_CTRL_REG6];
	err = dmard06_acc_i2c_write(acc, buf, 5);
	if (err < 0)
		goto err_resume_state;
	udelay(100);
	acc->hw_initialized = 1;
//	dprintk(KERN_INFO "%s: hw init done\n", DMARD06_ACC_DEV_NAME);

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

static int dmard06_acc_device_power_off(struct dmard06_acc_data *acc)
{
	if (atomic_cmpxchg(&acc->regulator_enabled, 1, 0)) {
	regulator_disable(acc->power);
    }
	/*int err;

	if (acc->pdata->power_off) {
		acc->pdata->power_off();
		acc->hw_initialized = 0;
	}else{
		u8 buf[2] = { CTRL_REG1, DMARD06_ACC_PM_OFF };
		err = dmard06_acc_i2c_write(acc, buf, 1);
		if (err < 0){
			dev_err(&acc->client->dev,
					"soft power off failed: %d\n", err);
			return err;
		}

	}
    */
	return 0;
}

 int dmard_acc_regulator_enbale(struct dmard06_acc_data *acc)
{
    atomic_set(&acc->regulator_enabled,1);
	return 	regulator_enable(acc->power);
}
static int dmard06_acc_device_power_on(struct dmard06_acc_data *acc)
{
  dmard_acc_regulator_enbale(acc);
	/*int err = -1;
	u8 buf[2];
	if (acc->pdata->power_on) {
		err = acc->pdata->power_on();
		if (err < 0) {
			dev_err(&acc->client->dev,
					"power_on failed: %d\n", err);
			return err;
		}
	}else{
		buf[0] = CTRL_REG1;
		buf[1] = 0x67;
		err = dmard06_acc_i2c_write(acc, buf, 1);
		if (err < 0){
			dev_err(&acc->client->dev,
					"power_on failed: %d\n", err);
			return err;
		}
		buf[0] = CTRL_REG4;
		buf[1] = 0x08;
		err = dmard06_acc_i2c_write(acc, buf, 1);
		if (err < 0){
			dev_err(&acc->client->dev,
					"power_on failed: %d\n", err);
			return err;
		}

	}
    */
	return 0;
}

static int dmard06_acc_enable(struct dmard06_acc_data *acc);
static int dmard06_acc_disable(struct dmard06_acc_data *acc);

static void dmard06_acc_regulator_enbale(struct dmard06_acc_data *acc)
{
	if (atomic_cmpxchg(&acc->regulator_enabled, 0, 1) == 0) {
  dmard_acc_regulator_enbale(acc);
		udelay(100);
		//dmard06_acc_hw_init(acc);
	}
}

static int dmard06_acc_get_acceleration_data(struct dmard06_acc_data *acc,
		int *xyz)
{
	int err = -1;//,i;
	u8 acc_data[6];
	s16 hw_d[3] = { 0 };
	acc_data[0] = (I2C_AUTO_INCREMENT | AXISDATA_REG);
	err = dmard06_acc_i2c_read(acc, acc_data, 6);
	if (err < 0){
		dprintk("------gsensor read data error\n");
		return err;
	}

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

	return err;
}

#ifdef CONFIG_SENSORS_ORI
extern void orientation_report_values(int x,int y,int z);
#endif
static void dmard06_acc_report_values(struct dmard06_acc_data *acc,
		int *xyz)
{
	//dprintk(KERN_INFO "%s read x=%d, y=%d, z=%d\n",DMARD06_ACC_DEV_NAME, xyz[0], xyz[1], xyz[2]);

	input_report_abs(acc->input_dev, ABS_X, -xyz[0]*2);
	input_report_abs(acc->input_dev, ABS_Y, -xyz[1]*2);
	input_report_abs(acc->input_dev, ABS_Z, -xyz[2]*2);
	input_sync(acc->input_dev);
#ifdef CONFIG_SENSORSpass the event_ORI
	if(acc->pdata->ori_pr_swap == 1){
		sensor_swap_pr((u16*)(xyz+0),(u16*)(xyz+1));
	}
	xyz[0] = ((acc->pdata->ori_roll_negate) ? (-xyz[0])
			: (xyz[0]));
	xyz[1] = ((acc->pdata->ori_pith_negate) ? (-xyz[1])
			: (xyz[1]));
	orientation_report_values(xyz[0],xyz[1],xyz[2]);
#endif
}


static int dmard06_acc_enable(struct dmard06_acc_data *acc)
{
	int err;
	u8 buf[2]={0};
	if ((acc->is_suspend == 0) && !atomic_cmpxchg(&acc->enabled, 0, 1)) {
		err = dmard06_acc_device_power_on(acc);
		if (err < 0) {
			atomic_set(&acc->enabled, 0);
			return err;
		}
//		enable_irq(acc->client->irq);

//		buf[0] = INT_SRC1;
	//	err = dmard06_acc_i2c_read(acc, buf, 1);
	}
	return 0;
}

static int dmard06_acc_disable(struct dmard06_acc_data *acc)
{
	if (atomic_cmpxchg(&acc->enabled, 1, 0)) {
//		flush_workqueue(acc->irq_work_queue);
		dmard06_acc_device_power_off(acc);
	}

	return 0;
}

struct linux_sensor_t hardware_data_dmard06 = {
	"dmard06 3-axis Accelerometer",
	"ST sensor",
	SENSOR_TYPE_ACCELEROMETER,0,64,1, 1, { }
};

static int dmard06_acc_validate_pdata(struct dmard06_acc_data *acc)
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


int dmard06_acc_update_g_range(struct dmard06_acc_data *acc, u8 new_g_range)
{
	int err=-1;

	u8 sensitivity;
	u8 buf[2];
	u8 updated_val;
	u8 init_val;
	u8 new_val;
	u8 mask = DMARD06_ACC_FS_MASK | HIGH_RESOLUTION;

	switch (new_g_range) {
		case GSENSOR_2G:
			new_g_range = DMARD06_ACC_G_2G;
			sensitivity = SENSITIVITY_2G;
			break;
		case GSENSOR_4G:
			new_g_range = DMARD06_ACC_G_4G;
			sensitivity = SENSITIVITY_4G;
			break;
		case GSENSOR_8G:
			new_g_range = DMARD06_ACC_G_8G;
			sensitivity = SENSITIVITY_8G;
			break;
		case GSENSOR_16G:
			new_g_range = DMARD06_ACC_G_16G;
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
		err = dmard06_acc_i2c_read(acc, buf, 1);
		if (err < 0)
			goto error;
		init_val = buf[0];
		acc->resume_state[RES_CTRL_REG4] = init_val;
		new_val = new_g_range | HIGH_RESOLUTION;
		updated_val = ((mask & new_val) | ((~mask) & init_val));
		buf[0] = CTRL_REG4;
		buf[1] = updated_val;
		err = dmard06_acc_i2c_write(acc, buf, 1);
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


int acc_input_close(struct input_dev *input)
{        struct  dmard06_acc_data *acc = input_get_drvdata(input);

		cancel_delayed_work_sync(&acc->input_work);
return 0;
}

 static int temp_enable(struct dmard06_acc_data * acc)
{

    static   int t;
    if (atomic_read(&acc->enabled)) 
    {
    printk("Dmard Sensor has already enable!\n");
    }
    else
    {
    atomic_set(&acc->enabled, 1);
    queue_delayed_work(acc->work_queue,&acc->dmard06_acc_delayed_work,msecs_to_jiffies(100));
    }
    return 0;
}
int acc_input_open(struct input_dev *input)
{
        struct dmard06_acc_data  *acc = input_get_drvdata(input);
        return temp_enable(acc);
}
        static int dmard06_acc_input_init(struct dmard06_acc_data *acc)
{
	int err;
	acc->input_dev = input_allocate_device();
	if (!acc->input_dev) {
		err = -ENOMEM;
		dev_err(&acc->client->dev, "input device allocation failed\n");
		goto err0;
	}
        acc->input_dev->open = acc_input_open;
        acc->input_dev->close = acc_input_close;



	acc->input_dev->name = "g_sensor";
	acc->input_dev->id.bustype = BUS_I2C;
	acc->input_dev->dev.parent = &acc->client->dev;

	input_set_drvdata(acc->input_dev, acc);

	set_bit(EV_ABS, acc->input_dev->evbit);
	input_set_abs_params(acc->input_dev, ABS_X, -G_MAX, G_MAX, 1, FLAT);
	input_set_abs_params(acc->input_dev, ABS_Y, -G_MAX, G_MAX, 1, FLAT);
	input_set_abs_params(acc->input_dev, ABS_Z, -G_MAX, G_MAX, 1, FLAT);
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

static void dmard06_acc_input_cleanup(struct dmard06_acc_data *acc)
{
	input_unregister_device(acc->input_dev);
	input_free_device(acc->input_dev);
}


static int dmard06_misc_open(struct inode *inode, struct file *file)
{
	int err;
	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;
	return 0;
}

long dmard06_misc_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int interval;

	struct miscdevice *dev = file->private_data;
	struct dmard06_acc_data *dmard06 = container_of(dev, struct dmard06_acc_data,dmard06_misc_device);

	switch (cmd) {
		case SENSOR_IOCTL_GET_DELAY:
			interval = dmard06->pdata->poll_interval;
			if (copy_to_user(argp, &interval, sizeof(interval)))
				return -EFAULT;
			break;
		case SENSOR_IOCTL_SET_DELAY:
			 dprintk("----dmard06 1----\n");
			if (atomic_read(&dmard06->enabled)) {
				mutex_lock(&dmard06->lock);
				if (copy_from_user(&interval, argp, sizeof(interval)))
					return -EFAULT;
				interval *= 10;  //for Sensor_new
				if (interval < dmard06->pdata->min_interval )
					interval = dmard06->pdata->min_interval;
				else if (interval > dmard06->pdata->max_interval)
					interval = dmard06->pdata->max_interval;
				dmard06->pdata->poll_interval = interval;
			//	dmard06_acc_update_odr(dmard06, dmard06->pdata->poll_interval);
			 dprintk("----ryder:set delay  not effect----\n");
				mutex_unlock(&dmard06->lock);
			//	dprintk("----dmard06 call SET_DELAY ----\n");
			}
			break;
		case SENSOR_IOCTL_SET_ACTIVE:
			 dprintk("----ryder:set active----\n");
			mutex_lock(&dmard06->lock);
			if (copy_from_user(&interval, argp, sizeof(interval)))
				return -EFAULT;
			if (interval > 1)
				return -EINVAL;
			if (interval){
			 /*dprintk("----dmard06 2.1----\n");
				dmard06->power_tag = interval;
				dmard06_acc_regulator_enbale(dmard06);
				dmard06_acc_enable(dmard06);
				dmard06_acc_update_odr(dmard06, dmard06->pdata->poll_interval);
                */
			//	dprintk("----dmard06 call SET_ACTIVE ----\n");
			}else{
			 /*dprintk("----dmard06 2.2----\n");
				dmard06->power_tag = interval;
				dmard06_acc_disable(dmard06);
				mdelay(2);
				if (atomic_cmpxchg(&dmard06->regulator_enabled, 1, 0))
					regulator_disable(dmard06->power);
			//	dprintk("----dmard06 call SET_NOt_ACTIVE ----\n");
            //	*/
			}
			mutex_unlock(&dmard06->lock);
			break;
		case SENSOR_IOCTL_GET_ACTIVE:
			interval = atomic_read(&dmard06->enabled);
			if (copy_to_user(argp, &interval, sizeof(interval)))
				return -EINVAL;
			break;
		case SENSOR_IOCTL_GET_DATA:
			if (copy_to_user(argp, &hardware_data_dmard06, sizeof(hardware_data_dmard06)))
				return -EINVAL;
			break;

		case SENSOR_IOCTL_GET_DATA_MAXRANGE:
			if (copy_to_user(argp, &dmard06->pdata->g_range, sizeof(dmard06->pdata->g_range)))
				return -EFAULT;
			break;

		case SENSOR_IOCTL_WAKE:
			input_event(dmard06->input_dev, EV_SYN,SYN_CONFIG, 0);
			break;
		default:
			return -EINVAL;
	}

	return 0;
}


static const struct file_operations dmard06_misc_fops = {
	.owner = THIS_MODULE,
	.open = dmard06_misc_open,
	.unlocked_ioctl = dmard06_misc_ioctl,
};

static int dmard06_acc_dev_init(struct dmard06_acc_data *acc)
{
	acc->dmard06_misc_device.minor = MISC_DYNAMIC_MINOR,
	acc->dmard06_misc_device.name =  "g_sensor",
	acc->dmard06_misc_device.fops = &dmard06_misc_fops;
	return misc_register(&acc->dmard06_misc_device);
}





s16 filter_call(s8* data, int size)
{
    int index;
    s8 max, min;
    int count = 0;
    s8 value = 0;
    max = min = data[0];
    for (index=0; index < size; index++) {
    if (data[index] > max) {
    max = data[index];
    }
    if (data[index] < min) {
    min = data[index];
    }
    count += data[index];
    }

    if (size <= 3) {
    value = count / size;
    } else {
        value = (count - max - min) / (size - 2);
    }

    return value;
}
static s8 *sensorlayout=NULL;
static s8 sensorlayout1[3][3]={ { 1, 0, 0},	{ 0, 1,	0}, { 0, 0, 1}};
static s8 sensorlayout2[3][3]={{ 0, 1, 0},    {-1, 0,	0},   { 0, 0, 1}};
static s8 sensorlayout3[3][3]={{-1, 0, 0},	{ 0,-1,	0}, { 0, 0, 1}};
static s8 sensorlayout4[3][3]={{ 0,-1, 0},	{ 1, 0,	0}, { 0, 0, 1}};
static s8 sensorlayout5[3][3]={{-1, 0, 0},	{ 0, 1,	0}, { 0, 0,-1}};
static s8 sensorlayout6[3][3]={ { 0,-1, 0}, {-1, 0,	0}, { 0, 0,-1}};
static s8 sensorlayout7[3][3]={{ 1, 0, 0},	{ 0,-1,	0}, { 0, 0,-1}};
static s8 sensorlayout8[3][3]={{ 0, 1, 0},	{ 1, 0,	0}, { 0, 0,-1}};
void remap_layout()
{
    switch(1)
    {
    case 1:
        sensorlayout=sensorlayout1;
        break;
    case 2:
        sensorlayout=sensorlayout2;
        break;
    case 3:
        sensorlayout=sensorlayout3;
        break;
    case 4:
        sensorlayout=sensorlayout4;
        break;
    case 5:
        sensorlayout=sensorlayout5;
        break;
    case 6:
        sensorlayout=sensorlayout6;
        break;
    case 7:
        sensorlayout=sensorlayout7;
        break;
    case 8:
        sensorlayout=sensorlayout8;
        break;
    default:
        break;
    }
        
}
static int device_i2c_xyz_read_reg(struct i2c_client *client,u8 *buffer, int length)
{
    //static  unsigned int flag 
	u8 cAddress = 0;
	cAddress = 0x41;
	int i = 0;
	//printk("DMARD06%s\n",__FUNCTION__);
	for(i=0;i<SENSOR_DATA_SIZE;i++)
	{
      //  dprintk("*%s*  *%x*",client,cAddress+i);
		buffer[i] = i2c_smbus_read_byte_data(client,cAddress+i);
	}
}

static int get_flag_bit( u8 value)
{
	s8 num = 0;
	int aaa = value & (1<<6);
	num = value & 0x3f;

	if(aaa == 0)
	{
		num = num;
	}
	else
	{
		num=num-64;
	}
	return num;
}
void device_i2c_read_xyz(struct i2c_client *client, s8 *xyz_p)
{
	s8 xyzTmp[SENSOR_DATA_SIZE*SAMPLE_COUNT];
	s8 xyzTmp2[SENSOR_DATA_SIZE]={0,0,0};
	int i, j;
	
	u8 buffer[3];
	IN_FUNC_MSG;   
	for (j=0;j<SAMPLE_COUNT;j++){
		device_i2c_xyz_read_reg(client, buffer, 3); 
        
		for(i = 0; i < SENSOR_DATA_SIZE; i++){
			xyzTmp[i*SAMPLE_COUNT+j] = get_flag_bit((buffer[i] >> 1));
		}
	}

	xyzTmp2[0]=filter_call(xyzTmp,SAMPLE_COUNT);
	xyzTmp2[1]=filter_call(xyzTmp+SAMPLE_COUNT,SAMPLE_COUNT);
	xyzTmp2[2]=filter_call(xyzTmp+2*SAMPLE_COUNT,SAMPLE_COUNT);
	//transfer to the default layout
	for(i = 0; i < SENSOR_DATA_SIZE; ++i)
	{
		xyz_p[i] = 0;
		for(j = 0; j < SENSOR_DATA_SIZE; ++j)
			xyz_p[i] += *(sensorlayout+i*3+j)* xyzTmp2[j];
	}

	//for(j=0;j<SAMPLE_COUNT*SENSOR_DATA_SIZE;j++)
	//remap_xyz(xyz_p,xyz_p+1,xyz_p+2);
	//PRINT_X_Y_Z(xyz_p[0], xyz_p[1], xyz_p[2]);
}
static void dmard06_acc_delayed_work_fun()
{
        
 //     s8 xyz[SENSOR_DATA_SIZE];
        s8 xyz[3];
        device_i2c_read_xyz( dmard06_acc->client, (s8 *)&xyz);
        input_report_abs(dmard06_acc->input_dev, ABS_X, -xyz[0]*2);
        input_report_abs(dmard06_acc->input_dev, ABS_Y, -xyz[1]*2);
        input_report_abs(dmard06_acc->input_dev, ABS_Z, -xyz[2]*2);
        input_sync(dmard06_acc->input_dev);        
 //       queue_delayed_work(dmard06_acc->work_queue,&dmard06_acc->dmard06_acc_delayed_work,msecs_to_jiffies(dmard06_acc->pdata->poll_interval));
        queue_delayed_work(dmard06_acc->work_queue,&dmard06_acc->dmard06_acc_delayed_work,msecs_to_jiffies(100));
     // printk("~~~~~~~~~~~~~~DMard X=%d,Y=%d,Z=%d\n ", xyz[0],xyz[1],xyz[2]);        
        return ;
}

static irqreturn_t dmard06_acc_interrupt(int irq, void *dev_id)
{

	struct dmard06_acc_data *acc = dev_id;

	/*
	if(acc->is_suspend == 1 || atomic_read(&acc->enabled) == 0){
		dprintk("---interrupt -suspend or disable\n");
		return IRQ_HANDLED;
	}
	*/
	disable_irq_nosync(acc->client->irq);
	if(!work_pending(&acc->irq_work))
		queue_work(acc->irq_work_queue, &acc->irq_work);
	else{
	}
	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void dmard06_acc_late_resume(struct early_suspend *handler);
static void dmard06_acc_early_suspend(struct early_suspend *handler);
#endif

int gsensor_reset(struct dmard06_acc_data *acc)
{
acc->power= regulator_get(&acc->client->dev, "vgsensor");//get regular 
	char cAddress = 0 , cData = 0;
	int result;
	s8 xyz[SENSOR_DATA_SIZE];
	int buffer[3];
    //if(!dmard_acc_regulator_enbale(acc));
    dmard_acc_regulator_enbale(acc);
	cAddress = SW_RESET;
    result = i2c_smbus_read_byte_data(acc->client,cAddress);
    dprintk(KERN_INFO "i2c Read SW_RESET = %x \n", result);
    cAddress = WHO_AM_I;
    result = i2c_smbus_read_byte_data(acc->client,cAddress);
    dprintk( "i2c Read WHO_AM_I = %d \n", result);
    cData=result;
    if(( cData&0x00FF) == WHO_AM_I_VALUE) //read 0Fh should be 06, else some err there
    {
        dprintk( "@@@ %s gsensor registered I2C driver!\n",__FUNCTION__);
       // dev.client = client;
    }
    else
    {
        dprintk( "@@@ %s gsensor I2C err = %d!\n",__FUNCTION__,cData);
       // dev.client = NULL;
        return -1;
    }
    remap_layout();
	return 0;
}
static int dmard06_acc_probe(struct i2c_client *client,		const struct i2c_device_id *id)
{
	struct dmard06_acc_data *acc;
	int err = -1,result=0;
	u8 buf[7]={0};
	dprintk("%s: probe start.\n", DMARD06_ACC_DEV_NAME);
	if (client->dev.platform_data == NULL) {
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

//	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
	result = i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE|I2C_FUNC_SMBUS_BYTE_DATA);
    if(!result)	{
        dev_err(&client->dev, "client not i2c capable\n");
		err = -ENODEV;
		goto exit_check_functionality_failed;
	}

	dmard06_acc = acc=kzalloc(sizeof(struct dmard06_acc_data), GFP_KERNEL);
	if (acc == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,"failed to allocate memory for module data: %d\n", err);
		goto exit_check_functionality_failed;
	}

	mutex_init(&acc->lock_rw);
	mutex_init(&acc->lock);
	acc->client = client;
//	i2c_set_clientdata(client, acc);

	acc->pdata = kmalloc(sizeof(*acc->pdata), GFP_KERNEL);
	if (acc->pdata == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev,
				"failed to allocate memory for pdata: %d\n",
				err);
		goto err_free;
	}
	memcpy(acc->pdata, client->dev.platform_data, sizeof(*acc->pdata));
	if (acc->pdata->init) {
		err = acc->pdata->init();
		if (err < 0) {
            dev_err(&client->dev, "init failed: %d\n", err);
			goto err_pdata_init;
		}
	}
	err = dmard06_acc_input_init(acc);
	if (err < 0) {
		dev_err(&client->dev, "input init failed\n");
		goto err_power_off;
	}
    gsensor_reset(acc);
	err = dmard06_acc_dev_init(acc);
    if(err) dprintk("ryder : failed to regist msic dev");
    acc->work_queue=create_workqueue("my_devpoll");
    INIT_DELAYED_WORK(&acc->dmard06_acc_delayed_work,dmard06_acc_delayed_work_fun);
	if(!(acc->work_queue)){
		err = -ESRCH;
		dprintk("creating workqueue failed\n");
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	acc->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	acc->early_suspend.suspend = dmard06_acc_early_suspend;
	acc->early_suspend.resume = dmard06_acc_late_resume;
	register_early_suspend(&acc->early_suspend);
#endif
	return 0;
err_power_off:
err_pdata_init:
	if (acc->pdata->exit)
		acc->pdata->exit();
err_read_who_am_i:
dmard_acc_regulator_enbale(acc);
exit_kfree_pdata:
	kfree(acc->pdata);
err_free:
	kfree(acc);
exit_check_functionality_failed:
	dprintk(KERN_ERR "%s: Driver Init failed\n", DMARD06_ACC_DEV_NAME);
	return err;
}

static int __devexit dmard06_acc_remove(struct i2c_client *client)
{
	int err;
	struct dmard06_acc_data *acc = i2c_get_clientdata(client);

	if(acc->pdata->gpio_int >= 0){
		destroy_workqueue(acc->irq_work_queue);
	}

	dmard06_acc_input_cleanup(acc);
	dmard06_acc_device_power_off(acc);
	if (acc->pdata->exit)
		acc->pdata->exit();
        kfree(acc->pdata);
	if (acc->power && atomic_cmpxchg(&acc->regulator_enabled, 1, 0)){
err=dmard_acc_regulator_enbale(acc);
		if (err < 0){
			dev_err(&acc->client->dev,
					"power_off regulator failed: %d\n", err);
			return err;
		}
		regulator_put(acc->power);
	}
	kfree(acc);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void dmard06_acc_late_resume(struct early_suspend *handler)
{
	struct dmard06_acc_data *acc;
	acc = container_of(handler, struct dmard06_acc_data, early_suspend);
	acc->is_suspend = 0;
	dmard06_acc_regulator_enbale(acc);
	if (!atomic_read(&acc->enabled)) {
		mutex_lock(&acc->lock);
		dmard06_acc_enable(acc);
		mutex_unlock(&acc->lock);
	}
}

static void dmard06_acc_early_suspend(struct early_suspend *handler)
{
	struct dmard06_acc_data *acc ;

	acc = container_of(handler, struct dmard06_acc_data, early_suspend);
	acc->is_suspend = 1;

//	disable_irq_nosync(acc->client->irq);
	if (atomic_read(&acc->enabled)) {
		dmard06_acc_disable(acc);
	}
	if (atomic_cmpxchg(&acc->regulator_enabled, 1, 0)) {
		regulator_disable(acc->power);
	}
}
#endif



static const struct i2c_device_id dmard06_acc_id[] = { { DMARD06_ACC_DEV_NAME, 0 }, { }, };

MODULE_DEVICE_TABLE(i2c, dmard06_acc_id);

static struct i2c_driver dmard06_acc_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = DMARD06_ACC_DEV_NAME,
	},
	.probe = dmard06_acc_probe,
	.remove = __devexit_p(dmard06_acc_remove),
	.id_table = dmard06_acc_id,
};

static int __init dmard06_acc_init(void)
{
	dprintk(KERN_INFO "%s accelerometer driver: init\n",DMARD06_ACC_DEV_NAME);
	return i2c_add_driver(&dmard06_acc_driver);
}

static void __exit dmard06_acc_exit(void)
{
	dprintk(KERN_INFO "%s accelerometer driver exit\n",DMARD06_ACC_DEV_NAME);
	i2c_del_driver(&dmard06_acc_driver);
	return;
}

module_init(dmard06_acc_init);
module_exit(dmard06_acc_exit);

MODULE_DESCRIPTION("Dmard06  digital accelerometer  kernel driver");
MODULE_AUTHOR("dwjia,Ingenic");
MODULE_LICENSE("GPL");

