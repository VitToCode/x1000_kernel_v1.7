#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <dlfcn.h>
#include <errno.h>
#include "testfunc.h"

struct TestFuncType
{
    PTestFunc func;
	char *command;
	char *dllname;
	void *handle;
	struct TestFuncType *next;
};
struct TestFuncType *ptest=NULL;
void RegistTestFunc(char *dllname,void *handle,PTestFunc test,char *command){
	struct TestFuncType *top,*tmp;
	if(ptest){
		top = ptest;
		while(top->next){
			top = top->next;
		}
		tmp = (struct TestFuncType *)malloc(sizeof(struct TestFuncType));
		tmp->func = test;
		tmp->command = strdup(command);
		tmp->dllname = strdup(dllname);
		tmp->handle = handle;
		tmp->next = NULL;
		top->next = tmp;
	}else{
		ptest = (struct TestFuncType *)malloc(sizeof(struct TestFuncType));
		ptest->func = test;
		ptest->command = strdup(command);
		ptest->dllname = strdup(dllname);
		ptest->handle = handle;
		ptest->next = NULL;
	}
	printf("Register Command = %s\n",command);
}
int LoadShareLib(char *fn){
	FILE *fp;
	char dllname[256];
	void* handle;
	PTestFunc func;
	PInterface pi;
	char *command;
	fp = fopen(fn,"rb");
	if(fp == NULL){
		printf("not find %s\n",fn);
		return -1;
	}
	while(!feof(fp)){
		if(fscanf(fp,"%s",dllname) > 0)
		{
			handle = dlopen(dllname, RTLD_LAZY);
			if (handle) {
				pi = dlsym(handle, "GetInterface");
				if(pi == NULL){
					printf("load %s fail!\n GetInterface error: %s\n",dllname,strerror(errno));
				}else{
					if(!pi(&func,&command))
						RegistTestFunc(dllname,handle,func,command);
					else
						printf("load %s fail!\nInterface error\n",dllname);
				}
				
			}else{
				printf("load %s fail!\nerror: %s\n",dllname,dlerror());
			}
		}
	}
	fclose(fp);
	return 0;
}
int main(int argc, char *argv[])
{
	char *command;
	struct TestFuncType *top = ptest,*tmp;
    if(argc < 2)
	{
		printf("./%s func args\n",argv[0]);
	}
	if(LoadShareLib("./test.ini") < 0){
		printf("Load Share lib Error!\n");
		return 1;
	}
	command = argv[1];
	top = ptest;
	while(top){
		if(strcmp(top->command,command) == 0){
			printf("cur command %s\n",command);
			return top->func(argc,argv);
		} 
		top = top->next;
	}
	top = ptest;
	while(top)
	{
		tmp = top;
		if(tmp->dllname)
			free(tmp->dllname);
		if(tmp->command)
			free(tmp->command);
		
		free(tmp);
		top = top->next;
	}
	
    return 0;
}
