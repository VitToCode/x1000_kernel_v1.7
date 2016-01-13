#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>

#define TM57PE20A_NAME "tm57pe20a-touch-button"
#define TM57PE20A_PHY "tm57pe20a-touch-button/input0"

unsigned short key = 0;
int key_press = 0;
unsigned int key_value = 0;
int key_flag = 0;
static struct workqueue_struct *tm57pe20a_wq;
struct tm57pe20a_touch_platform_data {
	int intr;
	int sda;
	int sclk;
};

struct tm57pe20a_touch_data {
	struct tm57pe20a_touch_platform_data *pdata;
	struct input_dev *input;
	struct work_struct work;
	int irq;
	int sclk_irq;
	int irq_times;
	unsigned short data;
	unsigned short tmp_data;
	unsigned short key_press;
	spinlock_t lock;
};
struct tm57pe20a_key {
	unsigned short key;
	unsigned int value;
};
struct tm57pe20a_key key_table[] = {
	{1,KEY_PLAYPAUSE},
	{2,KEY_NEXTSONG},
	{4,KEY_F5},
	{8,KEY_RECORD},
	{0x10,KEY_F3},
	{0x20,KEY_PREVIOUSSONG},
	{0x100,KEY_VOLUMEUP},
	{0xa00,KEY_VOLUMEDOWN},
	{0x40,KEY_F7},
};
static int  press_count = 0;
static int key_change = 0;
static int key_count = 0;
static int volume_key = 0;
static int first_report = 0;
static int press_time = 0;
static int play_flag = 0;
#define PRESS_TIMES 150 
static int report_key(struct tm57pe20a_touch_data *pdata)
{
	int i;
	int flags;
	int play_value = 0;;
	if(pdata->data == 0){
		key_press = 0;
		volume_key = 0;
		key_change = 0;
		first_report = 0;
		if(play_flag){
			if(press_time >= PRESS_TIMES) //long_press 3s
				play_value = KEY_F7;
			else if(press_time >= 2)
				play_value = KEY_PLAYPAUSE;
			play_flag = 0;
			press_time = 0;
			if(play_value ){
				input_event(pdata->input,EV_KEY,play_value,1);	
				input_sync(pdata->input);	
				input_event(pdata->input,EV_KEY,play_value,0);	
				input_sync(pdata->input);
			}
		}			

	}else if(pdata->data < 0x100){
		if(pdata->data == 1){
			press_time++;
			play_flag = 1;
			return ;
		}
		if(!key_press){
			if(play_flag == 1)
				return;
			key_change = 0;
			volume_key = 0;
			press_count++;
			if(press_count < 1)
				return ;
			press_count = 0;
			key_press = 1;
			key = pdata->data;
			for(i = 1;i < 6;i++){
				key_value = key_table[i].value;
				input_event(pdata->input,EV_KEY,key_table[i].value,1);	
				input_sync(pdata->input);	
				input_event(pdata->input,EV_KEY,key_table[i].value,0);	
				input_sync(pdata->input);
				break;
			}
		}
	}else if(pdata->data < 0xb00){
		pdata->data = pdata->data >> 8;
		if(!volume_key){
			volume_key = pdata->data;
			key_change = 0;
			first_report = 1;
		}
		if(!key_press){
			key_press = 1;
		}else{
			if(volume_key != pdata->data){
				key_change++;
				if(key_change < 2){
					return 0;
				}
				key_change = 0;
				if(first_report == 1){
					first_report = 0;
					return 0;
				}
		//	printk("222.volume_key= %d,pdata->data = %d\n",volume_key,pdata->data);
				if (volume_key <  pdata->data){
					key_value = KEY_VOLUMEDOWN;
					if((volume_key == 1 || volume_key == 2 || volume_key == 3)&& (pdata->data == 10 || pdata->data == 9 ||pdata->data == 8)){
						key_value = KEY_VOLUMEUP;
					}
				}else if (volume_key >  pdata->data ){
					key_value = KEY_VOLUMEUP;
					if((volume_key == 8 || volume_key == 9 || volume_key == 10)&& (pdata->data == 1 || pdata->data == 2 || pdata->data == 3)){
						key_value = KEY_VOLUMEDOWN;
					}
				}
				input_event(pdata->input,EV_KEY,key_value,1);
				input_sync(pdata->input);
				input_event(pdata->input,EV_KEY,key_value,0);
				input_sync(pdata->input);
				volume_key = pdata->data;
			}else 
				key_change = 0;
		}
		key_press = 1;
	}
}
static void tm57pe20a_touch_work_func(struct work_struct *work)
{
	struct tm57pe20a_touch_data *pdata ;
	pdata = container_of(work, struct tm57pe20a_touch_data, work);
	report_key(pdata);	
}

