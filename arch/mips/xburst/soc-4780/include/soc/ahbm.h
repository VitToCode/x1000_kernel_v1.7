
#ifndef __INCLUDE_AHBM_
#define __INCLUDE_AHBM_

#define DDR_MC		0x130100E4
#define DDR_RESULT_1	0x130100D4
#define DDR_RESULT_2	0x130100D8
#define DDR_RESULT_3	0x130100DC
#define DDR_RESULT_4	0x130100E0

#define AHBM_CIM_IOB		0x130f0000
#define AHBM_AHB0_IOB	 	0x130f0100
#define AHBM_GPU_IOB 		0x130f0200
#define AHBM_LCD_IOB 		0x130f0300
#define AHBM_XXX_IOB 		0x13230000
#define AHBM_AHB2_IOB		0x134c0000

#define ahbm_restart(M)			\
do{ 					\
	outl(0x1,AHBM_##M##_IOB);	\
	outl(0x0,AHBM_##M##_IOB);	\
}while(0)

#define ahbm_stop(M)						\
do{ 								\
	outl(inl(AHBM_##M##_IOB) | 0x2,AHBM_##M##_IOB);		\
}while(0)

#define ahbm_read(M,off) inl((AHBM_##M##_IOB)+(off))

#define PRINT(ARGS...) len += sprintf (page+len, ##ARGS)

#define PRINT_ARRAY(n,M) \
	do{					\
		PRINT("CH%d(%x):%x\n",n,(AHBM_##M##_IOB+0x04),ahbm_read(M,0x04));	\
		PRINT("CH%d(%x):%x\n",n,(AHBM_##M##_IOB+0x08),ahbm_read(M,0x08));	\
		PRINT("CH%d(%x):%x\n",n,(AHBM_##M##_IOB+0x10),ahbm_read(M,0x10));	\
		PRINT("CH%d(%x):%x\n",n,(AHBM_##M##_IOB+0x14),ahbm_read(M,0x14));	\
		PRINT("CH%d(%x):%x\n",n,(AHBM_##M##_IOB+0x18),ahbm_read(M,0x18));	\
		PRINT("CH%d(%x):%x\n",n,(AHBM_##M##_IOB+0x1c),ahbm_read(M,0x1c));	\
		PRINT("CH%d(%x):%x\n",n,(AHBM_##M##_IOB+0x20),ahbm_read(M,0x20));	\
		PRINT("CH%d(%x):%x\n",n,(AHBM_##M##_IOB+0x24),ahbm_read(M,0x24));	\
		PRINT("CH%d(%x):%x\n",n,(AHBM_##M##_IOB+0x28),ahbm_read(M,0x28));	\
	}while(0);

#define PRINT_ARRAY2(n,M) \
	do{					\
		PRINT_ARRAY(n,M); \
		PRINT("CH%d(%x):%x\n",n,(AHBM_##M##_IOB+0x2c),ahbm_read(M,0x2c));	\
		PRINT("CH%d(%x):%x\n",n,(AHBM_##M##_IOB+0x30),ahbm_read(M,0x30));	\
		PRINT("CH%d(%x):%x\n",n,(AHBM_##M##_IOB+0x34),ahbm_read(M,0x34));	\
		PRINT("CH%d(%x):%x\n",n,(AHBM_##M##_IOB+0x38),ahbm_read(M,0x38));	\
		PRINT("CH%d(%x):%x\n",n,(AHBM_##M##_IOB+0x3c),ahbm_read(M,0x3c));	\
	}while(0);


#endif


