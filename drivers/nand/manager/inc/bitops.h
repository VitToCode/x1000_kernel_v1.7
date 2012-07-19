#ifndef __NAND_BITOPS_H_
#define __NAND_BITOPS_H_

/*bit operator */
#ifndef LINUX_KERNEL

static inline void set_bit(unsigned int nr,unsigned int *addr)
{
	 *addr |= 1<<nr;
}

static inline void clear_bit(unsigned int nr,unsigned int *addr)
{
	 *addr &= ~(1<<nr);
}

/*if bit equal set return 1 else 0 */
static inline int test_bit(unsigned int nr,unsigned int *addr)
{
	return (*addr)&((1<<nr));
}

#else
#include <linux/bitops.h>
#endif

#endif 

