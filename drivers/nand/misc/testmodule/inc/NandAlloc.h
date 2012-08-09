#ifndef _NANDALLOC_H_
#define _NANDALLOC_H_

#include "clib.h"
#ifndef ALLOC_DEBUG
static inline void *Nand_VirtualAlloc(int size){
	return malloc(size);
}
static inline void Nand_VirtualFree(void *val){
	return free(val);
}

static inline void *Nand_ContinueAlloc(int size){
	void *v = malloc(size);
	if(v == 0)
		printf("==================== size = %d\n",size);
	return v;
	//return malloc(size);
}
static inline void Nand_ContinueFree(void *val){
	return free(val);
}
#else
void *Nand_VirtualAlloc_debug(int size,char *str);
void Nand_VirtualFree_debug(void *val,char *str);

void *Nand_ContinueAlloc_debug(int size,char *str);
void Nand_ContinueFree_debug(void *val,char *str);

void Nand_adddetect(void *val);

#define Nand_VirtualAlloc(x) ({char s[100]; sprintf(s,"%s %d",__FILE__,__LINE__);Nand_VirtualAlloc_debug(x,s);});

#define Nand_ContinueAlloc(x) ({char s[100]; sprintf(s,"%s %d",__FILE__,__LINE__);Nand_ContinueAlloc_debug(x,s);});

#define Nand_VirtualFree(x) do{char s[100]; sprintf(s,"%s %d",__FILE__,__LINE__);Nand_VirtualFree_debug(x,s);}while(0);
#define Nand_ContinueFree(x) do{char s[100]; sprintf(s,"%s %d",__FILE__,__LINE__);Nand_ContinueFree_debug(x,s);}while(0);


#endif

#endif /* _NANDALLOC_H_ */
