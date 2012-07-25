#include <stdio.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define MODE_NAME_LEN 32

/* LCDC ioctl commands */
#define JZFB_GET_MODENUM		_IOR('F', 0x100, int)
#define JZFB_GET_MODELIST		_IOR('F', 0x101, char *)
#define JZFB_SET_MODE			_IOW('F', 0x102, char *)
#define JZFB_SET_VIDMEM			_IOW('F', 0x103, unsigned int *)
#define JZFB_ENABLE			_IO('F', 0x104)
#define JZFB_DISABLE			_IO('F', 0x105)

/* HDMI ioctl commands */
#define HDMI_POWER_OFF			_IO('F', 0x301)
#define	HDMI_VIDEOMODE_CHANGE		_IOW('F', 0x302, char *)
#define	HDMI_POWER_ON			_IO('F', 0x303)

int setmode();

int main(int argc, char*argv[])
{
	setmode(2);
	sleep(20);
	setmode(1);
	return 0;
}

int setmode(int tmp)
{
	int fbfd;
	int hdmifd;
	int mode_num;
	char (*ml)[32];
	char mode_name[32];
	int tmp_mode = tmp;
	int tmp_mode1 = 0;
	int i;
	unsigned int hdmi_vidmem;

	/* open fb1: tft ttl lcd */
	fbfd = open("/dev/graphics/fb1", O_RDWR);
	if (fbfd < 0) {
		printf(" open /dev/graphics/fb1 faild\n");
		return -1;
	}
	close(fbfd);

	
	fbfd = open("/dev/graphics/fb0", O_RDWR);
	if (fbfd < 0) {
		printf(" open /dev/graphics/fb0 faild\n");
		return -1;
	}

	/* get hdmi modes number */
	if (ioctl(fbfd, JZFB_GET_MODENUM, &mode_num) < 0) {
		printf("JZFB_GET_MODENUM faild\n");
		return -1;
	}
	printf("API FB mode number is %d\n", mode_num);
	ml = malloc(sizeof(char)* MODE_NAME_LEN * mode_num);
	if (ioctl(fbfd, JZFB_GET_MODELIST, ml) < 0) {
		printf("JZFB_GET_MODELIST faild\n");
		return -1;
	}

	for(i=0;i<mode_num;i++)
		printf("List[%d]: %s\n",i, ml[i]);

	hdmifd = open("/dev/hdmi", O_RDWR);
	if (hdmifd < 0) {
		printf("open hdmi device fail\n");
		return -1;
	}

	printf("please enter which mode you want to change:0-4  \n");
	scanf("%d", &tmp_mode);
	printf("---type-ok\n");
	if (ioctl(hdmifd, HDMI_VIDEOMODE_CHANGE, ml[tmp_mode]) < 0) {
		printf("HDMI_VIDEOMODE_CHANGE faild\n");
		return -1;
	}

//	printf("please enter the hdmi buffer physical address--\n");
//	scanf("%x", &hdmi_vidmem);
	hdmi_vidmem =0x0f400000;
	printf("--addr---ok\n");
	if (ioctl(fbfd, JZFB_SET_VIDMEM, &hdmi_vidmem) < 0) {
		printf("JZFB_SET_VIDMEM faild\n");
		return -1;
	}
	printf("--ioctl set vidmem---ok\n");
	if (ioctl(fbfd, JZFB_SET_MODE, ml[tmp_mode]) < 0) {
		printf("JZFB set hdmi mode faild\n");
		return -1;
	}
	printf("API set mode name = %s success\n", ml[tmp_mode]);

	/* enable LCD controller */
	if (ioctl(fbfd, JZFB_ENABLE, 0) < 0) {
		printf(" JZFB_ENABLE faild\n");
		return -1;
	}
	close(fbfd);
	close(hdmifd);
#if 0
	if (ioctl(hdmifd, HDMI_POWER_OFF, 0) < 0) {
		printf("HDMI_VIDEOMODE_   offf  faild\n");
		return -1;
	}
	printf("hdmi video mode off  success\n");
#endif
	return 0;
}
