#ifndef __OVISP_ISP_H__
#define __OVISP_ISP_H__

#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <asm/irq.h>
#include <asm/io.h>
//#include <mach/mfp.h>
//#include <mach/i2c.h>
//#include <mach/gpio.h>
#include <mach/camera.h>
#include <mach/ovisp-v4l2.h>
#include <mach/regs-isp.h>

/* ISP notify types. */
#define ISP_NOTIFY_DATA_START			(0x00010000)
#define ISP_NOTIFY_DATA_START0			(0x00010001)
#define ISP_NOTIFY_DATA_START1			(0x00010002)
#define ISP_NOTIFY_DATA_DONE			(0x00020000)
#define ISP_NOTIFY_DATA_DONE0			(0x00020010)
#define ISP_NOTIFY_DATA_DONE1			(0x00020020)
#define ISP_NOTIFY_DROP_FRAME			(0x00040000)
#define ISP_NOTIFY_DROP_FRAME0			(0x00040100)
#define ISP_NOTIFY_DROP_FRAME1			(0x00040200)
#define ISP_NOTIFY_OVERFLOW			(0x00080000)

/*ISP group write config*/
#define I2C_CMD_READ		0x0001
#define I2C_CMD_ADDR_16BIT	0x0002
#define I2C_CMD_DATA_16BIT	0x0004


/* ISP clock number. */
#define ISP_CLK_NUM				(5)

struct isp_device;

struct isp_buffer {
	unsigned long addr;
};

struct isp_format {
	unsigned int width;
	unsigned int height;
	unsigned int dev_width;
	unsigned int dev_height;
	unsigned int code;
	unsigned int fourcc;
	struct v4l2_fmt_data *fmt_data;
};

struct isp_capture {
	int snapshot;
	struct isp_buffer buf;
	struct ovisp_camera_client *client;
};

struct isp_prop {
	int index;
	int bypass;
};

struct isp_parm {
	int contrast;
	int effects;
	int flicker;
	int brightness;
	int flash_mode;
	int focus_mode;
	int iso;
	int exposure;
	int auto_exposure;
	int gain;
	int saturation;
	int scene_mode;
	int sharpness;
	int white_balance;
	int auto_white_balance; // xhshen
	int zoom;
	int hflip;
	int vflip;
	int frame_rate;
	int in_width;
	int in_height;
	int in_format;
	int out_width;
	int out_height;
	int out_format;
	int crop_x;
	int crop_y;
	int crop_width;
	int crop_height;
	int ratio_d;
	int ratio_dcw;
	int ratio_up;
	int dowscaleFlag;
	int dcwFlag;
};

struct isp_ops {
	int (*init)(struct isp_device *, void *);
	int (*release)(struct isp_device *, void *);
	int (*open)(struct isp_device *, struct isp_prop *);
	int (*close)(struct isp_device *, struct isp_prop *);
	int (*config)(struct isp_device *, void *);
	int (*suspend)(struct isp_device *, void *);
	int (*resume)(struct isp_device *, void *);
	int (*mclk_on)(struct isp_device *, int);
	int (*mclk_off)(struct isp_device *, int);
	int (*offline_process)(struct isp_device *, void *, void*);
	int (*start_capture)(struct isp_device *, struct isp_capture *);
	int (*stop_capture)(struct isp_device *, void *);
	int (*enable_capture)(struct isp_device *, struct isp_buffer *);
	int (*disable_capture)(struct isp_device *, void *);
	int (*update_buffer)(struct isp_device *, struct isp_buffer *, int);
	int (*check_fmt)(struct isp_device *, struct isp_format *);
	int (*try_fmt)(struct isp_device *, struct isp_format *);
	int (*pre_fmt)(struct isp_device *, struct isp_format *);
	int (*s_fmt)(struct isp_device *, struct isp_format *);
	int (*s_ctrl)(struct isp_device *, struct v4l2_control *);
	int (*g_ctrl)(struct isp_device *, struct v4l2_control *);
	int (*s_parm)(struct isp_device *, struct v4l2_streamparm *);
	int (*g_parm)(struct isp_device *, struct v4l2_streamparm *);
	int (*tlb_init)(struct isp_device *);
	int (*tlb_deinit)(struct isp_device *);
	int (*tlb_map_one_vaddr)(struct isp_device *, unsigned int, unsigned int);
	int (*tlb_unmap_all_vaddr)(struct isp_device *);
};

struct isp_i2c_cmd {
	unsigned int flags;
	unsigned char addr;
	unsigned short reg;
	unsigned short data;
};

struct isp_i2c {
	struct mutex lock;
	struct i2c_adapter adap;
	struct ovisp_i2c_platform_data *pdata;
	int (*xfer_cmd)(struct isp_device *, struct isp_i2c_cmd *);
};

