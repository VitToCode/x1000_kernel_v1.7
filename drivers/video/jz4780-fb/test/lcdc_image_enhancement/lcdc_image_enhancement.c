#include<stdio.h>
#include<linux/fb.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "lcdc_image_enhancement.h"
#if 0 
 JZFB_GET_GAMMA	
 JZFB_SET_GAMMA	
 JZFB_GET_CSC	
 JZFB_SET_CSC	
 JZFB_GET_LUMA	
 JZFB_SET_LUMA	
 JZFB_GET_HUE	
 JZFB_SET_HUE	
 JZFB_GET_CHROMA	
 JZFB_SET_CHROMA	
 JZFB_GET_VEE	
 JZFB_SET_VEE	
 JZFB_GET_DITHER	
 JZFB_SET_DITHER	
#endif

static char st = 'w';
static int fb_fd0=0;
void Stop(int signo);
void Stop(int signo) 
{
	st = 'q';
	printf("---get ctrl +c \n");
	close(fb_fd0);
}

int main(int argc, char *argv[])
{
	int num=0,value=0,i=0,value1=0;
	struct enh_gamma  test_gamma;
	struct enh_csc	  test_csc;
	struct enh_luma	  test_luma;
	struct enh_hue	  test_hue;
	struct enh_chroma test_chroma;
	struct enh_vee	  test_vee;
	struct enh_dither test_dither;

	signal(SIGINT, Stop); 
	fb_fd0 = open("/dev/graphics/fb0", O_RDWR);
	if (fb_fd0 < 0) {
		printf("open fb 0 fail\n");
		return -1;
	}

while(st!='q'){

	num = 0;
	value = 0;
	printf("===================================================\n");
	printf("please select image enhancement options:\n");
	printf("1--CONTRAST_ENH\n");
	printf("2--BRIGHTNESS_ENH\n\n");
	printf("3--HUE_SIN_ENH\n");
	printf("4--HUE_COS_ENH\n\n");
//	printf("5--VEE_ENH\n");
	printf("6--SATURATION_ENH\n");
//	printf("7--DITHER_ENH\n");
//	printf("7--GAMMA_ENH\n");
//	printf("8--rgb-ycc\nchoice num= ");
	printf("9--Enable or Disabel enh \nchoice num= ");
	scanf("%d", &num);
	if(num != 8 || num !=9 ){
		printf("===please set your value--( 0~100 ):\n");
		scanf("%d", &value1);
	}

	switch (num){
		case 1:
			//printf("===please set your contrast value--( 0~2047 D:1024):\n");
			//scanf("%d", &value);
			value = value1*2047/100;
			printf("set=%d--real=%d\n",value1,value);
			if (ioctl(fb_fd0, JZFB_GET_LUMA, &test_luma) < 0) {
				printf("JZFB_GET_CSC faild\n");
				return -1;
			}

			test_luma.contrast_en=1;
			test_luma.contrast=value;

			if (ioctl(fb_fd0, JZFB_SET_LUMA, &test_luma) < 0) {
				printf("JZFB_SET_CSC faild\n");
				return -1;
			}

			break;
		case 2:
			//printf("===please set your brightness value--( 0~2047 D:1024):\n");
			//scanf("%d", &value);
			value = value1*2047/100;
			printf("set=%d--real=%d\n",value1,value);
			if (ioctl(fb_fd0, JZFB_GET_LUMA, &test_luma) < 0) {
				printf("JZFB_GET_LUMA faild\n");
				return -1;
			}

			test_luma.brightness_en=1;
			test_luma.brightness=value;

			if (ioctl(fb_fd0, JZFB_SET_LUMA, &test_luma) < 0) {
				printf("JZFB_SET_LUMA faild\n");
				return -1;
			}

			break;
		case 3:
			//printf("===please set your hue sin value--( 1024~3072 D:):\n");
			//scanf("%d", &value);
			value = value1*2048/100 + 1024;
			printf("set=%d--real=%d\n",value1,value);
			if (ioctl(fb_fd0, JZFB_GET_HUE, &test_hue) < 0) {
				printf("JZFB_GET_HUE faild\n");
				return -1;
			}

			test_hue.hue_en=1;
			test_hue.hue_sin=value;

			if (ioctl(fb_fd0, JZFB_SET_HUE, &test_hue) < 0) {
				printf("JZFB_SET_HUE faild\n");
				return -1;
			}
			break;
		case 4:
			//printf("===please set your hue cos value--( 1024~3072 ):\n");
			//scanf("%d", &value);
			value = value1*2048/100 + 1024;
			printf("set=%d--real=%d\n",value1,value);
			if (ioctl(fb_fd0, JZFB_GET_HUE, &test_hue) < 0) {
				printf("JZFB_GET_HUE faild\n");
				return -1;
			}

			test_hue.hue_en=1;
			test_hue.hue_cos=value;

			if (ioctl(fb_fd0, JZFB_SET_HUE, &test_hue) < 0) {
				printf("JZFB_SET_HUE faild\n");
				return -1;
			}
			break;
		case 5:
			//printf("===please set your vee value--( 0~1023 ):\n");
			//scanf("%d", &value);
			value = value1*1023/100;
			printf("set=%d--real=%d\n",value1,value);
			if (ioctl(fb_fd0, JZFB_GET_VEE, &test_vee) < 0) {
				printf("JZFB_GET_VEE faild\n");
				return -1;
			}

			test_vee.vee_en=1;
			for(i=0;i<512;i++){
				test_vee.vee_data0[i]=value;
				test_vee.vee_data1[i]=value;
			}

			if (ioctl(fb_fd0, JZFB_SET_VEE, &test_vee) < 0) {
				printf("JZFB_SET_VEE faild\n");
				return -1;
			}

			break;
		case 6:
		//	printf("===please set your saturation value--( 0~2047):\n");
		//	scanf("%d", &value);
			value = value1*2047/100;
			printf("set=%d--real=%d\n",value1,value);

			if (ioctl(fb_fd0, JZFB_GET_CHROMA, &test_chroma) < 0) {
				printf("JZFB_GET_CHROMA faild\n");
				return -1;
			}
			printf("====%x  %x\n",test_chroma.saturation_en,test_chroma.saturation);
			test_chroma.saturation_en=1;
			test_chroma.saturation=value;
			printf("====%x  %x\n",test_chroma.saturation_en,test_chroma.saturation);

			if (ioctl(fb_fd0, JZFB_SET_CHROMA, &test_chroma) < 0) {
				printf("JZFB_SET_CHROMA faild\n");
				return -1;
			}

			break;
		case 7:
			//printf("===please set your gamma value--( 0~1023 ):\n");
			//scanf("%d", &value);
			value = value1*1023/100;
			printf("set=%d--real=%d\n",value1,value);

			if (ioctl(fb_fd0, JZFB_GET_GAMMA, &test_gamma) < 0) {
				printf("JZFB_GET_GAMMA faild\n");
				return -1;
			}

			test_gamma.gamma_en=1;
			for(i=0;i<512;i++){
				test_gamma.gamma_data0[i]=value;
				test_gamma.gamma_data1[i]=value;
			}

			if (ioctl(fb_fd0, JZFB_SET_GAMMA, &test_gamma) < 0) {
				printf("JZFB_SET_GAMMA faild\n");
				return -1;
			}

			break;
		case 8:
			printf("===please set your ycc rgb value--( 0~3 ):\n");
			scanf("%d", &value);
			if (ioctl(fb_fd0, JZFB_GET_CSC, &test_csc) < 0) {
				printf("JZFB_GET_CSC faild\n");
				return -1;
			}

			test_csc.rgb2ycc_en=1;
			test_csc.rgb2ycc_mode=value;
			test_csc.ycc2rgb_en=1;
			test_csc.ycc2rgb_mode=value;

			if (ioctl(fb_fd0, JZFB_SET_CSC, &test_csc) < 0) {
				printf("JZFB_SET_CSC faild\n");
				return -1;
			}
			break;
		case 9:
			printf("===please set your enable or disable  value--( 0~1 ):\n");
			scanf("%d", &value);
			if (ioctl(fb_fd0, JZFB_ENABLE_ENH, &value) < 0) {
				printf("JZFB_ENABLE_ENH faild\n");
				return -1;
			}
			break;
		default:
			printf("--error\n");
			break;

	}
}
	return 0;
}

