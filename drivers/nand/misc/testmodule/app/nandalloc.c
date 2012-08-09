#include <NandAlloc.h>
#include <string.h>

#ifdef ALLOC_DEBUG
struct mmem_t{
    char* v;
	int size;
	char str[100];
};
static struct mmem_t mmem[1000];
static void *detect_addree = 0;

void Nand_adddetect(void *val){
	detect_addree = val;
}
static void* mmaloc(int size,char *str){
	char *v;
	int i = 0;
	v = malloc(size);
	for(i = 0;i < 1000;i++){
		if(mmem[i].v == 0)
			break;
	}
	mmem[i].v = v;
	mmem[i].size = size;
	strcpy(mmem[i].str,str);
	return v;
}
static void mfree(void *val,char* str){
	int i;
	for(i = 0;i < 1000;i++){
		if(mmem[i].v == val)
			break;
	}
	if(detect_addree){
		if(detect_addree == val){
			printf("<free> %s->%s\n",mmem[i].str,str);
		}
	}
	mmem[i].v = 0;
	free(val);
}

void* Nand_VirtualAlloc_debug(int size,char *str)
{
return mmaloc(size,str);
}
void Nand_VirtualFree_debug(void *val,char *str){
mfree(val,str);
}

void *Nand_ContinueAlloc_debug(int size,char *str){
return mmaloc(size,str);
}
void Nand_ContinueFree_debug(void *val,char *str){
mfree(val,str);
}
#endif
