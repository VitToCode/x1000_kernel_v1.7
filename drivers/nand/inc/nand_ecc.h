/*
 * lib_nand/jz_ecc.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) Ingenic Semiconductor Co., Ltd.
 */

#ifndef __JZ_ECC_H__
#define __JZ_ECC_H__

#include "jz4780_nand_def.h"

#define USE_BCH_AC	1	/* enable BCH Auto-Correction */

/*
 * BCH ECC Control
 */
#define BCH_ENCODE	1
#define BCH_DECODE	0

#define ECC_CODE_BUFF_LEN	(8 * 1024) 

/**
 * struct jz_ecc -
 *
 * @eccsize:	Data bytes per ECC step
 * @eccbytes:	ECC bytes per step
 * @eccpos:
 * @eccsteps:	Number of ECC steps per page
 * @errssize:   store BHINT register and BHERR0~11 register per step
 */
struct jz_ecc {
	unsigned int eccsize;
	unsigned int eccbytes;
	unsigned int parsize;
	unsigned int eccbit;
	unsigned int eccpos;
	unsigned int eccsteps;
	unsigned int errssize;
  //	unsigned int ecctotal;
	void (*ecc_init) (void *nand_ecc, void *flash_type);
	unsigned char *(*get_ecc_code_buffer) (unsigned int len);	
//	void (*ecc_get_eccbytes) (void *nand_ecc);
	void (*free_bch_buffer)(unsigned int len);	
//#ifdef CONFIG_MTD_NAND_DMA                               // added by hui
	int (*dma_ecc_encode_stage1)(unsigned char *sourcebuf, unsigned char *targetbuf);
	int (*dma_ecc_encode_stage2)(unsigned char *sourcebuf, unsigned char *eccbuf);
	int (*dma_ecc_decode_stage1)(unsigned char *sourcebuf, unsigned char *eccbuf, unsigned int *errsbuf);
	int (*dma_ecc_decode_stage2)(unsigned char *sourcebuf, unsigned char *targetbuf);
	void *(*get_pdma_databuf_pointer)(void);
//#else
	void (*ecc_enable_encode) (NAND_BASE *host,unsigned char *buf, unsigned char *eccbuf, unsigned int eccbit, unsigned int steps);
	int (*ecc_enable_decode) (NAND_BASE *host,unsigned char *buf, unsigned char *eccbuf, unsigned int eccbit, unsigned int steps);
	int (*ecc_finish) (void);
//	unsigned int ecc_code[ECC_CODE_BUFF_LEN];
	unsigned char *peccbuf;
//#endif
};

typedef struct jz_ecc JZ_ECC;

/*
 * Constants for oob configuration
 */
#define NAND_ECC_POS			2
//#define NAND_SMALL_BADBLOCK_POS         5
//#define NAND_LARGE_BADBLOCK_POS         0
#define NAND_BADBLOCK_POS       0

#endif /* __JZ_ECC_H__ */

