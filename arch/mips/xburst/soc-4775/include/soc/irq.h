/*
 * IRQ number in JZ47xx INTC definition.
 *   Only support 4770 now. 2011-9-23
 *
 * Copyright (C) 2010 Ingenic Semiconductor Co., Ltd.
 */

#ifndef __INTC_IRQ_H__
#define __INTC_IRQ_H__

#include <irq.h>

enum {
// interrupt controller interrupts
	IRQ_AIC1 = IRQ_INTC_BASE,
	IRQ_AIC0,
	IRQ_BCH,
	IRQ_HDMI,
	IRQ_HDMI_WAKEUP,
	IRQ_OHCI,
	IRQ_RESERVED0,
	IRQ_SSI1,
	IRQ_SSI0,
	IRQ_TSSI0,
	IRQ_PDMA,
	IRQ_TSSI1,
	IRQ_GPIO5,
	IRQ_GPIO4,
	IRQ_GPIO3,
	IRQ_GPIO2,
	IRQ_GPIO1,
	IRQ_GPIO0,
#define IRQ_GPIO_PORT(N) (IRQ_GPIO0 - (N))
	IRQ_SADC,
	IRQ_X2D,
	IRQ_EHCI,
	IRQ_OTG,
	IRQ_IPU1,
	IRQ_LCD1,
	IRQ_GPS_1MS,
	IRQ_TCU2,
	IRQ_TCU1,
	IRQ_TCU0,
	IRQ_GPS,
	IRQ_IPU0,
	IRQ_CIM,
	IRQ_LCD0,

	IRQ_RTC,
	IRQ_OWI,
	IRQ_UART4,
	IRQ_MSC2,
	IRQ_MSC1,
	IRQ_MSC0,
	IRQ_SCC,
	IRQ_RESERVED3,
	IRQ_PCM0,
	IRQ_KBC,
	IRQ_GPVLC,
	IRQ_COMPRESS,
	IRQ_HARB2,
	IRQ_RESERVED4,
	IRQ_HARB0,
	IRQ_CPM,
	IRQ_UART3,
	IRQ_UART2,
	IRQ_UART1,
	IRQ_UART0,
	IRQ_DDR,
	IRQ_RESERVED5,
	IRQ_NEMC,
	IRQ_ETHC,
	IRQ_I2C4,
	IRQ_I2C3,
	IRQ_I2C2,
	IRQ_I2C1,
	IRQ_I2C0,
	IRQ_PDMAM,
	IRQ_VPU,
	IRQ_GPU,
};

enum {
	IRQ_MCU = IRQ_MCU_BASE,
};

#endif
