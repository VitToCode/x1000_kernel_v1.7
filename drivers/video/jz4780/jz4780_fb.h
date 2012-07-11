#include <linux/fb.h>
#include <linux/earlysuspend.h>
#include "aosd.h"

#define NUM_FRAME_BUFFERS 2
#define PIXEL_ALIGN 16
#define MODE_NAME_LEN 32

struct jzfb_framedesc {
	uint32_t next;
	uint32_t databuf;
	uint32_t id;
	uint32_t cmd;
	uint32_t offsize;
	uint32_t page_width;
	uint32_t cmd_num;
	uint32_t desc_size;
} __packed;

enum jzfb_format_order {
	FORMAT_X8R8G8B8 = 1,
	FORMAT_X8B8G8R8,
};

struct jzfb_fg_t {
	int fg;     /* 0, fg0  1, fg1 */
	int bpp;	/* foreground bpp */
	int x;		/* foreground start position x */
	int y;		/* foreground start position y */
	int w;		/* foreground width */
	int h;		/* foreground height */
	unsigned int alpha;     /* ALPHAEN, alpha value */
	unsigned int bgcolor;   /* background color value */
};

struct jzfb_osd_t {
	int decompress;		      	/* enable decompress function, used by FG0 */
	int block;	  	        /* enable 16x16 block function */

	unsigned int colorkey0;	        /* foreground0's Colorkey enable, Colorkey value */
	unsigned int colorkey1;         /* foreground1's Colorkey enable, Colorkey value */

	struct jzfb_fg_t fg0;          /* fg0 info */
	struct jzfb_fg_t fg1;	        /* fg1 info */
};

struct jzfb {
	int id;           /* 0, lcdc0  1, lcdc1 */
	int is_enabled;   /* 0, disable  1, enable */
	int irq;          /* lcdc interrupt num */
	int open_cnt;
	int desc_num;
	char clk_name[16];
	char pclk_name[16];

	struct fb_info *fb;
	struct platform_device *pdev;
	struct jzfb_platform_data *pdata;
	void __iomem *base;
	struct resource *mem;

	wait_queue_head_t frame_wq;

	size_t vidmem_size;
	void *vidmem;
	dma_addr_t vidmem_phys;

	int frm_size;
	volatile int frm_id;
	struct jzfb_framedesc *framedesc; /* dma 0 descriptor base address */
	struct jzfb_framedesc *fg1_framedesc; /* FG 1 dma descriptor */
	dma_addr_t framedesc_phys;

	enum jzfb_format_order fmt_order; /* frame buffer pixel format order */
	struct jzfb_osd_t osd; /* osd's config information */
	struct jzfb_aosd_info aosd; /* compress data info */

	struct clk *ldclk;
	struct clk *lpclk;

	struct early_suspend early_suspend;

	struct mutex lock;
};

static inline unsigned long reg_read(struct jzfb *jzfb, int offset)
{
	return readl(jzfb->base + offset); 
}

static inline void reg_write(struct jzfb *jzfb, int offset, unsigned long val)
{
	writel(val, jzfb->base + offset); 
}

static void dump_lcdc_registers(struct jzfb *jzfb);
static void jzfb_enable(struct fb_info *info);
static int jzfb_set_par(struct fb_info *info);

/* ioctl commands */
#define JZFB_GET_MODENUM		_IOR('F', 0x100, int)
#define JZFB_GET_MODELIST		_IOR('F', 0x101, char *)
#define JZFB_SET_MODE			_IOW('F', 0x102, char *)
#define JZFB_SET_VIDMEM			_IOW('F', 0x103, unsigned int *)
#define JZFB_ENABLE			_IOW('F', 0x104, unsigned int *)
#define JZFB_DISABLE			_IOW('F', 0x105, unsigned int *)
