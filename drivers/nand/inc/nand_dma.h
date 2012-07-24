/*
 * lib_nand/core/nand_dma.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) Ingenic Semiconductor Co., Ltd.
 */

#ifndef __NAND_DMA_H__
#define __NAND_DMA_H__

enum {
	BANK0_POINTER,              // first address of 2k buffer for storing bch errs report register or parity register
//	BANK1_POINTER,              // first address of 2k buffer for storing data
//	BANK2_POINTER,              // first address of 2k buffer for storing data
	DATA_POINTER_LIST,          // first address of linked list for data buffer pointer
};

typedef struct linked_list {
	unsigned char *pbuf;
	unsigned int *perrs;
	struct linked_list *next;
	unsigned char mark;
}LINKED_LIST;

/*
 * BDMA Control
 */
#define BCH_DMA_CHAN	0   // fixed to channel 0
#define NAND_DMA_CHAN	1   // fixed to channel 1 
#define MEMCPY_DMA_CHAN 2   // fixed to channel 2

#define DMA_TERMINATE  0
#define DMA_RUNNING -1
#define DMA_AR  -2



struct nand_bdma{
	int (*nand_dma_init)(JZ_ECC *pnand_ecc);
	void (*nand_dma_deinit)(void);
    void *(*get_relevant_pointer)(void);   //
	unsigned char *(*get_dma_ecc_buf)(unsigned int len);                      	             
	void (*reset_bch_buffer)(unsigned int len);	
	void (*nand_dma_start)(unsigned int channel);
	int (*nand_dma_finish)(unsigned int channel);
};
typedef struct nand_bdma JZ_NAND_BDMA;

#endif /* _NAND_DMA_H_ */
