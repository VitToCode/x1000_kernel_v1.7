#include<stdio.h>
#include<linux/fb.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>


#include "../../../../../../hardware/xb4780/libjzipu/android_jz_ipu.h"
#include "../../../../../hardware/xb4780/libdmmu/dmmu.h"


#define SOURCE_BUFFER_SIZE 0x200000 /* 2M */
#define START_ADDR_ALIGN 0x1000 /* 4096 byte */
#define STRIDE_ALIGN 0x800 /* 2048 byte */
#define PIXEL_ALIGN 16 /* 16 pixel */

#define MODE_NAME_LEN 32

//#define FRAME_SIZE  70 /* 70 frame of y and u/v video data */
#define FRAME_SIZE  30 /* 70 frame of y and u/v video data */ /* test */

#define KEY_COLOR_RED     0x00
#define KEY_COLOR_GREEN   0x04
#define KEY_COLOR_BLUE    0x00

/* LCDC ioctl commands */
#define JZFB_GET_MODENUM		_IOR('F', 0x100, int)
#define JZFB_GET_MODELIST		_IOR('F', 0x101, char *)
#define JZFB_SET_VIDMEM			_IOW('F', 0x102, unsigned int *)
#define JZFB_SET_MODE			_IOW('F', 0x103, char *)
#define JZFB_ENABLE			_IOW('F', 0x104, int)

#define JZFB_SET_ALPHA			_IOW('F', 0x123, struct jzfb_fg_alpha)
#define JZFB_SET_COLORKEY		_IOW('F', 0x125, struct jzfb_color_key)
#define JZFB_SET_BACKGROUND		_IOW('F', 0x124, struct jzfb_bg)

#define JZFB_ENABLE_FG0			_IOW('F', 0x139, int)

/* hdmi ioctl commands */
#define HDMI_POWER_OFF			_IO('F', 0x301)
#define	HDMI_VIDEOMODE_CHANGE		_IOW('F', 0x302, char *)
#define	HDMI_POWER_ON			_IO('F', 0x303)

static unsigned int tlb_base_phys;

struct source_info {
	unsigned int width;
	unsigned int height;
	void *y_buf_v;
	void *u_buf_v;
	void *v_buf_v;
};

struct dest_info {
	unsigned int width;
	unsigned int height;
	unsigned long size;
	int y_stride;
	int u_stride;
	int v_stride;
	void *out_buf_v;
};

struct jzfb_fg_alpha {
	__u32 fg; /* 0:fg0, 1:fg1 */
	__u32 enable;
	__u32 mode; /* 0:global alpha, 1:pixel alpha */
	__u32 value; /* 0x00-0xFF */
};
struct jzfb_color_key {
	__u32 fg; /* 0:fg0, 1:fg1 */
	__u32 enable;
	__u32 mode; /* 0:color key, 1:mask color key */
	__u32 red;
	__u32 green;
	__u32 blue;
};

struct jzfb_bg {
	__u32 fg; /* 0:fg0, 1:fg1 */
	__u32 red;
	__u32 green;
	__u32 blue;
};

static int line_pixel = 10;
static void jzfb_display_h_color_bar(struct source_info *source_info)
{
	int i,j;
	int w, h;
	int bpp;
	unsigned int *p32;

	p32 = (unsigned int *)source_info->y_buf_v;

	w = source_info->width;
	h = source_info->height;
	bpp = 4;

	printf("LCD H COLOR BAR w,h,bpp(%d,%d,%d)\n", w, h, bpp);

	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++) {
			int c32;
			switch ((i / line_pixel) % 4) {
			case 0:
				c32 = 0x00FF0000;
				break;
			case 1:
				c32 = 0x0000FF00;
				break;
			case 2:
				c32 = 0x000000FF;
				break;
			default:
				c32 = 0xFFFFFFFF;
				break;
			}

			*p32++ = c32;
		}
		if (w % PIXEL_ALIGN) {
			p32 += ((source_info->width + PIXEL_ALIGN -1) & ~(PIXEL_ALIGN -1) - w);
		}
	}
	line_pixel--;
	if (line_pixel <= 0) {
		line_pixel = 10;
	}
}

