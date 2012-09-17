#ifndef __CHIP_CACHE_H__
#define __CHIP_CACHE_H__

#define cache_prefetch(label,l2)					\
do{									\
	unsigned long addr,end;						\
	/* Prefetch codes from label */					\
	addr = (unsigned long)(&&label) & ~(32 - 1);			\
	end = (unsigned long)(&&l2) & ~(32 - 1);			\
	end += 32;							\
	for (; addr < end; addr += 32) {				\
		__asm__ volatile (					\
				".set mips32\n\t"			\
				" cache %0, 0(%1)\n\t"			\
				".set mips32\n\t"			\
				:					\
				: "I" (Index_Prefetch_I), "r"(addr));	\
	}								\
}									\
while(0)

#endif /* __CHIP_CACHE_H__ */
