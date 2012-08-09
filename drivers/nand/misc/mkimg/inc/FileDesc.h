#ifndef _FILEDESC_H_
#define _FILEDESC_H_

struct filedesc
{
    char *outname;
	char *inname;

	int length;
	int bytesperpage;
	int pageperblock;
	int startblock;
	int blocks;
	
};
struct argoption
{
	char *configname;
	struct filedesc file_desc;
};
void help();
void dumpconfig();
void get_option(int argc,char *argv[]);

void ND_probe(struct filedesc *file_desc);
#endif /* _FILEDESC_H_ */
