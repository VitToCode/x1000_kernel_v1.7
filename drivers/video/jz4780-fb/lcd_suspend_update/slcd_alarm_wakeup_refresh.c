#ifdef CONFIG_SLCD_SUSPEND_ALARM_WAKEUP_REFRESH
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <linux/fs.h>
#include <linux/sysctl.h>
#include <linux/delay.h>
#include <asm/cacheops.h>
#include <asm/rjzcache.h>
#include <asm/fpu.h>
#include <linux/syscore_ops.h>
#include <linux/regulator/consumer.h>
#include <linux/vmalloc.h>
#include <linux/fb.h>

#include <soc/cache.h>
#include <soc/base.h>
#include <soc/cpm.h>
#include <soc/irq.h>
#include <tcsm.h>

#include "jz4780_fb.h"
#include "slcd_suspend_debug.h"
#include "slcd_alarm_wakeup_refresh.h"



/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */
struct fb_var_screeninfo fb_var;

//extern struct jzfb *jzfb0;
extern struct fb_info *suspend_fb;
extern void *suspend_base;


static int buffer_count = 6;
//static int next_buffer_count_buffer_count = 6;
//static int buffer_size = (800*480*4 * 3); /* lcd frame_buffer size */
static struct clock_buffers *clock_buffers=NULL;


/* open bitmap file */
static int read_bitmap_file(char * filename, char * data, int buffer_size) {
	int fd, err = 0;
	unsigned count;

	printk_dbg("filename=%s\n", filename);
	fd = sys_open(filename, O_RDONLY, 0);
	if (fd < 0) {
		printk(KERN_WARNING "%s: Can not open %s\n",
		       __func__, filename);
		return -ENOENT;
	}
	count = (unsigned)sys_lseek(fd, (off_t)0, 2);
	printk_dbg("count=%d\n", count);
	if (count == 0) {
		sys_close(fd);
		err = -EIO;
		goto err_logo_close_file;
	}

	printk_dbg("count=%d\n", count);
	if ( count > buffer_size )
		count = buffer_size;

	sys_lseek(fd, (off_t)0, 0);

	if (!data) {
		printk(KERN_WARNING "%s:Can not alloc data\n", __func__);
		err = -ENOMEM;
		goto err_logo_close_file;
	}

	if ((unsigned)sys_read(fd, (char *)data, count) != count) {
		err = -EIO;
	}


err_logo_close_file:
	sys_close(fd);

	return err;
}


int update_clock(void)
{
	printk_dbg("%s() ENTER\n", __FUNCTION__);


	printk_dbg("clock_buffers->next_buffer_count=%d\n", clock_buffers->next_buffer_count);


	/* copy next_buffer_count_buffer to lcd frame buffer */
	{
		struct clock_bitmap_buffer *bitmap;
		bitmap = clock_buffers->bitmap_buffers + clock_buffers->next_buffer_count;
		if ( bitmap->valid ) {
			void * fb;
			int size;
			int xres, yres;
			struct jzfb *jzfb = suspend_fb->par;
			struct fb_var_screeninfo *var = &suspend_fb->var;
			xres = var->xres;
			yres = var->yres;
			size = jzfb->vidmem_size;

			fb = jzfb->vidmem;
			//fb = suspend_base;

			printk_dbg("memcpy((void*)fb=%p, bitmap->buffer=%p, size=%d);, xres=%d, yres=%d\n",
				   (void*)fb, (void*)bitmap->buffer, (int)size, (int)xres, (int)yres);

			memcpy((void*)fb, bitmap->buffer, size);


		}
	}

	/* update_slcd_frame_buffer */
	//update_slcd_frame_buffer();

	clock_buffers->next_buffer_count++;
	if ( clock_buffers->next_buffer_count > clock_buffers->buffer_count-1)
		clock_buffers->next_buffer_count = 0;


	return 0;
}


