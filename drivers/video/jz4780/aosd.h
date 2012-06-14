#ifndef __AOSD_H__
#define __AOSD_H__

struct jzfb_aosd_info {
	unsigned long addr0;
	unsigned long addr1;
	unsigned long addr2;
	unsigned long addr3;
	unsigned long waddr;

	unsigned long smem_start;
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
	unsigned char *buf_comp[2]; // buffer for compress output
}; 

int aosd_compress_init(void);
void set_aosd_compress_buffer(struct jzfb_aosd_info *aosd);
void aosd_start_compress(void);
void calculate_compress_ratio(int dy, unsigned int height, unsigned int width, unsigned int frame_size);
int lcdc_should_use_com_decom_mode(void);
void print_aosd_regs(void);

#endif /* __AOSD_H__ */
