
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>

#include <mach/jz_cim.h>

struct fake_sensor {
	struct cim_sensor cs;
	char name[8];

	unsigned int width;
	unsigned int height;
};

unsigned char yuv422i_color_odd[3][4] = {{0xd6, 0x97, 0xca,0x97}, {0x5f, 0x25, 0x6e, 0x25}, {0x73, 0x8e, 0x37, 0x8e}};
unsigned char yuv422i_color_even[3][4] = {{0x5f, 0x25, 0x6e, 0x25}, {0x73, 0x8e, 0x37, 0x8e}, {0xd6, 0x97, 0xca, 0x97}};

int fake_probe(void *data){return 0;}
int fake_init(void *data){return 0;}
int fake_power_on(void *data){return 0;}
int fake_shutdown(void *data){return 0;}
int fake_af_init(void *data){return 0;}
int fake_start_af(void *data){return 0;}
int fake_stop_af(void *data){return 0;}
int fake_set_preivew_mode(void *data){return 0;}
int fake_set_capture_mode(void *data){return 0;}
int fake_set_parameter(void *data,int mode, int arg){return 0;}

int fake_set_resolution(void *data,int width,int height)
{
	struct fake_sensor *fake = data;
	fake->width = width;
	fake->height = height;
	return	0;
}

int fake_fill_buffer(void *data,char *buf)
{
	static int i = 0;
	struct fake_sensor *fake = data;
	memset(buf,255 * (i++ % 2),fake->width * fake->height * 3);
	return 0;
}

struct frm_size cfrm_size[] = {
	{2048,1532},
	{1024,768},
};

struct frm_size pfrm_size[] = {
	{800,600},
	{640,480},
};

int fake_register(void)
{
	unsigned int i;
	struct fake_sensor *s;
	for ( i = 0; i < CONFIG_FAKE_NR; i++) {
		s = kzalloc(sizeof(struct fake_sensor), GFP_KERNEL);
		sprintf(s->name,"fake%d",i);

		s->cs.id 		= i;
		s->cs.name 		= s->name;
		s->cs.probe		= fake_probe;
		s->cs.init		= fake_init;
		s->cs.power_on		= fake_power_on;
		s->cs.shutdown		= fake_shutdown;
		s->cs.af_init		= fake_af_init;
		s->cs.start_af		= fake_start_af;
		s->cs.stop_af		= fake_stop_af;
		s->cs.set_preivew_mode	= fake_set_preivew_mode;
		s->cs.set_capture_mode	= fake_set_capture_mode;
		s->cs.set_resolution	= fake_set_resolution;
		s->cs.set_parameter	= fake_set_parameter;
		s->cs.fill_buffer	= fake_fill_buffer;

		s->cs.preview_size 	= pfrm_size;
		s->cs.capture_size 	= cfrm_size;

		s->cs.private 		= (void *)s;

		camera_sensor_register(&s->cs);
	}

	return 0;
}

module_init(fake_register);