static irqreturn_t tm57pe20a_sclk_irq_handler(int irq, void *dev_id)
{
	struct tm57pe20a_touch_data *pdata = dev_id;
	pdata->irq_times++;
	unsigned char bit;
	bit = __gpio_get_value(pdata->pdata->sda);
	pdata->data = (pdata->data << 1) | bit;
	if(pdata->irq_times == 16){
		pdata->irq_times = 0;
		udelay(1500);
//		printk("===>pdata->data = 0x%x\n",pdata->data);
		disable_irq_nosync(pdata->sclk_irq);
	//	schedule_work(&pdata->work);
		report_key(pdata);
		enable_irq(pdata->irq);
	}
	return IRQ_HANDLED;
}
static irqreturn_t tm57pe20a_touch_irq_handler(int irq, void *dev_id)
{
	struct tm57pe20a_touch_data *pdata = dev_id;
	disable_irq_nosync(pdata->irq);
	pdata->data = 0;
	enable_irq(pdata->sclk_irq);
	pdata->irq_times = 0;
	return IRQ_HANDLED;
}
static int tm57pe20a_gpio_init(struct tm57pe20a_touch_data *pdata)
{
	if (gpio_request_one(pdata->pdata->intr,GPIOF_DIR_IN, "tm57pe20a_irq")) {
		printk("no tm57pe20a_irq pin available\n");
		pdata->pdata->intr = -EBUSY;
		return -1;
	}
	
	if (gpio_request_one(pdata->pdata->sclk,GPIOF_DIR_IN,"tm57pe20a_sclk")) {
		printk("no tm57pe20a_sclk pin available\n");
		pdata->pdata->sclk = -EBUSY;
		return -1;
	}

	if (gpio_request_one(pdata->pdata->sda,GPIOF_DIR_IN,"tm57pe20a_sda")) {
		printk("no tm57pe20a_sda pin available\n");
		pdata->pdata->sda = -EBUSY;
		return -1;
	}
	gpio_set_pull(pdata->pdata->sda,1);

	return 0;
}
static int __devinit tm57pe20a_touch_bt_probe(struct platform_device *pdev)
{
	struct tm57pe20a_touch_data *tm_data;
	struct device *dev = &pdev->dev;
	int error;
	int irq;
	tm_data = kzalloc(sizeof(struct tm57pe20a_touch_data),GFP_KERNEL);
	if(tm_data == NULL){
		dev_err(dev, "failed to allocate state\n");
		error = -ENOMEM;
		goto fail;
	}
	tm_data->pdata = pdev->dev.platform_data;
	tm_data->input = input_allocate_device();
	if (tm_data->input == NULL) {
		dev_err(dev, "failed to allocate state\n");
		error = -ENOMEM;
		goto fail;
	}

	tm_data->input->name = TM57PE20A_NAME;
	tm_data->input->phys = TM57PE20A_PHY;
	tm_data->input->id.bustype = BUS_HOST;
	tm_data->input->dev.parent = &pdev->dev;
	tm_data->input->id.vendor = 0x0001;
	tm_data->input->id.product = 0x0001;
	tm_data->input->id.version = 0x0100;
	__set_bit(EV_KEY, tm_data->input->evbit);
	int i;
	for(i = 0;i < sizeof(key_table) / sizeof(key_table[0]);i++)
		input_set_capability(tm_data->input,EV_KEY,key_table[i].value);

	error = input_register_device(tm_data->input);
	if (error) {
		dev_err(dev, "Unable to register input device:error %d\n",error);
		goto fail;
	}

	spin_lock_init(&tm_data->lock);
	INIT_WORK(&tm_data->work, tm57pe20a_touch_work_func);
	tm57pe20a_wq = create_singlethread_workqueue("tm57pe20a_touch");

	if(tm57pe20a_gpio_init(tm_data)){
		printk("gpio init error\n");
		return error;
	}

	tm_data->irq = gpio_to_irq(tm_data->pdata->intr);
	tm_data->irq_times = 0;
	tm_data->data = 0;
	tm_data->tmp_data = 0;
	error = request_irq(tm_data->irq, tm57pe20a_touch_irq_handler,
			IRQF_TRIGGER_FALLING | IRQF_DISABLED,
			tm_data->input->name,tm_data);

	if (error != 0) {
		dev_err(dev, "request irq is error\n");
		return error;
	}

	tm_data->sclk_irq = gpio_to_irq(tm_data->pdata->sclk);
	
	error = request_irq(tm_data->sclk_irq, tm57pe20a_sclk_irq_handler,
			IRQF_TRIGGER_RISING |IRQF_DISABLED,
			tm_data->input->name,tm_data);
	
	if (error != 0) {
		dev_err(dev, "request irq is error\n");
		return error;
	}
	disable_irq(tm_data->sclk_irq);
	return 0;
fail:
	printk("tm57pe20a probe failed!!!\n");
}

static int __devexit tm57pe20a_touch_bt_remove(struct platform_device *pdev)
{
	return 0;
}
#ifdef CONFIG_PM
static int tm57pe20a_touch_bt_suspend(struct device *dev)
{
	return 0;
}
static int tm57pe20a_touch_bt_resume(struct device *dev)
{
	return 0;
}
static const struct dev_pm_ops tm57pe20a_touch_bt_pm_ops = {
	.suspend        = tm57pe20a_touch_bt_suspend,
	.resume         = tm57pe20a_touch_bt_resume,
};
#endif

static struct platform_driver tm57pe20a_touch_bt_device_driver = {
	.probe          = tm57pe20a_touch_bt_probe,
	.remove         = __devexit_p(tm57pe20a_touch_bt_remove),
	.driver         = {
		.name   = "tm57pe20a-touch-button",
		.owner  = THIS_MODULE,
#ifdef CONFIG_PM
		.pm     = &tm57pe20a_touch_bt_pm_ops,
#endif
	}
};

static int __init tm57pe20a_touch_bt_init(void)
{
	return platform_driver_register(&tm57pe20a_touch_bt_device_driver);
}

static void __exit tm57pe20a_touch_bt_exit(void)
{
	platform_driver_unregister(&tm57pe20a_touch_bt_device_driver);
}

module_init(tm57pe20a_touch_bt_init);
module_exit(tm57pe20a_touch_bt_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jxsun <jingxin.sun@ingenic.com>");
MODULE_DESCRIPTION("Keyboard driver for CPU GPIOs");
MODULE_ALIAS("platform:tm57pe20a touch button");
