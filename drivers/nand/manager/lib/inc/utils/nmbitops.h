#ifndef __NM_BITOPS_H_
#define __NM_BITOPS_H_

static inline void nm_set_bit(unsigned int nr,unsigned int *addr)
{
	 *addr |= 1<<nr;
}

static inline void nm_clear_bit(unsigned int nr,unsigned int *addr)
{
	 *addr &= ~(1<<nr);
}

/*if bit equal set return 1 else 0 */
static inline int nm_test_bit(unsigned int nr,unsigned int *addr)
{
	return (*addr)&((1<<nr));
}
static inline int nm_test_bitcount(unsigned int nr,unsigned int enr,unsigned int *addr)
{
	int i,count=0;

	for(i=nr;i<enr;i++){
		if((*addr)&((1<<i))){
			count++;
		}
	}
	return count;
}
#endif

