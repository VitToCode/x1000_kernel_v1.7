/**
 * xb_snd_card.c
 *
 * jbbi <jbbi@ingenic.cn>
 *
 * 24 APR 2012
 *
 */
#include <mach/jzsnd.h>
#include "interface/xb_snd_dsp.h"
#include "interface/xb_snd_mixer.h"

static LIST_HEAD(ddata_head);

/*###########################################################*\
* support functions
\*###########################################################*/
static inline struct snd_dev_data* get_ddata_by_minor(int minor)
{
	struct snd_dev_data *ddata = NULL;

	list_for_each_entry(ddata, &ddata_head, list) {
		if (ddata && (ddata->minor == minor))
			return ddata;
	}

	printk("SOUND ERROR: %s(line:%d), no device has minor %d !\n",
		   __func__, __LINE__, minor);
	return NULL;
}

/*###########################################################*\
* file operations
\*###########################################################*/
/********************************************************\
 * llseek
\********************************************************/
static loff_t xb_snd_llseek(struct file *file, loff_t offset, int origin)
{
	loff_t ret = 0;
	int dev = iminor(file->f_path.dentry->d_inode);
	struct snd_dev_data *ddata = get_ddata_by_minor(dev);

	switch (dev & 0x0f) {
	case SND_DEV_DSP:
		ret = xb_snd_dsp_llseek(file, offset, origin, ddata);
		break;

	case SND_DEV_CTL:
		ret = xb_snd_mixer_llseek(file, offset, origin, ddata);
		break;
	}

	return ret;
}

/********************************************************\
 * read
\********************************************************/
static ssize_t xb_snd_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = -EIO;
	int dev = iminor(file->f_path.dentry->d_inode);
	struct snd_dev_data *ddata = get_ddata_by_minor(dev);

	if (ddata->is_suspend)
		return -EIO;

	if (count == 0)
		return 0;

	switch (dev & 0x0f) {
	case SND_DEV_DSP:
		ret = xb_snd_dsp_read(file, buffer, count, ppos, ddata);
		break;

	case SND_DEV_CTL:
		ret = xb_snd_mixer_read(file, buffer, count, ppos, ddata);
		break;
	}

	return ret;
}

/********************************************************\
 * write
\********************************************************/
static ssize_t xb_snd_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
	ssize_t ret = -EIO;
	int dev = iminor(file->f_path.dentry->d_inode);
	struct snd_dev_data *ddata = get_ddata_by_minor(dev);

	if (ddata->is_suspend)
		return -EIO;

	if (count == 0)
		return 0;

	switch (dev & 0x0f) {
	case SND_DEV_DSP:
		ret = xb_snd_dsp_write(file, buffer, count, ppos, ddata);
		break;

	case SND_DEV_CTL:
		ret = xb_snd_mixer_write(file, buffer, count, ppos, ddata);
		break;
	}

	return ret;
}

/********************************************************\
 * poll
\********************************************************/
static unsigned int xb_snd_poll(struct file *file, poll_table *wait)
{
	unsigned int ret = 0;
	int dev = iminor(file->f_path.dentry->d_inode);
	struct snd_dev_data *ddata = get_ddata_by_minor(dev);

	switch (dev & 0x0f) {
	case SND_DEV_DSP:
		ret = xb_snd_dsp_poll(file, wait, ddata);
		break;

	case SND_DEV_CTL:
		ret = xb_snd_mixer_poll(file, wait, ddata);
		break;
	}

	return ret;
}

/********************************************************\
 * ioctl
\********************************************************/
static long xb_snd_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	long ret = -EIO;
	int dev = iminor(file->f_path.dentry->d_inode);
	struct snd_dev_data *ddata = get_ddata_by_minor(dev);

	switch (dev & 0x0f) {
	case SND_DEV_DSP:
		ret = xb_snd_dsp_ioctl(file, cmd, arg, ddata);
		break;

	case SND_DEV_CTL:
		ret = xb_snd_mixer_ioctl(file, cmd, arg, ddata);
		break;
	}

	return ret;
}

/********************************************************\
 * mmap
\********************************************************/
static int xb_snd_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret = -EINVAL;
	int dev = iminor(file->f_path.dentry->d_inode);
	struct snd_dev_data *ddata = get_ddata_by_minor(dev);

	if (!((file->f_mode & FMODE_READ) && (file->f_mode & FMODE_WRITE))){
		printk("SOUND ERROR: %s(line:%d) use mmap must be opend as O_RDWR!\n",
			   __func__, __LINE__);
		return -EINVAL;
	}

	if (vma == NULL)
		return -EINVAL;

	switch (dev & 0x0f) {
	case SND_DEV_DSP:
		ret = xb_snd_dsp_mmap(file, vma, ddata);
		break;

	case SND_DEV_CTL:
		ret = xb_snd_mixer_mmap(file, vma, ddata);
		break;
	}

	return ret;
}

