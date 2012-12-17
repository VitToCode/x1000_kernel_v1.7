#ifndef _NANDALLOC_H_
#define _NANDALLOC_H_

void *Nand_VirtualAlloc(int size);
void Nand_VirtualFree(void *val);
void *Nand_ContinueAlloc(int size);
void Nand_ContinueFree(void *val);

#endif /* _NANDALLOC_H_ */
