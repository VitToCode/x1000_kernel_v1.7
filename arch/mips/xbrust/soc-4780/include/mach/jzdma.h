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
#define _RTP(NAME) JZDMA_REQ_##NAME##_tx,JZDMA_REQ_##NAME##_rx
	JZDMA_REQ_AUTO = 0x08,
	JZDMA_REQ_TSSI,
	JZDMA_REQ_DREQ = 0x0C,
	JZDMA_REQ_RES_0d,
	_RTP(UART3),_RTP(UART2),_RTP(UART1),_RTP(UART0),
	_RTP(SSI),_RTP(AIC),_RTP(MSC),
	JZDMA_REQ_TCU = 0x1C,
	JZDMA_REQ_SADC,
	_RTP(MSC1),_RTP(SSI1),_RTP(PM),_RTP(MSC2),_RTP(I2C),_RTP(I2C1),
	JZDMA_REQ_RES_3d = 0x3D,
	_RTP(I2C2),
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
	unsigned short req_type_tx,req_type_rx;
};

#endif