int initIPUSourceBuffer(struct ipu_image_info * ipu_img, struct source_info *source_info)
{
	int ret = 0;
	struct source_data_info *srcInfo;
	struct ipu_data_buffer *srcBuf;


	srcInfo = &ipu_img->src_info;
	srcBuf = &ipu_img->src_info.srcBuf;
	memset(srcInfo, 0, sizeof(struct source_data_info));

	srcInfo->fmt = HAL_PIXEL_FORMAT_JZ_YUV_420_B;
//	srcInfo->fmt = HAL_PIXEL_FORMAT_BGRX_8888;
	srcInfo->width = source_info->width;
	srcInfo->height = source_info->height;
	srcInfo->is_virt_buf = 1;
	srcInfo->stlb_base = tlb_base_phys;

	srcBuf->y_buf_v = source_info->y_buf_v;
	srcBuf->u_buf_v = source_info->u_buf_v;
	srcBuf->v_buf_v = source_info->v_buf_v;
//	srcBuf->u_buf_v = 0;
//	srcBuf->v_buf_v = 0;

//	srcBuf->y_stride = ((source_info->width + (PIXEL_ALIGN - 1)) &
//			     (~(PIXEL_ALIGN - 1))) * 4;
//	srcBuf->u_stride = 0;
//	srcBuf->v_stride = 0;

	srcBuf->y_stride = (source_info->width * 16 +
			    (STRIDE_ALIGN - 1)) & (~(STRIDE_ALIGN-1));
	srcBuf->u_stride = (source_info->width * 16 / 2 +
			    (STRIDE_ALIGN - 1)) & (~(STRIDE_ALIGN-1));
	srcBuf->v_stride = (source_info->width * 16 / 2 +
			    (STRIDE_ALIGN - 1)) & (~(STRIDE_ALIGN-1));

	return 0;
}

