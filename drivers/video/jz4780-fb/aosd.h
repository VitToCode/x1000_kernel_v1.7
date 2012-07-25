#ifndef __AOSD_H__
#define __AOSD_H__

struct jzfb_aosd_info {
	unsigned long addr0;
	unsigned long addr1;
	unsigned long addr2;
	unsigned long addr3;
	unsigned long waddr;

	unsigned long smem_start;
	void __iomem *base;
	unsigned int  addr_len;

	unsigned int  alpha_value;
	unsigned int  frmlv;
	unsigned int  order;
	unsigned int  format_mode;
	unsigned int  alpha_mode;
	unsigned int  aligned_64;
	unsigned int  height;
	unsigned int  width;
	unsigned int  without_alpha;
	unsigned int  compress_done;
	unsigned int  bpp;
	unsigned int  buf;
	unsigned char *buf_comp[2]; /* buffer for compress output */
	unsigned int src_stride; /* in bytes, 16Words aligned at least */
	unsigned int dst_stride; /* in bytes, 16Words aligned at least */
}; 

#define ALPHA_OSD_START         0x46a8
#define ALPHA_OSD_GET_INFO      0x46a9
#define ALPHA_OSD_SET_MODE      0x46aa
#define COMPRESS_START          0x46ab
#define COMPRESS_GET_INFO      0x46ac
#define COMPRESS_SET_MODE      0x46ad
#define ALPHA_OSD_PRINT      0x46ae

int aosd_compress_init(void);
void set_aosd_compress_buffer(struct jzfb_aosd_info *aosd);
void aosd_start_compress(void);
void calculate_compress_ratio(int dy, unsigned int height, unsigned int width, unsigned int frame_size);
int lcdc_should_use_com_decom_mode(void);
void print_aosd_regs(void);

#endif /* __AOSD_H__ */
