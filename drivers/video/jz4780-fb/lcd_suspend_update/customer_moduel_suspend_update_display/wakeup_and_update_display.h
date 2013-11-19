#ifndef __WAKEUP_AND_UPDATE_DISPLAY_H__
#define __WAKEUP_AND_UPDATE_DISPLAY_H__


struct clock_bitmap_buffer {
	int valid;
	int hour;
	int minute;
	char * buffer;
};


struct clock_buffers {
	int buffer_count;
	int next_buffer_count;
	int buffer_size;

	struct clock_bitmap_buffer * bitmap_buffers;
};

struct pic_arg {
	int pic_count;
	char *pic_buf;
	char *format;
};

enum watch_ops {
	WATCH_OPEN = 1,
	WATCH_PIC_PATH,
	WATCH_PERIOD,
	WATCH_CLOSE,
};

#pragma pack(push)
#pragma pack(2)
typedef struct BITMAPFILEHEADER
{
	unsigned short int       bfType;
	unsigned int             bfSize;
	unsigned short int       bfReserved1;
	unsigned short int       bfReserved2;
	unsigned int             bfOffBits;

} BITMAPFILEHEADER;
#pragma pack(pop)

typedef struct BMPINFOHEADER
{
	unsigned int             biSize;
	unsigned int             biWidth;
	unsigned int             biHeight;
	unsigned short int       biPlanes;
	unsigned short int       biBitCount;
	unsigned short int       biCompression;
	unsigned int             biSizeImage;
	unsigned int             biXPelsPerMeter;
	unsigned int             biYPelsPerMeter;
	unsigned int             biClrUsed;
	unsigned int             biClrImportant;
} BITMAPINFOHEADER;
typedef struct
{
	BITMAPFILEHEADER file;
	BITMAPINFOHEADER info;
} bmp;

typedef union RGB {
	struct{
		unsigned char   rgbBlue;
		unsigned char   rgbGreen;
		unsigned char   rgbRed;
		unsigned char   rgbReserved;
	};
	unsigned long u;
} RGB;

typedef struct _BMPINFO {
	BITMAPINFOHEADER   bmiHeader;
	RGB             bmiColors[256];
} BMPINFO;


extern int is_configed_slcd_rtc_alarm_refresh(void);

extern int update_clock(void);

extern int slcd_refresh_prepare(void);
extern int slcd_refresh_finish(void);

#endif /* __WAKEUP_AND_UPDATE_DISPLAY_H__ */
