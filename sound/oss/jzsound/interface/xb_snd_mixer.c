/**
 * xb_snd_mixer.c
 *
 * jbbi <jbbi@ingenic.cn>
 *
 * 24 APR 2012
 *
 */

#include "xb_snd_mixer.h"

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
ssize_t xb_snd_mixer_write(struct file *file,
						   const char __user *buffer,
						   size_t count,
						   loff_t *ppos,
						   struct snd_dev_data *ddata)
{
	return -1;
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
	return -1;
}

/********************************************************\
 * release
\********************************************************/
int xb_snd_mixer_release(struct inode *inode,
						 struct file *file,
						 struct snd_dev_data *ddata)
{
	return -1;
}

/********************************************************\
 * probe
\********************************************************/
int xb_snd_mixer_probe(struct snd_dev_data *ddata)
{
	return -1;
}