/********************************************************\
 * open
\********************************************************/
static int xb_snd_open(struct inode *inode, struct file *file)
{
	int ret = -ENXIO;
	int dev = iminor(inode);
	struct snd_dev_data *ddata = get_ddata_by_minor(dev);

	switch (dev & 0x0f) {
	case SND_DEV_DSP:
		ret = xb_snd_dsp_open(inode, file, ddata);
		break;

	case SND_DEV_CTL:
		ret = xb_snd_mixer_open(inode, file, ddata);
		break;
	}

	return ret;
}

/********************************************************\
 * release
\********************************************************/
static int xb_snd_release(struct inode *inode, struct file *file)
{
	int ret = -EINVAL;
	int dev = iminor(file->f_path.dentry->d_inode);
	struct snd_dev_data *ddata = get_ddata_by_minor(dev);

	switch (dev & 0x0f) {
	case SND_DEV_DSP:
		ret = xb_snd_dsp_release(inode, file, ddata);
		break;

	case SND_DEV_CTL:
		ret = xb_snd_mixer_release(inode, file, ddata);
		break;
	}

	return ret;
}

const struct file_operations xb_snd_fops = {
	.owner			= THIS_MODULE,
	.llseek			= xb_snd_llseek,
	.read			= xb_snd_read,
	.write			= xb_snd_write,
	.poll			= xb_snd_poll,
	.unlocked_ioctl	= xb_snd_ioctl,
	.mmap			= xb_snd_mmap,
	.open			= xb_snd_open,
	.release		= xb_snd_release,
};

/*########################################################*\
* devices driver
\*########################################################*/

/********************************************************\
 * xb_snd_suspend
\********************************************************/
static int xb_snd_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = -EINVAL;
	struct snd_dev_data *ddata = pdev->dev.platform_data;
	if (ddata && ddata->suspend)
		ret = ddata->suspend(pdev, state);
	return ret;
}

/********************************************************\
 * xb_snd_resume
\********************************************************/
static int xb_snd_resume(struct platform_device *pdev)
{
	int ret = -EINVAL;
	struct snd_dev_data *ddata = pdev->dev.platform_data;
	if (ddata && ddata->resume)
		ret = ddata->resume(pdev);
	return ret;
}

/********************************************************\
 * xb_snd_shutdown
\********************************************************/
static void xb_snd_shutdown(struct platform_device *pdev)
{
	struct snd_dev_data *ddata = pdev->dev.platform_data;
	if (ddata && ddata->shutdown)
		ddata->shutdown(pdev);
}

/********************************************************\
 * xb_snd_probe
\********************************************************/
static int xb_snd_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;
	int snd_minor = -1;
	struct snd_dev_data *ddata = pdev->dev.platform_data;

	if (ddata) {
		/* check minor */
		if (ddata->minor > MAX_SND_MINOR)
			return -EINVAL;

		/* device probe */
		switch (ddata->minor & 0x0f) {
		case SND_DEV_DSP:
			ret = xb_snd_dsp_probe(ddata);
			break;

		case SND_DEV_CTL:
			ret = xb_snd_mixer_probe(ddata);
			break;

		default:
			return -EINVAL;
		}

		/* register device */
		ret = register_sound_special(&xb_snd_fops, ddata->minor);
		if (ret != ddata->minor) {
			printk("SOUND ERROR: %s(line:%d) register sound device error! minor = %d\n",
				   __func__, __LINE__, snd_minor);
			return ret;
		}

		if (ddata->init) {
			if(ddata->init(pdev)) {
				printk("SOUND ERROR: device init error!\n");
				return -1;
			}
		}
	}

	list_add(&ddata->list ,&ddata_head);

	return 0;
}

struct platform_device_id xb_snd_driver_ids[] = {
	{
		.name 			= "dsp",
		.driver_data 	= 0,
	},
	{
		.name 			= "mixer",
		.driver_data 	= 0,
	},
};

__refdata static struct platform_driver xb_snd_driver = {
	.probe		=  xb_snd_probe,
	.driver		= {
		.name	= "dsp",
		.owner	= THIS_MODULE,
	},
	.id_table	= xb_snd_driver_ids,
	.suspend	= xb_snd_suspend,
	.resume		= xb_snd_resume,
	.shutdown   = xb_snd_shutdown,
};

static int __init xb_snd_init(void)
{
	return platform_driver_register(&xb_snd_driver);
}

static void __exit xb_snd_exit(void)
{
	platform_driver_unregister(&xb_snd_driver);
}

module_init(xb_snd_init);
module_exit(xb_snd_exit);
