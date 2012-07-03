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

enum jzdma_req_type {
#define _RTP(NAME) JZDMA_REQ_##NAME##_TX,JZDMA_REQ_##NAME##_RX
	JZDMA_REQ_RESERVED0 = 0x03,
	_RTP(I2S1),
	_RTP(I2S0),
	JZDMA_REQ_AUTO_TXRX = 0x08,
	JZDMA_REQ_SADC_RX,
	JZDMA_REQ_RESERVED1 = 0x0c,
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
#undef _RTP
};

/* this struct is used for the devices which use the dma engine */
struct jzdma_slave {
	/* the followed values must gaved by client device driver.
	 * 
	 *   For client device using, driver must set reg_width to 1/2/4.
	 * 
	 *   If need mem->device, should set tx_reg with write FIFO port
	 * phys address and req_type_tx with request type value like
	 * JZDMA_REQ_UART3_tx.
	 * 
	 *   If need device->mem, should set rx_reg with read FIFO port
	 * phys address and req_type_rx with request type value like
	 * JZDMA_REQ_UART3_rx.
	 * 
	 */

	/* transfer reg port width, always be 1,2,4 */
	short reg_width;
	/* max of transfer unit size, always be 1,2,4,8,16,32,64,128 
	 * for device use FIFO, it's equal to half of FIFO size
	 */
	short max_tsz;
	/* the tx/rx reg address */
	unsigned long tx_reg,rx_reg;

	/* cleint could set the EACKS,EACKM,ERDM,REIL */
#define JZDMA_DCM_MSK	0xF00F0000
	unsigned long dcm;
	/* enum jzdma_req_type value, like JZDMA_REQ_UART3_tx */
	unsigned short req_type_tx, req_type_rx;
};

#endif
