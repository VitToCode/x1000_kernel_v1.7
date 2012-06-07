/*
 * Platform device support for Jz4780 SoC.
 *
 * Copyright 2007, <zpzhong@ingenic.cn>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2.  This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/resource.h>

#include <soc/clk.h>
#include <soc/gpio.h>
#include <soc/base.h>
#include <soc/irq.h>

#include <mach/platform.h>
#include <mach/jzdma.h>

#ifdef CONFIG_SERIAL_8250
/* Serial device defined for serial console */
#include <linux/serial_core.h>
#include <linux/serial_8250.h>

int __init jzsoc_register_8250serial(int id)
{
	struct uart_port s;

	memset(&s, 0, sizeof(s));
	s.flags = UPF_BOOT_AUTOCONF | UPF_SKIP_TEST | UPF_IOREMAP;
	s.iotype = SERIAL_IO_MEM;
	s.regshift = 2;
	s.uartclk = 12000000;	/* always use 12MHz */

	s.line = id;
	s.mapbase = UART0_IOBASE + id * 0x1000;

	/* FIXME, if the irq order in INTC changed! */
	s.irq = IRQ_UART0 - id;

	return early_serial_setup(&s);
}
#endif

/* device IO define array */
struct jz_gpio_func_def platform_devio_array[] = {
#ifdef CONFIG_MMC0_JZ4780
	MSC0_PORTA,
#endif
#ifdef CONFIG_MMC1_JZ4780
	MSC1_PORTD,
#endif
#ifdef CONFIG_MMC2_JZ4780
	MSC2_PORTE,
#endif
#ifdef CONFIG_I2C0_JZ4780
	I2C0_PORTD,
#endif
#ifdef CONFIG_I2C1_JZ4780
	I2C1_PORTE,
#endif
#ifdef CONFIG_I2C2_JZ4780
	I2C2_PORTF,
#endif
#ifdef CONFIG_I2C3_JZ4780
	I2C3_PORTD,
#endif
#ifdef CONFIG_I2C4_JZ4780_PE3
	I2C4_PORTE_OFF3,
#endif
#ifdef CONFIG_I2C4_JZ4780_PE12
	I2C4_PORTE_OFF12,
#endif
#ifdef CONFIG_I2C4_JZ4780_PF
	I2C4_PORTF,
#endif
	UART2_PORTC,
	LCD_PORTC,
	PWM1_PORTE,
	MII_PORTBDF,
};

int platform_devio_array_size = ARRAY_SIZE(platform_devio_array);

int jz_device_register(struct platform_device *pdev,void *pdata)
{
	pdev->dev.platform_data = pdata;

	return platform_device_register(pdev);
}

static u64 jz_msc_dmamask =  ~(u32)0;

#define DEF_MSC(NO)								\
static struct resource jz_msc##NO##_resources[] = {				\
	{									\
		.start          = MSC##NO##_IOBASE,				\
		.end            = MSC##NO##_IOBASE + 0x1000 - 1,		\
		.flags          = IORESOURCE_MEM,				\
	},									\
	{									\
		.start          = IRQ_MSC##NO,					\
		.end            = IRQ_MSC##NO,					\
		.flags          = IORESOURCE_IRQ,				\
	},									\
};										\
struct platform_device jz_msc##NO##_device = {					\
	.name = "jzmmc",							\
	.id = NO,								\
	.dev = {								\
		.dma_mask               = &jz_msc_dmamask,			\
		.coherent_dma_mask      = 0xffffffff,				\
	},									\
	.resource       = jz_msc##NO##_resources,				\
	.num_resources  = ARRAY_SIZE(jz_msc##NO##_resources),			\
};
DEF_MSC(0);
DEF_MSC(1);
DEF_MSC(2);

static u64 jz_i2c_dmamask =  ~(u32)0;

#define DEF_I2C(NO)								\
static struct resource jz_i2c##NO##_resources[] = {				\
	[0] = {									\
		.start          = I2C##NO##_IOBASE,				\
		.end            = I2C##NO##_IOBASE + 0x1000 - 1,		\
		.flags          = IORESOURCE_MEM,				\
	},									\
	[1] = {									\
		.start          = IRQ_I2C##NO,					\
		.end            = IRQ_I2C##NO,					\
		.flags          = IORESOURCE_IRQ,				\
	},									\
	[2] = {									\
		.start          = JZDMA_REQ_I2C##NO##_TX,			\
		.end            = JZDMA_REQ_I2C##NO##_RX,			\
		.flags          = IORESOURCE_DMA,				\
	},									\
};										\
struct platform_device jz_i2c##NO##_device = {					\
	.name = "jz-i2c",							\
	.id = NO,								\
	.dev = {								\
		.dma_mask               = &jz_i2c_dmamask,			\
		.coherent_dma_mask      = 0xffffffff,				\
	},									\
	.num_resources  = ARRAY_SIZE(jz_i2c##NO##_resources),			\
	.resource       = jz_i2c##NO##_resources,				\
};
DEF_I2C(0);
DEF_I2C(1);
DEF_I2C(2);
DEF_I2C(3);
DEF_I2C(4);


