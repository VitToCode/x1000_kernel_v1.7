#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
	FILE *fb_bin = NULL;
	FILE *fb_c = NULL;

	char *head_c = "unsigned char WAVEFORM_LUT[] = {\n";
	char *end_c = "\n};\n";

	unsigned long file_length = 0;
	unsigned long write_char_num = 0;

	int ret = 0;

	if(argc != 3){
		printf("ERROR:Input parameter error\n");
		printf("./binary_array_tools source_file object_file\n");
		return -1;
	}
	printf("source file:%s\nobject_file:%s\n", argv[1], argv[2]);

	fb_bin = fopen(argv[1], "rb");
	if(fb_bin == NULL) {
		printf("ERROR:Open %s failed\n", argv[1]);
		return -1;
	}

	if(fseek(fb_bin, 0, SEEK_END)) {
		printf("ERROR:Calculated length of the %s failed\n", argv[1]);
		fclose(fb_bin);
		return -1;
	}
	file_length = ftell(fb_bin);
	printf("%s:file length = %ld\n", argv[1], file_length);

	if(fseek(fb_bin, 0, SEEK_SET)) {
		printf("ERROR:Calculated length of the %s failed\n", argv[1]);
		fclose(fb_bin);
		return -1;
	}

	fb_c = fopen(argv[2], "w");
	if(fb_c == NULL) {
		printf("ERROR:Open %s failed\n", argv[2]);
		fclose(fb_bin);
		return -1;
	}

	ret = fwrite(head_c, strlen(head_c), 1, fb_c);
	while(write_char_num < file_length) {
		char write_buffer[100] = "";
		unsigned char read_buffer[11] = "";

		ret = fread(read_buffer, 1, 10, fb_bin);
		sprintf(write_buffer, "    0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x, 0x%02x,\n",
				read_buffer[0], read_buffer[1], read_buffer[2], read_buffer[3], read_buffer[4],
				read_buffer[5], read_buffer[6], read_buffer[7], read_buffer[8], read_buffer[9]);

		write_char_num += ret;

		if(ret < 10) {
			ret = fwrite(write_buffer, 1, ret * strlen("0x00, ") + 4 - 2, fb_c);
		} else {
			ret = fwrite(write_buffer, 1, strlen(write_buffer), fb_c);
		}

		printf("\r %lu%%", (write_char_num*100/file_length));
		fflush(stdout);
	}
	ret = fwrite(end_c, strlen(end_c), 1, fb_c);
	printf("\r 100%%\n");

	fclose(fb_bin);
	fclose(fb_c);
	return 0;
}
