#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/wait.h>

#include <mach/jzsnd.h>

#include "xb_snd_detect.h"

static void snd_switch_set_state(struct snd_switch_data *switch_data, int state)
{
	switch_set_state(&switch_data->sdev, state);

	if (switch_data->type == SND_SWITCH_TYPE_GPIO) {
		if (switch_data->hp_valid_level == HIGH_VALID) {
			if (state) {
				irq_set_irq_type(switch_data->irq, IRQF_TRIGGER_FALLING);
				//irq_set_irq_type(switch_data->irq, IRQF_TRIGGER_LOW);
			} else {
				irq_set_irq_type(switch_data->irq, IRQF_TRIGGER_RISING);
				//irq_set_irq_type(switch_data->irq, IRQF_TRIGGER_HIGH);
			}
		} else if (switch_data->hp_valid_level == LOW_VALID) {
			if (state) {
				//irq_set_irq_type(switch_data->irq, IRQF_TRIGGER_HIGH);
				irq_set_irq_type(switch_data->irq, IRQF_TRIGGER_RISING);
			} else {
				irq_set_irq_type(switch_data->irq, IRQF_TRIGGER_FALLING);
				//irq_set_irq_type(switch_data->irq, IRQF_TRIGGER_LOW);
			}
		}
	}
}

static void snd_switch_work(struct work_struct *work)
{
	int state = 0;
	int tmp_state =0;
	int i = 0;
	struct snd_switch_data *switch_data =
		container_of(work, struct snd_switch_data, work);

	/* if gipo switch */
	if (switch_data->type == SND_SWITCH_TYPE_GPIO) {
		//__gpio_disable_pull(switch_data->hp_gpio);
		gpio_direction_input(switch_data->hp_gpio);
		state = gpio_get_value(switch_data->hp_gpio);
		for (i = 0; i < 5; i++) {
			msleep(20);
			//__gpio_disable_pull(data->hp_gpio);
			tmp_state = gpio_get_value(switch_data->hp_gpio);
			if (tmp_state != state) {
				i = -1;
				//__gpio_disable_pull(data->hp_gpio);
				state = gpio_get_value(switch_data->hp_gpio);
				continue;
			}
		}

		if (state == (int)switch_data->hp_valid_level)
			state = 1;
		else
			state = 0;
	}

	/* if codec internal hpsense */
	if (switch_data->type == SND_SWITCH_TYPE_CODEC) {
		state = switch_data->codec_get_sate();
	}

	if (state == 1 && switch_data->mic_gpio != -1) {
		gpio_direction_input(switch_data->mic_gpio);
		if (gpio_get_value(switch_data->mic_gpio) != switch_data->mic_vaild_level)
			state <<= 1;
		else
			state <<= 0;
	} else
		state <<= 1;

	snd_switch_set_state(switch_data, state);
}

static irqreturn_t snd_irq_handler(int irq, void *dev_id)
{
	struct snd_switch_data *switch_data =
	    (struct snd_switch_data *)dev_id;

	schedule_work(&switch_data->work);
	return IRQ_HANDLED;
}

static int snd_switch_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int snd_switch_resume(struct platform_device *pdev)
{
	struct snd_switch_data *switch_data = pdev->dev.platform_data;

	if (!switch_data) {
		printk("hp detect device resume fail.\n");
		return 0;
	}

	snd_switch_work(&switch_data->work);

	return 0;
}

static ssize_t switch_snd_print_name(struct switch_dev *sdev, char *buf)
{
	struct snd_switch_data *switch_data =
		container_of(sdev ,struct snd_switch_data, sdev);

	if (!switch_data->name_headset_on &&
			!switch_data->name_headset_on&&
			!switch_data->name_off)
		return sprintf(buf,"%s.\n",sdev->name);

	return -1;
}

static ssize_t switch_snd_print_state(struct switch_dev *sdev, char *buf)
{
	struct snd_switch_data *switch_data =
		container_of(sdev, struct snd_switch_data, sdev);
	const char *state;
	unsigned int state_val = switch_get_state(sdev);
	if ( state_val == 1)
		state = switch_data->state_headset_on;
	else if ( state_val == 2)
		state = switch_data->state_headphone_on;
	else
		state = switch_data->state_off;

	if (state)
		return sprintf(buf, "%s\n", state);

	return -1;
}

static int snd_switch_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct snd_switch_data *switch_data = pdev->dev.platform_data;

	switch_data->sdev.print_state = switch_snd_print_state;
	switch_data->sdev.print_name = switch_snd_print_name;

	ret = switch_dev_register(&switch_data->sdev);
	if (ret < 0) {
		printk("switch dev register fail.\n");
		goto err_switch_dev_register;
	}

	INIT_WORK(&switch_data->work, snd_switch_work);

	if (switch_data->type == SND_SWITCH_TYPE_GPIO) {

		if (!gpio_is_valid(switch_data->hp_gpio))
			goto err_test_gpio;

		ret = gpio_request(switch_data->hp_gpio, pdev->name);
		if (ret < 0)
			goto err_request_gpio;


		switch_data->irq = gpio_to_irq(switch_data->hp_gpio);
		if (switch_data->irq < 0) {
			printk("get irq error.\n");
			ret = switch_data->irq;
			goto err_detect_irq_num_failed;
		}

		ret = request_irq(switch_data->irq, snd_irq_handler,
						  IRQF_TRIGGER_FALLING, pdev->name, switch_data);
		if (ret < 0) {
			printk("requst irq fail.\n");
			goto err_request_irq;
		}

	} else {
		wake_up_interruptible(&switch_data->wq);
	}
	/* Perform initial detection */
	snd_switch_work(&switch_data->work);
	printk("snd_switch_probe susccess\n");
	return 0;

err_request_irq:
err_detect_irq_num_failed:
	gpio_free(switch_data->hp_gpio);
err_request_gpio:
err_test_gpio:
    switch_dev_unregister(&switch_data->sdev);
err_switch_dev_register:

	return ret;
}

static int __devexit snd_switch_remove(struct platform_device *pdev)
{
	struct snd_switch_data *switch_data = pdev->dev.platform_data;

	if (switch_data->type == SND_SWITCH_TYPE_GPIO) {
		cancel_work_sync(&switch_data->work);
		gpio_free(switch_data->hp_gpio);
	}

	switch_dev_unregister(&switch_data->sdev);

	kfree(switch_data);

	return 0;
}


static struct platform_device_id xb_snd_det_ids[] = {
#define JZ_HP_DETECT_TABLE(NO)	{.name = DEV_DSP_HP_DET_NAME,.driver_data = SND_DEV_DETECT##NO##_ID},	\
								{.name = DEV_DSP_DOCK_DET_NAME,.driver_data = SND_DEV_DETECT##NO##_ID}
	JZ_HP_DETECT_TABLE(0),
	//JZ_HP_DETECT_TABLE(1),
	{}
#undef JZ_HP_DETECT_TABLE
};



static struct platform_driver snd_switch_driver = {
	.probe		= snd_switch_probe,
	.remove		= __devexit_p(snd_switch_remove),
	.driver		= {
		.name	= "snd-det",
		.owner	= THIS_MODULE,
	},
	.id_table	= xb_snd_det_ids,
	.suspend	= snd_switch_suspend,
	.resume		= snd_switch_resume,
};

static int __init snd_switch_init(void)
{
	return platform_driver_register(&snd_switch_driver);
}

static void __exit snd_switch_exit(void)
{
	platform_driver_unregister(&snd_switch_driver);
}

device_initcall_sync(snd_switch_init);
module_exit(snd_switch_exit);
