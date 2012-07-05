/*
 *  Copyright (C) 2010 Ingenic Semiconductor Inc.
 *
 *  Author: <zpzhong@ingenic.cn>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *
 * This file is a part of generic dmaengine, it's 
 * used for other device to use dmaengine.
 */

#ifndef __MACH_JZDMA_H__
#define __MACH_JZDMA_H__

#define NR_DMA_CHANNELS 	32

enum jzdma_req_type {
#define _RTP(NAME) JZDMA_REQ_##NAME##_TX,JZDMA_REQ_##NAME##_RX
	JZDMA_REQ_RESERVED0 = 0x03,
	_RTP(I2S1),
	_RTP(I2S0),
	JZDMA_REQ_AUTO_TXRX = 0x08,
	JZDMA_REQ_SADC_RX,
	JZDMA_REQ_RESERVED1 = 0x0b,
	_RTP(UART4),
	_RTP(UART3),
	_RTP(UART2),
	_RTP(UART1),
	_RTP(UART0),
	_RTP(SSI0),
	_RTP(SSI1),
	_RTP(MSC0),
	_RTP(MSC1),
	_RTP(MSC2),
	_RTP(PCM0),
	_RTP(PCM1),
	_RTP(I2C0),
	_RTP(I2C1),
	_RTP(I2C2),
	_RTP(I2C3),
	_RTP(I2C4),
	_RTP(DES),
#undef _RTP
};

enum jzdma_type {
	JZDMA_REQ_INVAL = 0,
#define _RTP(NAME) JZDMA_REQ_##NAME = JZDMA_REQ_##NAME##_TX
	_RTP(I2S1),
	_RTP(I2S0),
	JZDMA_REQ_AUTO = JZDMA_REQ_AUTO_TXRX,
	JZDMA_REQ_SADC = JZDMA_REQ_SADC_RX,
	_RTP(UART4),
	_RTP(UART3),
	_RTP(UART2),
	_RTP(UART1),
	_RTP(UART0),
	_RTP(SSI0),
	_RTP(SSI1),
	_RTP(MSC0),
	_RTP(MSC1),
	_RTP(MSC2),
	_RTP(PCM0),
	_RTP(PCM1),
	_RTP(I2C0),
	_RTP(I2C1),
	_RTP(I2C2),
	_RTP(I2C3),
	_RTP(I2C4),
	_RTP(DES),
	JZDMA_REQ_NAND0 = JZDMA_REQ_AUTO_TXRX,
	JZDMA_REQ_NAND1 = JZDMA_REQ_AUTO_TXRX,
	JZDMA_REQ_NAND2 = JZDMA_REQ_AUTO_TXRX,
	JZDMA_REQ_NAND3 = JZDMA_REQ_AUTO_TXRX,
	JZDMA_REQ_NAND4 = JZDMA_REQ_AUTO_TXRX,
#undef _RTP
};

struct jzdma_platform_data {
	enum jzdma_type map[NR_DMA_CHANNELS];
};

#endif

