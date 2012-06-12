#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include <mach/jzsnd.h>

static void snd_switch_set_state(struct snd_switch_data *switch_data, int state)
{
	switch_set_state(&switch_data->sdev, state);

	if (switch_data->type == SND_SWITCH_TYPE_GPIO) {
		if (switch_data->valid_level == 1) {
			if (state) {
				irq_set_irq_type(switch_data->irq, IRQF_TRIGGER_LOW);
				switch_data->valid_level = 0;
			}
			else
				irq_set_irq_type(switch_data->irq, IRQF_TRIGGER_HIGH);
		} else {
			if (state) {
				irq_set_irq_type(switch_data->irq, IRQF_TRIGGER_HIGH);
				switch_data->valid_level = 1;
			}
			else
				irq_set_irq_type(switch_data->irq, IRQF_TRIGGER_LOW);
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
		//__gpio_disable_pull(switch_data->gpio);
		state = gpio_get_value(switch_data->gpio);
		for (i = 0; i < 5; i++) {
			msleep(20);
			//__gpio_disable_pull(data->gpio);
			tmp_state = gpio_get_value(switch_data->gpio);
			if (tmp_state != state) {
				i = -1;
				//__gpio_disable_pull(data->gpio);
				state = gpio_get_value(switch_data->gpio);
				continue;
			}
		}

		if (state == switch_data->valid_level) {
			state = 1;
		}
		else
			state = 0;
	}

	/* if codec internal hpsense */
	if (switch_data->type == SND_SWITCH_TYPE_CODEC) {
		state = switch_data->codec_get_sate();
	}

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
	struct snd_switch_data *switch_data = platform_get_drvdata(pdev);

	if (!switch_data)
		return -1;

	snd_switch_work(&switch_data->work);

	return 0;
}

static ssize_t switch_snd_print_name(struct switch_dev *sdev, char *buf)
{
	struct snd_switch_data *switch_data =
		container_of(sdev ,struct snd_switch_data, sdev);

	if (!switch_data->name_on&&!switch_data->name_off)
		return sprintf(buf,"%s.\n",sdev->name);

	return -1;
}

static ssize_t switch_snd_print_state(struct switch_dev *sdev, char *buf)
{
	struct snd_switch_data *switch_data =
		container_of(sdev, struct snd_switch_data, sdev);
	const char *state;

	if (switch_get_state(sdev))
		state = switch_data->state_on;
	else
		state = switch_data->state_off;

	if (state)
		return sprintf(buf, "%s\n", state);

	return -1;
}

static int snd_switch_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct snd_switch_data *switch_data = platform_get_drvdata(pdev);

	switch_data->sdev.print_state = switch_snd_print_state;
	switch_data->sdev.print_name = switch_snd_print_name;

	ret = switch_dev_register(&switch_data->sdev);
	if (ret < 0)
		goto err_switch_dev_register;

	INIT_WORK(&switch_data->work, snd_switch_work);

	if (switch_data->type == SND_SWITCH_TYPE_GPIO) {

		if (!gpio_is_valid(switch_data->gpio))
			goto err_test_gpio;

		ret = gpio_request(switch_data->gpio, pdev->name);
		if (ret < 0)
			goto err_request_gpio;

		ret = gpio_direction_input(switch_data->gpio);
		if (ret < 0)
			goto err_set_gpio_input;

		switch_data->irq = gpio_to_irq(switch_data->gpio);
		if (switch_data->irq < 0) {
			ret = switch_data->irq;
			goto err_detect_irq_num_failed;
		}

		ret = request_irq(switch_data->irq, snd_irq_handler,
						  IRQF_TRIGGER_LOW, pdev->name, switch_data);
		if (ret < 0)
			goto err_request_irq;

	}

	/* Perform initial detection */
	snd_switch_work(&switch_data->work);

	return 0;

err_request_irq:
err_detect_irq_num_failed:
err_set_gpio_input:
	gpio_free(switch_data->gpio);
err_request_gpio:
err_test_gpio:
    switch_dev_unregister(&switch_data->sdev);
err_switch_dev_register:

	return ret;
}

static int __devexit snd_switch_remove(struct platform_device *pdev)
{
	struct snd_switch_data *switch_data = platform_get_drvdata(pdev);

	if (switch_data->type == SND_SWITCH_TYPE_GPIO) {
		cancel_work_sync(&switch_data->work);
		gpio_free(switch_data->gpio);
	}

    switch_dev_unregister(&switch_data->sdev);

	kfree(switch_data);

	return 0;
}


struct platform_device_id xb_snd_det_ids[] = {
	{
		.name 			= DEV_DSP_HP_DET_NAME,
		.driver_data 	= SND_DEV_DETECT0_ID,
	},
	{
		.name 			= DEV_DSP_DOCK_DET_NAME,
		.driver_data 	= SND_DEV_DETECT0_ID,
	},
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

module_init(snd_switch_init);
module_exit(snd_switch_exit);
