#include <linux/fb.h>
#include <linux/earlysuspend.h>
#include "aosd.h"

#define NUM_FRAME_BUFFERS 2
#define PIXEL_ALIGN 16
#define DMA_DESC_NUM 2
#define CFG_DCACHE_SIZE  16384

struct jzfb_framedesc {
	uint32_t next_desc;
	uint32_t databuf;
	uint32_t frame_id;
	uint32_t cmd;
	uint32_t offsize;
	uint32_t page_width;
	uint32_t cmd_num;
	uint32_t desc_size;
} __packed;

enum format_order {
	FORMAT_X8R8G8B8  =  1,
	FORMAT_X8B8G8R8  =  2,
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
	enum format_order fmt_order;    /* pixel format order */
	int decompress;		      	/* enable decompress function, used by FG0 */
	int useIPUasInput;		 /* useIPUasInput, used by FG1 */

	unsigned int osd_cfg;	        /* OSDEN, ALHPAEN, F0EN, F1EN, etc */
	unsigned int osd_ctrl;	        /* IPUEN, OSDBPP, etc */
	unsigned int rgb_ctrl;	        /* RGB Dummy, RGB sequence, RGB to YUV */
	unsigned int colorkey0;	        /* foreground0's Colorkey enable, Colorkey value */
	unsigned int colorkey1;         /* foreground1's Colorkey enable, Colorkey value */
	unsigned int ipu_restart;       /* IPU Restart enable, ipu restart interval time */

	int line_length;                /* line_length, stride, in byte */

	struct jzfb_fg_t fg0;          /* fg0 info */
	struct jzfb_fg_t fg1;	        /* fg1 info */
};

struct jzfb {
	int id;           /* 0, lcdc0  1, lcdc1 */
	int is_enabled;   /* 0, disable  1, enable */
	int irq;          /* lcdc interrupt num */
	int open_cnt;

	struct fb_info *fb;
	struct platform_device *pdev;
	struct jzfb_platform_data *pdata;
	void __iomem *base;
	struct resource *mem;

	unsigned int frame_done;
	unsigned int frame_requested;
	unsigned int next_frameID;
	wait_queue_head_t frame_wq;
	spinlock_t update_lock;
	struct mutex lock;

	size_t vidmem_size;
	void *vidmem;
	unsigned int vidmem_phys;
	struct jzfb_framedesc *framedesc[2];		/* dma descriptor base address */


	struct jzfb_osd_t osd;				/* osd's config information */
	//	struct output_device_base cur_output_dev;	/* output mode's abstract struct */

	struct jzfb_aosd_info aosd;			/* compress data info */

	struct early_suspend early_suspend;

	struct clk *ldclk;
	struct clk *lpclk;
};

static inline unsigned long reg_read(struct jzfb *jzfb, int offset)
{
	return readl(jzfb->base + offset); 
}

static inline void reg_write(struct jzfb *jzfb, int offset, unsigned long val)
{
	writel(val, jzfb->base + offset); 
}

extern int tft_lcd_pan_display(struct fb_var_screeninfo *var, struct fb_info *info);
extern int prepare_dma_descriptor(struct jzfb *jzfb);

