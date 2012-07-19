#ifndef _NANDALLOC_H_
#define _NANDALLOC_H_
#include <linux/slab.h>
#include <linux/vmalloc.h>

static inline void *Nand_VirtualAlloc(int size){
	return vmalloc(size);
}

static inline void Nand_VirtualFree(void *val){
	vfree(val);
}

static inline void *Nand_ContinueAlloc(int size){
	return kmalloc(size,GFP_KERNEL); 
}

static inline void Nand_ContinueFree(void *val){
	kfree(val);
}

#endif /* _NANDALLOC_H_ */
