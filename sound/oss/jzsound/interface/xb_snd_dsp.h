/**
 * xb_snd_dsp.h
 *
 * jbbi <jbbi@ingenic.cn>
 *
 * 24 APR 2012
 *
 */

#ifndef __XB_SND_DSP_H__
#define __XB_SND_DSP_H__

#include <linux/dmaengine.h>
#include <linux/wait.h>
#include <linux/scatterlist.h>
#include <linux/spinlock.h>
#include <linux/poll.h>
#include <mach/jzdma.h>
#include <mach/jzsnd.h>

/**
 * filter
 **/
int convert_8bits_signed2unsigned(void *buffer, int counter);
int convert_8bits_stereo2mono(void *buff, int data_len);
int convert_8bits_stereo2mono_signed2unsigned(void *buff, int data_len);
int convert_16bits_stereo2mono(void *buff, int data_len);
int convert_16bits_stereomix2mono(void *buff, int data_len);

/**
 * functions interface
 **/
loff_t xb_snd_dsp_llseek(struct file *file,
						 loff_t offset,
						 int origin,
						 struct snd_dev_data *ddata);

ssize_t xb_snd_dsp_read(struct file *file,
						char __user *buffer,
						size_t count,
						loff_t *ppos,
						struct snd_dev_data *ddata);

ssize_t xb_snd_dsp_write(struct file *file,
						 const char __user *buffer,
						 size_t count,
						 loff_t *ppos,
						 struct snd_dev_data *ddata);

unsigned int xb_snd_dsp_poll(struct file *file,
							   poll_table *wait,
							   struct snd_dev_data *ddata);

long xb_snd_dsp_ioctl(struct file *file,
					  unsigned int cmd,
					  unsigned long arg,
					  struct snd_dev_data *ddata);

int xb_snd_dsp_mmap(struct file *file,
					struct vm_area_struct *vma,
					struct snd_dev_data *ddata);

int xb_snd_dsp_open(struct inode *inode,
					struct file *file,
					struct snd_dev_data *ddata);

int xb_snd_dsp_release(struct inode *inode,
					   struct file *file,
					   struct snd_dev_data *ddata);

int xb_snd_dsp_probe(struct snd_dev_data *ddata);

#endif //__XB_SND_DSP_H__