int initIPUDestBuffer(struct ipu_image_info * ipu_img, struct dest_info *dst_info)
{
	int ret = 0;
	struct dest_data_info *dstInfo;
	struct ipu_data_buffer *dstBuf;


	dstInfo = &ipu_img->dst_info;
	dstBuf = &ipu_img->dst_info.dstBuf;
	memset(dstInfo, 0, sizeof(struct dest_data_info));


	dstInfo->dst_mode = IPU_OUTPUT_TO_LCD_FG1;
//	dstInfo->fmt = HAL_PIXEL_FORMAT_RGBX_8888;
	dstInfo->fmt = HAL_PIXEL_FORMAT_BGRX_8888;
	dstInfo->left = 0;
	dstInfo->top = 0;
//	dstInfo->left = 100;
//	dstInfo->top = 80;
	dstInfo->lcdc_id = 0;

	dstInfo->width = dst_info->width;
	dstInfo->height = dst_info->height;

	dstBuf->y_stride = dst_info->y_stride;
	printf("%s, %d\n", __func__, __LINE__);

	return 0;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int i;
	int loop;
	int frameCount = 0;
//	int fb_fd1;
	struct ipu_image_info * ipu_image_info;
	struct fb_var_screeninfo *var_info;
	void *source_buffer;
	struct source_info source_info;
	struct dest_info dst_info;
	void *dest_buffer;
	int ipu_out_width = 320;
	int ipu_out_height = 240;

	struct jzfb_fg_alpha fg0_alpha;
	struct jzfb_color_key color_key;
	struct jzfb_bg fg1_background;

	int fb_fd1;
	int mode_num;
	char (*ml)[32];
	int hdmi_fd = -1;
	int tmp_mode;
	int enable;

	if (argc == 3) {
		ipu_out_width = atoi(argv[1]);
		ipu_out_height = atoi(argv[2]);
	}

	void *temp_buffer;
	int mIPU_inited = 0;

	struct dmmu_mem_info dmmu_tlb;

	fb_fd1 = open("/dev/graphics/fb0", O_RDWR);
	if (fb_fd1 < 0) {
		printf("open fb 0 fail\n");
		return -1;
	}
	var_info = malloc(sizeof(struct fb_var_screeninfo));
	if (!var_info) {
		printf("alloc framebuffer var screen info fail\n");
		return -1;
	}
	memset(var_info, 0, sizeof(struct fb_var_screeninfo));
	if (ioctl(fb_fd1, FBIOGET_VSCREENINFO, var_info) < 0) {
		printf("get framebuffer var screen info fail\n");
		return -1;
	}

	/* get hdmi modes number */
	if (ioctl(fb_fd1, JZFB_GET_MODENUM, &mode_num) < 0) {
		printf("JZFB_GET_MODENUM faild\n");
		return -1;
	}
	printf("API FB mode number is %d\n", mode_num);
	ml = (char (*)[MODE_NAME_LEN])malloc(sizeof(char)* MODE_NAME_LEN * mode_num);
	if (ioctl(fb_fd1, JZFB_GET_MODELIST, ml) < 0) {
		printf("JZFB_GET_MODELIST faild\n");
		return -1;
	}

	for(i=0;i<mode_num;i++)
		printf("List[%d]: %s\n",i, ml[i]);

#if 0
	tmp_mode = 0;
	printf("please enter the hdmi video mode number\n");
	scanf("%d", &tmp_mode);

	if (ioctl(fb_fd1, JZFB_SET_MODE, ml[tmp_mode]) < 0) {
		printf("JZFB set hdmi mode faild\n");
		return -1;
	}
	printf("API set mode name = %s success\n", ml[tmp_mode]);
#endif

#if 1

	fg0_alpha.fg = 0;
	fg0_alpha.enable = 1;
	fg0_alpha.mode = 0; /* set the global alpha of FG 0 */
	fg0_alpha.value = 0xa0;
	if (ioctl(fb_fd1, JZFB_SET_ALPHA, &fg0_alpha) < 0) {
		printf("set fg 0 global alpha fail\n");
		return -1;
	}

	color_key.fg = 0;
	color_key.enable = 1; /* enable the color key of FG 0 */
	color_key.mode = 0;
	color_key.red = KEY_COLOR_RED;
	color_key.green = KEY_COLOR_GREEN;
	color_key.blue = KEY_COLOR_BLUE;
	if (ioctl(fb_fd1, JZFB_SET_COLORKEY, &color_key) < 0) {
		printf("set fg 0 color key fail\n");
		return -1;
	}
#endif
	/* disble LCD controller FG 0 */
	enable = 1;
	if (ioctl(fb_fd1, JZFB_ENABLE_FG0, &enable) < 0) {
		printf("disable LCDC 0 FG 0 faild\n");
		return -1;
	}

	source_buffer = (void *)malloc(SOURCE_BUFFER_SIZE);
	if (!source_buffer) {
		printf("alloc source_buffer fail\n");
		return -1;
	}
	printf("source_buffer = %p\n", source_buffer);
	memset(source_buffer, 0, SOURCE_BUFFER_SIZE);

	source_info.y_buf_v = (void *)(((unsigned int)source_buffer +
					(START_ADDR_ALIGN - 1)) &
				       (~(START_ADDR_ALIGN -1)));
	source_info.u_buf_v = (void *)((unsigned int)source_info.y_buf_v + 0x100000);
	source_info.v_buf_v = (void *)((unsigned int)source_info.u_buf_v);
	source_info.width = 320;
	source_info.height = 240;

	dst_info.width = ipu_out_width;
	dst_info.height = ipu_out_height;

	dst_info.size = ((ipu_out_width + (PIXEL_ALIGN - 1)) &
		(~(PIXEL_ALIGN - 1))) * ipu_out_height
		* (var_info->bits_per_pixel >> 3);

	dst_info.y_stride = ((var_info->xres + (PIXEL_ALIGN - 1)) &
			     (~(PIXEL_ALIGN - 1))) * var_info->bits_per_pixel >> 3;
	dst_info.u_stride = 0;
	dst_info.v_stride = 0;

#if 1
	dst_info.out_buf_v = mmap(0, dst_info.size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd1, 0);
	if (!dst_info.out_buf_v) {
		printf("mmap framebuffer fail\n");
		return -1;
	}
#endif

	temp_buffer = (void *)malloc(SOURCE_BUFFER_SIZE);
	if (!temp_buffer) {
		printf("alloc temp buffer fail\n");
		return -1;
	}
	memset(source_buffer, 0, SOURCE_BUFFER_SIZE);
	printf("main() %d\n", __LINE__);

	ret = dmmu_init();
	if (ret < 0) {
		printf("dmmu_init failed\n");
		return -1;
	}
	ret = dmmu_get_page_table_base_phys(&tlb_base_phys);
	if (ret < 0) {
		printf("dmmu_get_page_table_base_phys failed!\n");
		return -1;
	}

	printf("main() %d\n", __LINE__);
	memset(&dmmu_tlb, 0, sizeof(struct dmmu_mem_info));

	dmmu_tlb.vaddr = source_buffer;
	dmmu_tlb.size = SOURCE_BUFFER_SIZE; /* test */

	ret = dmmu_map_user_memory(&dmmu_tlb);
	if (ret < 0) {
		printf("src dmmu_map_user_memory failed!\n");
		return -1;
	}

	printf("main() %d\n", __LINE__);
	loop = FRAME_SIZE;

	ipu_image_info = NULL;
	if (ipu_open(&ipu_image_info) < 0) {
		printf("open ipu fail\n");
	}

	initIPUDestBuffer(ipu_image_info, &dst_info);
	printf("main() %d\n", __LINE__);
	while (loop--) {
#if 1
		char filename1[20] = {0};
		sprintf(filename1, "/data/yuv/y%d.raw", frameCount);
		FILE* sfd1 = fopen(filename1, "rb");
		if(sfd1){
			printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!fread\n");
			fread(temp_buffer, 320 * 240, 1, sfd1);
			fclose(sfd1);
			sfd1 = NULL;
		}else
			printf("fail to open nals.txt\n");

		printf("re-align Y Buffer\n");
		void * dst;
		void * src;
		int hhh;
		dst = source_info.y_buf_v;
		src = temp_buffer;
		int dstStride = 320 * 16;
		int srcStride = 320 * 16;

		dstStride = (dstStride + (STRIDE_ALIGN - 1))&(~(STRIDE_ALIGN-1));
		printf("dstStride=%d, srcStride=%d\n", dstStride, srcStride);
		for (hhh=0; hhh< (240/16); hhh++) {
			memcpy(dst, src, 320*16);
			dst = (void *)((unsigned int)dst + dstStride);
			src = (void *)((unsigned int)src + srcStride);
		}

		char filename2[20] = {0};
		sprintf(filename2, "/data/yuv/u%d.raw", ++frameCount); /* uv file from u1.raw */
		FILE* sfd2 = fopen(filename2, "rb");
		if (sfd2) {
			printf("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!fread");
			fread(temp_buffer, 320 * 240 / 2, 1, sfd2);
			fclose(sfd2);
			sfd2 = NULL;
		} else {
			printf("fail to open nals.txt\n");
		}

		printf("re-align UV Buffer\n" );
		dst = source_info.u_buf_v;
		src = temp_buffer;
		dstStride = 320 * 16 / 2;
		srcStride = 320 * 16 / 2;

		dstStride = (dstStride + (STRIDE_ALIGN - 1))&(~(STRIDE_ALIGN-1));

		for (hhh = 0; hhh < (240/16); hhh++) {
			memcpy(dst, src, 320*16/2);
			dst = (void *)((unsigned int)dst + dstStride);
			src = (void *)((unsigned int)src + srcStride);
		}
#endif
//		jzfb_display_h_color_bar(&source_info);

		initIPUSourceBuffer(ipu_image_info, &source_info);

		if (mIPU_inited == 0) {
			int enable;
			if ((ret = ipu_init(ipu_image_info)) < 0) {
				printf("ipu_init() failed mIPUHandler=%p\n", ipu_image_info);
				return -1;
			} else {
				printf("%s, %d\n", __func__, __LINE__);
				mIPU_inited = 1;
				printf("mIPU_inited == true\n");
			}

			ipu_postBuffer(ipu_image_info);

			/* enable LCD controller */
			enable = 1;
			if (ioctl(fb_fd1, JZFB_ENABLE, &enable) < 0) {
				printf(" JZFB_ENABLE faild\n");
				return -1;
			}
#if 0
			hdmi_fd = open("/dev/hdmi", O_RDWR);
			printf("open hdmi device hdmi_fd = %d\n", hdmi_fd);
			if (hdmi_fd < 0)
				printf("open hdmi device fail\n");

			if (ioctl(hdmi_fd, HDMI_VIDEOMODE_CHANGE, ml[tmp_mode]) < 0) {
				printf("HDMI_VIDEOMODE_CHANGE faild\n");
				return -1;
			}
#endif
		}
		ipu_postBuffer(ipu_image_info);
		printf("ipu_postBuffer success\n");

		android_memset32(dst_info.out_buf_v, KEY_COLOR_RED | KEY_COLOR_GREEN
		       | KEY_COLOR_BLUE, dst_info.size);
//		android_memset32(dst_info.out_buf_v, 0x80808080, dst_info.size);
//		memset(dst_info.out_buf_v, KEY_COLOR_RED | KEY_COLOR_GREEN
//		       | KEY_COLOR_BLUE, dst_info.size);
		printf("memset ok\n");

		var_info->yoffset = 0;
		if (ioctl(fb_fd1, FBIOPAN_DISPLAY, var_info) < 0)
			printf("pan display fail\n");

		sleep(1);
//		frameCount++;
		printf("while() loop = %d\n", loop);
	}
	ret = ipu_close(&ipu_image_info);
	if (ret < 0) {
		printf("IPU close failed\n");
		return -1;
	}

	fg0_alpha.fg = 0;
	fg0_alpha.enable = 1;
	fg0_alpha.mode = 0; /* recover the original global alpha value */
	fg0_alpha.value = 0xff;
	if (ioctl(fb_fd1, JZFB_SET_ALPHA, &fg0_alpha) < 0) {
		printf("set FG 0 original global alpha fail\n");
		return -1;
	}
	color_key.fg = 0;
	color_key.enable = 0; /* disable the color key of FG 0 */
	if (ioctl(fb_fd1, JZFB_SET_COLORKEY, &color_key) < 0) {
		printf("disable fg 0 color key fail\n");
		return -1;
	}

	enable = 1;
	if (ioctl(fb_fd1, JZFB_ENABLE, &enable) < 0) {
		printf(" JZFB_ENABLE faild\n");
		return -1;
	}

	close(fb_fd1);

	close(hdmi_fd);

	return 0;
}
