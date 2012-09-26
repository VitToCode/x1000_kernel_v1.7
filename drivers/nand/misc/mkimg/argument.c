#include "clib.h"
#include "FileDesc.h"

extern struct argoption arg_option;
char *GetIniKeyString(char *title,char *key,char *filename)
{
	FILE *fp;
	char szLine[1024];
	static char tmpstr[1024];
	int rtnval;
	int i = 0;
	int flag = 0;
	char *tmp;
	if(title == NULL)
		flag = 1;

	if((fp = fopen(filename, "r")) == NULL)
	{
		printf("have no	such file(%s) \n",filename);
		exit(1);
	}
	while(!feof(fp))
	{
		rtnval = fgetc(fp);
		if(rtnval == EOF)
		{
			break;
		}
		else
		{
			szLine[i++] = rtnval;
		}
		if(rtnval == '\n')
		{
			//i--;
			szLine[--i] = '\0';
			i = 0;
			tmp = strchr(szLine, '=');

			if(( tmp != NULL )&&(flag == 1))
			{
				if(strstr(szLine,key)!=NULL)
				{
					//注释行
					if ('#' == szLine[0]){
					}
					else if ( '/' == szLine[0] && '/' == szLine[1] ){
					}else
					{
						//找打key对应变量
						strcpy(tmpstr,tmp+1);
						fclose(fp);
						return tmpstr;
					}
				}
			}
			else
			{
				strcpy(tmpstr,"[");
				strcat(tmpstr,title);
				strcat(tmpstr,"]");
				if( strncmp(tmpstr,szLine,strlen(tmpstr)) == 0 )
				{
					//找到title
					flag = 1;
				}
			}
		}
	}
	fclose(fp);
	return "";
}
int get_filelen(char *filename){
	int size;
	FILE *fp;

	if((fp = fopen(filename, "r")) == NULL)
	{
		printf("have no	such file(%s) \n",filename);
		exit(1);
	}
	fseek(fp,0,SEEK_END);
	size = ftell(fp);
	fclose(fp);
	return size;
}
void get_option(int argc,char *argv[]){
    int result;
	opterr = 0;
	char *str;
    while( (result = getopt(argc, argv, "i::o::dpf::")) != -1 )
    {
		switch(result)
		{
		case 'i':
			arg_option.file_desc.inname = argv[optind];
			arg_option.file_desc.length = get_filelen(arg_option.file_desc.inname);
			break;
		case 'f':
			arg_option.configname = argv[optind];
			str = GetIniKeyString(0,"bytesperpage",arg_option.configname);
			arg_option.file_desc.bytesperpage = strtoul(str,0,0);
			str = GetIniKeyString(0,"pageperblock",arg_option.configname);
			arg_option.file_desc.pageperblock = strtoul(str,0,0);
			str = GetIniKeyString(0,"startblock",arg_option.configname);
			arg_option.file_desc.startblock = strtoul(str,0,0);
			str = GetIniKeyString(0,"blocks",arg_option.configname);
			arg_option.file_desc.blocks = strtoul(str,0,0);

			break;
		case 'p':

			arg_option.file_desc.bytesperpage = strtoul(argv[optind],0,0);
			arg_option.file_desc.pageperblock = strtoul(argv[optind+1],0,0);
			arg_option.file_desc.startblock = strtoul(argv[optind+2],0,0);
			arg_option.file_desc.blocks = strtoul(argv[optind+3],0,0);

			break;
		case 'o':
			arg_option.file_desc.outname = argv[optind];
			break;
		case 'd':
			arg_option.debug = 1; /* if 1: debug the bin to img */
			break;
		default:
			help();
			break;
		}
		printf("argv[%d]=%s\n", optind, argv[optind]);
    }
}
void dumpconfig(){

	printf("configname = %s\n",arg_option.configname);
	printf("inname = %s\n",arg_option.file_desc.inname);
	printf("outname = %s\n",arg_option.file_desc.outname);
	printf("infile size = %d\n",arg_option.file_desc.length);
   	printf("bytesperpage = %d\n",arg_option.file_desc.bytesperpage);
	printf("pageperblock = %d\n",arg_option.file_desc.pageperblock);
	printf("startblock = %d\n",arg_option.file_desc.startblock);
	printf("blocks = %d\n",arg_option.file_desc.blocks);

}