static int alloc_bitmap_buffers(void)
{
	int cnt;
	struct clock_bitmap_buffer *bitmap_buffer;
	if ( clock_buffers == NULL ) {
		struct jzfb *jzfb = suspend_fb->par;
		clock_buffers = (struct clock_buffers *) kmalloc(sizeof(struct clock_buffers), GFP_KERNEL);
		if ( !clock_buffers ) {
			printk_dbg("alloc clock_buffers failed\n");
			return -1;
		}

		clock_buffers->next_buffer_count = 0;
		clock_buffers->buffer_count = buffer_count;
		clock_buffers->buffer_size = jzfb->vidmem_size;//buffer_size;
		clock_buffers->bitmap_buffers = (struct clock_bitmap_buffer*)kmalloc(sizeof(struct clock_bitmap_buffer)*buffer_count, GFP_KERNEL);
		bitmap_buffer = clock_buffers->bitmap_buffers;
		for (cnt=0; cnt<clock_buffers->buffer_count; cnt++) {
			bitmap_buffer->valid=0;
			bitmap_buffer ++;
		}
	}

	bitmap_buffer = clock_buffers->bitmap_buffers;
	for (cnt=0; cnt<clock_buffers->buffer_count; cnt++) {
		bitmap_buffer->buffer = (char * )vmalloc(clock_buffers->buffer_size);
		printk_dbg("alloc bitmap_buffer, cnt=%d, buffer_size=%d, ->buffer=%p\n", cnt, clock_buffers->buffer_size, bitmap_buffer->buffer);
		if (!bitmap_buffer->buffer) {
			break;
		}
		bitmap_buffer ++;
	}

	if ( cnt<clock_buffers->buffer_count ) {
		printk_info("alloc bitmap_buffer->buffer failed, cnt=%d, buffer_size=%d\n", cnt, clock_buffers->buffer_size);
		return -ENOMEM;
	}


	return 0;
}


static int free_bitmap_buffers(void)
{
	int cnt;
	struct clock_bitmap_buffer *bitmap_buffer;

	if ( !clock_buffers ) {
		printk_dbg("clock_buffers = NULL\n");
		return -1;
	}

	bitmap_buffer = clock_buffers->bitmap_buffers;
	for (cnt=0; cnt<clock_buffers->buffer_count; cnt++) {
		vfree((void*)bitmap_buffer->buffer);
		bitmap_buffer->buffer = NULL;
		bitmap_buffer ++;
	}



	return 0;
}

char *bitmap_filename[] = {
	"/sdcard/fb0.dat",
	"/sdcard/fb1.dat",
};


static int load_bitmaps(void)
{
	int cnt;
	struct clock_bitmap_buffer *bitmap;

	if ( !clock_buffers ) {
		printk_dbg("clock_buffers = NULL\n");
		return -1;
	}

	bitmap = clock_buffers->bitmap_buffers;
	for (cnt=0; cnt<clock_buffers->buffer_count; cnt++) {
		char * filename;
		printk_dbg("load_bitmaps(), cnt=%d, buffer_size=%d\n", cnt, clock_buffers->buffer_size);
		filename = bitmap_filename[cnt&1];
		if ( read_bitmap_file(filename, bitmap->buffer, clock_buffers->buffer_size) ) {
			printk_dbg("read_bitmap_file(%s) failed\n", filename);
			bitmap->valid = 0;
		}
		else {
			bitmap->valid = 1;
		}

		printk_dbg("load_bitmaps() bitmap->valid=%d\n", bitmap->valid);

		bitmap++;
	}

	return 0;
}


/* -------------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------------- */




int slcd_refresh_prepare(void)
{
	int ret;
	printk_dbg("%s() ENTER\n", __FUNCTION__);

	/* alloc bitmap buffers */
	printk_dbg("%s() alloc bitmap buffers.\n", __FUNCTION__);
	ret = alloc_bitmap_buffers();

	/* load bitmaps */
	printk_dbg("%s() load bitmaps.\n", __FUNCTION__);
	load_bitmaps();


	return 0;
}


int slcd_refresh_finish(void)
{
	printk_dbg("%s() ENTER\n", __FUNCTION__);


	/* release  bitmaps */
	printk_dbg("%s() release bitmaps.\n", __FUNCTION__);


	/* free bitmap buffers */
	printk_dbg("%s() free bitmap buffers.\n", __FUNCTION__);
	free_bitmap_buffers();

	return 0;
}


int is_configed_slcd_rtc_alarm_refresh(void)
{

	return 1;
}

#endif	/* CONFIG_SLCD_SUSPEND_ALARM_WAKEUP_REFRESH */
