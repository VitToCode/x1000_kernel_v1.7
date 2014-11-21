#ifndef __DMIC_TEST_H__
#define __DMIC_TEST_H__

#include "dmic_ops.h"


#define DMA_CHANNEL     (5)
#define DMIC_REQ_TYPE	(5)

#define TCSM_BANK_5     (0xb3427000)
#define TCSM_BANK_6		(0xb3428000)

#define NR_DESC         (4)
#define DMIC_RX_FIFO    (DMIC_BASE_ADDR + DMICDR)

#define DMA_DESC_ADDR		(TCSM_BANK_6)
#define DMA_DESC_SIZE	(NR_DESC * 8)

#define BUF_SIZE    (4096)



extern void dma_init_for_dmic(void);
#endif

