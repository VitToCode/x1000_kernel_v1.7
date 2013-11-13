#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

enum watch_ops {
	WATCH_OPEN = 1,
	WATCH_PIC_PATH,
	WATCH_PERIOD,
	WATCH_CLOSE,
};

int main(int argc,char **argv){
	int ret;
	int fd = open ("/dev/watch_update",O_RDWR);
	if(fd == -1) {
		perror("open file failure!\n");
		return -1;
	}
	//ret = ioctl(fd,WATCH_OPEN,0);
	//ret = ioctl(fd,WATCH_PERIOD,24);
	//ret = ioctl(fd,WATCH_CLOSE,0);
	ret = ioctl(fd,WATCH_PIC_PATH,"hell;workl;jfdkl;jlsfj;jfa;slkf;kflsja");
	if (ret == -1) {
		perror("operate the devicd failure!\n");
		return -1;
	}
	ret = close(fd);
	if (ret == -1) {
		perror("close file is failure!\n");
		return -1;
	}

	return 0;
}