struct isp_debug{
	int settingfile_loaded;
	int status;
};

struct isp_tlb_vaddrmanager{
	struct list_head vaddr_entry;
	unsigned int vaddr;
	unsigned int size;
};

struct isp_tlb_pidmanager{
	struct list_head pid_entry;
	pid_t pid;
	unsigned int tlbbase;
	struct list_head vaddr_list;
};

struct isp_device {
	struct device *dev;
	struct resource *res;
	void __iomem *base;
	struct isp_ops *ops;
	struct isp_i2c i2c;
	struct isp_parm parm;
	struct completion completion;
	struct completion bracket_capture;
	int clk_enable[ISP_CLK_NUM];
	struct clk *clk[ISP_CLK_NUM];
	struct v4l2_fmt_data fmt_data;
	struct ovisp_camera_client *client;
	struct ovisp_camera_platform_data *pdata;
	int (*irq_notify)(unsigned int, void *);
	void *data;
	spinlock_t lock;
	unsigned char intr_l;
	unsigned char intr_h;
	unsigned char mac_intr_l;
	unsigned char mac_intr_h;
	int format_active;
	int snapshot;
	int running;
	int bypass;
	int poweron;

	/*csi power*/
	struct regulator * csi_power;
	int boot;
	int input;
	int irq;
	bool pp_buf;
	struct isp_buffer buf_start;
	int first_init;
	struct isp_debug debug;
	unsigned char bracket_end;
	int hdr_mode;
	int MaxExp;
	int hdr[8];
	int tlb_flag; // 0 disable; 1 enble
	struct list_head tlb_list;
};

#define isp_dev_call(isp, f, args...)				\
	(!(isp) ? -ENODEV : (((isp)->ops && (isp)->ops->f) ?	\
						 (isp)->ops->f((isp) , ##args) : -ENOIOCTLCMD))

static inline unsigned int isp_reg_readl(struct isp_device *isp,
		unsigned int offset)
{
#define pp 3
#if (pp == 0)
	return (((unsigned int)readb(isp->base + offset) << 24)
			| ((unsigned int)readb(isp->base + offset + 1) << 16)
			| ((unsigned int)readb(isp->base + offset + 2) << 8)
			| (unsigned int)readb(isp->base + offset + 3));
#elif (pp == 1)
	return (((unsigned int)readb(isp->base + offset))
			| ((unsigned int)readb(isp->base + offset + 1) << 8)
			| ((unsigned int)readb(isp->base + offset + 2) << 16)
			| (unsigned int)readb(isp->base + offset + 3) << 24);
#elif (pp == 3)
	return readl(isp->base + offset);
#endif
}

static inline unsigned short isp_reg_readw(struct isp_device *isp,
		unsigned int offset)
{
#if 0
	return (((unsigned short)readb(isp->base + offset) << 8)
			| (unsigned short)readb(isp->base + offset + 1));
#else
	return readw(isp->base + offset);
#endif
}

static inline unsigned char isp_reg_readb(struct isp_device *isp,
		unsigned int offset)
{
	return readb(isp->base + offset);
}

static inline void isp_reg_writel(struct isp_device *isp,
		unsigned int value, unsigned int offset)
{
#if 0
	writeb((value >> 24) & 0xff, isp->base + offset + 3);
	writeb((value >> 16) & 0xff, isp->base + offset + 2);
	writeb((value >> 8) & 0xff, isp->base + offset + 1);
	writeb(value & 0xff, isp->base + offset + 0);
#else
	writel(value, isp->base + offset);
#endif
}

static inline void isp_reg_writew(struct isp_device *isp,
		unsigned short value, unsigned int offset)
{
#if 0
	writeb((value >> 8) & 0xff, isp->base + offset  + 1);
	writeb(value & 0xff, isp->base + offset + 0);
#else
	writew(value, isp->base + offset);
#endif
}

static inline void isp_reg_writeb(struct isp_device *isp,
		unsigned char value, unsigned int offset)
{
	writeb(value, isp->base + offset);
}

static void isp_firmware_writeb(struct isp_device * isp,
		unsigned char value, unsigned int offset)
{
	int pos = offset % 4;
	isp_reg_writeb(isp, value, offset - pos + (3 - pos));
}
static unsigned char isp_firmware_readb(struct isp_device * isp,
		unsigned int offset)
{
	int pos = offset % 4;
	return isp_reg_readb(isp, offset - pos + (3 -  pos));
}
extern int isp_device_init(struct isp_device* isp);
extern int isp_device_release(struct isp_device* isp);
extern unsigned char isp_firmware_readb(struct isp_device * isp,unsigned int offset);


#endif/*__OVISP_ISP_H__*/
