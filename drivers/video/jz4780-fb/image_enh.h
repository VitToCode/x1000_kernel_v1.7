
#ifndef _IMAGE_ENH_H_
#define _IMAGE_ENH_H_

struct enh_gamma {
	__u32 gamma_en:1;
	__u32 gamma_data0[512];
	__u32 gamma_data1[512];
};

struct enh_csc {
	__u32 rgb2ycc_en:1;
	__u32 rgb2ycc_mode;
	__u32 ycc2rgb_en:1;
	__u32 ycc2rgb_mode;
};

struct enh_luma {
	__u32 brightness_en:1;
	__u32 brightness;
	__u32 contrast_en:1;
	__u32 contrast;
};

struct enh_hue {
	__u32 hue_en:1;
	__u32 hue_sin;
	__u32 hue_cos;
};

struct enh_chroma {
	__u32 saturation_en:1;
	__u32 saturation;
};

struct enh_vee {
	__u32 vee_en:1;
	__u32 vee_data0[512];
	__u32 vee_data1[512];
};

struct enh_dither {
	__u32 dither_en:1;
	__u32 dither_red;
	__u32 dither_green;
	__u32 dither_blue;
};

#define JZFB_GET_GAMMA			_IOW('F', 0x120, struct enh_gamma)
#define JZFB_SET_GAMMA			_IOW('F', 0x121, struct enh_gamma)
#define JZFB_GET_CSC			_IOW('F', 0x122, struct enh_csc)
#define JZFB_SET_CSC			_IOW('F', 0x123, struct enh_csc)
#define JZFB_GET_LUMA			_IOW('F', 0x124, struct enh_luma)
#define JZFB_SET_LUMA			_IOW('F', 0x125, struct enh_luma)
#define JZFB_GET_HUE			_IOW('F', 0x126, struct enh_hue)
#define JZFB_SET_HUE			_IOW('F', 0x127, struct enh_hue)
#define JZFB_GET_CHROMA			_IOW('F', 0x128, struct enh_chroma)
#define JZFB_SET_CHROMA			_IOW('F', 0x129, struct enh_chroma)
#define JZFB_GET_VEE			_IOW('F', 0x130, struct enh_vee)
#define JZFB_SET_VEE			_IOW('F', 0x131, struct enh_vee)
#define JZFB_GET_DITHER			_IOW('F', 0x132, struct enh_dither)
#define JZFB_SET_DITHER			_IOW('F', 0x133, struct enh_dither)

#endif /* _IMAGE_ENH_H_ */
