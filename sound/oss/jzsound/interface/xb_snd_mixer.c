/**
 * xb_snd_mixer.c
 *
 * jbbi <jbbi@ingenic.cn>
 *
 * 24 APR 2012
 *
 */

#include "xb_snd_mixer.h"
#include "xb_snd_dsp.h"
#include <linux/soundcard.h>
/*###########################################################*\
 * interfacees
\*###########################################################*/
/********************************************************\
 * llseek
\********************************************************/
loff_t xb_snd_mixer_llseek(struct file *file,
						   loff_t offset,
						   int origin,
						   struct snd_dev_data *ddata)
{
	return 0;
}

/********************************************************\
 * read
\********************************************************/
ssize_t xb_snd_mixer_read(struct file *file,
						  char __user *buffer,
						  size_t count,
						  loff_t *ppos,
						  struct snd_dev_data *ddata)
{
	return -1;
}

/********************************************************\
 * write
\********************************************************/
static void print_format(unsigned long fmt)
{
	switch(fmt) {
		case AFMT_U8:
			printk("AFMT_U8.\n");
			break;
		case AFMT_S8:
			printk("AFMT_S8.\n");
			break;
		case AFMT_S16_LE:
			printk("AFMT_S16_LE.\n");
			break;
		case AFMT_S16_BE:
			printk("AFMT_S16_BE.\n");
			break;
		default :
			printk("unknown format.\n");
	}
}

ssize_t xb_snd_mixer_write(struct file *file,
						   const char __user *buffer,
						   size_t count,
						   loff_t *ppos,
						   struct snd_dev_data *ddata)
{
	char buf_byte;
	unsigned long fmt_in = 0;
	unsigned long fmt_out = 0;
	unsigned int channels_in = 0;
	unsigned int  channels_out = 0;
	unsigned long rate_in = 0;
	unsigned long rate_out = 0;
	if (copy_from_user((void *)&buf_byte, buffer, 1)) {
		printk("JZ MIX: copy_from_user failed !\n");
		return -EFAULT;
	}

	switch (buf_byte) {
		case '1' :
			printk(" \"1\" command :print codec and aic register.\n");
			ddata->dev_ioctl(SND_MIXER_DUMP_REG,0);
			break;
		case '2':
			printk(" \"2\" command :print audio hp and speaker gpio state.\n");
			ddata->dev_ioctl(SND_MIXER_DUMP_GPIO,0);
			break;
		case '3':
			printk(" \"3\" command :print current format channels and rate.\n");
			ddata->dev_ioctl(SND_DSP_GET_RECORD_FMT, (unsigned long)&fmt_in);
			ddata->dev_ioctl(SND_DSP_GET_REPLAY_FMT, (unsigned long)&fmt_out);
			printk("record format : ");
			print_format(fmt_in);
			printk("replay format : ");
			print_format(fmt_out);
			ddata->dev_ioctl(SND_DSP_GET_RECORD_CHANNELS,(unsigned long)&channels_in);
			ddata->dev_ioctl(SND_DSP_GET_REPLAY_CHANNELS,(unsigned long)&channels_out);
			printk("record channels : %d.\n", channels_in);
			printk("replay channels : %d.\n", channels_out);
			ddata->dev_ioctl(SND_DSP_GET_RECORD_RATE,(unsigned long)&rate_in);
			ddata->dev_ioctl(SND_DSP_GET_REPLAY_RATE,(unsigned long)&rate_out);
			printk("record samplerate : %ld.\n", rate_in);
			printk("replay samplerate : %ld.\n", rate_out);
			break;
		default:
			printk("undefine debug interface \"%c\".\n", buf_byte);
			printk(" \"1\" command :print codec and aic register.\n");
			printk(" \"2\" command :print audio hp and speaker gpio state.\n");
			printk(" \"3\" command :print current format channels and rate.\n");
	}
	return count;
}

/********************************************************\
 * poll
\********************************************************/
unsigned int xb_snd_mixer_poll(struct file *file,
							   poll_table *wait,
							   struct snd_dev_data *ddata)
{
	return 0;
}

/********************************************************\
 * ioctl
\********************************************************/
long xb_snd_mixer_ioctl(struct file *file,
						unsigned int cmd,
						unsigned long arg,
						struct snd_dev_data *ddata)
{
	switch (cmd) {
		//case SNDCTL_MIX_DESCRIPTION:
		/* OSS 4.x: get description text for a mixer control */
		//break;
		//case SNDCTL_MIX_ENUMINFO:
		/* OSS 4.x: get choice list for a MIXT_ENUM control */
		//break;
		//case SNDCTL_MIX_EXTINFO:
		/* OSS 4.x: get a mixer extension descriptor */
		//break;
		//case SNDCTL_MIX_NREXT:
		/* OSS 4.x: get number of mixer extension descriptor records */
		//break;
		//case SNDCTL_MIX_NRMIX:
		/* OSS 4.x: get number of mixer devices in the system */
		//break;
		//case SNDCTL_MIX_READ:
		/* OSS 4.x: read the current value of a mixer control */
		//break;
		//case SNDCTL_MIX_WRITE:
		/* OSS 4.x: change value of a mixer control */
		//break;
	}
	return -1;
}

/********************************************************\
 * mmap
\********************************************************/
int xb_snd_mixer_mmap(struct file *file,
					  struct vm_area_struct *vma,
					  struct snd_dev_data *ddata)
{
	return -1;
}

/********************************************************\
 * open
\********************************************************/
int xb_snd_mixer_open(struct inode *inode,
					  struct file *file,
					  struct snd_dev_data *ddata)
{
	return 0;
}

/********************************************************\
 * release
\********************************************************/
int xb_snd_mixer_release(struct inode *inode,
						 struct file *file,
						 struct snd_dev_data *ddata)
{
	return 0;
}

/********************************************************\
 * probe
\********************************************************/
int xb_snd_mixer_probe(struct snd_dev_data *ddata)
{
	return 0;
}
