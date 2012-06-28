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

#include <soc/gpio.h>
#include <soc/base.h>
#include <soc/irq.h>

#include <mach/platform.h>
#include <mach/jzdma.h>
#include <mach/jzsnd.h>

/* device IO define array */
struct jz_gpio_func_def platform_devio_array[] = {
#ifdef CONFIG_MMC0_JZ4780_PA_4BIT
	MSC0_PORTA_4BIT,
#endif
#ifdef CONFIG_MMC0_JZ4780_PA_8BIT
	MSC0_PORTA_8BIT,
#endif
#ifdef CONFIG_MMC0_JZ4780_PE_4BIT
	MSC0_PORTE,
#endif
#ifdef CONFIG_MMC0_JZ4780_PA_4BIT_RESET
	MSC0_PORTA_4BIT_RESET,
#endif
#ifdef CONFIG_MMC0_JZ4780_PA_8BIT_RESET
	MSC0_PORTA_8BIT_RESET,
#endif
#ifdef CONFIG_MMC1_JZ4780_PD_4BIT
	MSC1_PORTD,
#endif
#ifdef CONFIG_MMC1_JZ4780_PE_4BIT
	MSC1_PORTE,
#endif
#ifdef CONFIG_MMC2_JZ4780_PB_4BIT
	MSC2_PORTB,
#endif
#ifdef CONFIG_MMC2_JZ4780_PE_4BIT
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
#ifdef CONFIG_SERIAL_JZ47XX_UART0
	UART0_PORTF,
#endif
#ifdef CONFIG_SERIAL_JZ47XX_UART1
	UART1_PORTD,
#endif
#ifdef CONFIG_SERIAL_JZ47XX_UART2
	UART2_PORTD,
#endif
#ifdef CONFIG_SERIAL_JZ47XX_UART3
	UART3_PORTDE,
#endif
#ifdef CONFIG_SERIAL_JZ47XX_UART4
	UART4_PORTC,
#endif
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

static struct resource jz_pdma_res[] = {
	[0] = {
		.flags = IORESOURCE_MEM,
		.start = PDMA_IOBASE,
		.end = PDMA_IOBASE + 0x10000 - 1,
	},
	[1] = {
		.flags = IORESOURCE_IRQ,
		.name = "dma_irq",
		.start = IRQ_PDMA,
	}
};
struct platform_device jz_pdma_device = {
	.name = "jzdma",
	/* id = -1 means we only have one same device in system */
	.id = -1,
	.resource = jz_pdma_res,
	.num_resources = ARRAY_SIZE(jz_pdma_res),
};

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

/**
 * sound devices, include i2s0, i2s1, pcm0, pcm1 and an internal codec
 * note, the internal codec can only access by i2s0
 **/
static u64 jz_i2s_dmamask =  ~(u32)0;
#define SND_DEV_I2S0 SND_DEV_DSP0
#define SND_DEV_I2S1 SND_DEV_DSP1
#define DEF_I2S(NO)		\
static struct resource jz_i2s##NO##_resources[] = {			\
	[0] = {	\
		.start          = AIC##NO##_IOBASE,			\
		.end            = AIC##NO##_IOBASE + 0x1000 -1,	\
		.flags          = IORESOURCE_MEM,			\
	},	\
};	\
struct platform_device jz_i2s##NO##_device = {				\
	.name		= DEV_DSP_NAME,					\
	.id			= minor2index(SND_DEV_I2S##NO),		\
	.dev = {							\
		.dma_mask               = &jz_i2s_dmamask,		\
		.coherent_dma_mask      = 0xffffffff,			\
	},								\
	.resource       = jz_i2s##NO##_resources,			\
	.num_resources  = ARRAY_SIZE(jz_i2s##NO##_resources),		\
};
DEF_I2S(0);
DEF_I2S(1);

static u64 jz_pcm_dmamask =  ~(u32)0;
#define SND_DEV_PCM0 SND_DEV_DSP2
#define SND_DEV_PCM1 SND_DEV_DSP3
#define DEF_PCM(NO)								\
static struct resource jz_pcm##NO##_resources[] = {			\
	[0] = {								\
		.start          = PCM##NO##_IOBASE,			\
		.end            = PCM##NO##_IOBASE,			\
		.flags          = IORESOURCE_MEM,			\
	},	\
};	\
struct platform_device jz_pcm##NO##_device = {				\
	.name		= DEV_DSP_NAME,					\
	.id			= minor2index(SND_DEV_PCM##NO),		\
	.dev = {							\
		.dma_mask               = &jz_pcm_dmamask,		\
		.coherent_dma_mask      = 0xffffffff,			\
	},								\
	.resource       = jz_pcm##NO##_resources,			\
	.num_resources  = ARRAY_SIZE(jz_pcm##NO##_resources),		\
};
DEF_PCM(0);
DEF_PCM(1);

struct platform_device jz_codec_device = {
	.name		= "jz_codec",
};

static u64 jz_fb_dmamask = ~(u64)0;

#define DEF_LCD(NO)								\
static struct resource jz_fb##NO##_resources[] = {				\
	[0] = {									\
		.start          = LCDC##NO##_IOBASE,				\
		.end            = LCDC##NO##_IOBASE+ 0x1000 - 1,		\
		.flags          = IORESOURCE_MEM,				\
	},									\
	[1] = {									\
		.start          = IRQ_LCD##NO,					\
		.end            = IRQ_LCD##NO,					\
		.flags          = IORESOURCE_IRQ,				\
	},									\
};										\
struct platform_device jz_fb##NO##_device = {					\
	.name = "jz-fb",							\
	.id = NO,								\
	.dev = {								\
		.dma_mask               = &jz_fb_dmamask,			\
		.coherent_dma_mask      = 0xffffffff,				\
	},									\
	.num_resources  = ARRAY_SIZE(jz_fb##NO##_resources),			\
	.resource       = jz_fb##NO##_resources,				\
};
DEF_LCD(0);
DEF_LCD(1);


#define DEF_UART(NO)								\
static struct resource jz_uart##NO##_resources[] = {				\
	[0] = {									\
		.start          = UART##NO##_IOBASE,				\
		.end            = UART##NO##_IOBASE + 0x1000 - 1,		\
		.flags          = IORESOURCE_MEM,				\
	},									\
	[1] = {									\
		.start          = IRQ_UART##NO,					\
		.end            = IRQ_UART##NO,					\
		.flags          = IORESOURCE_IRQ,				\
	},									\
};										\
struct platform_device jz_uart##NO##_device = {					\
	.name = "jz-uart",							\
	.id = NO,								\
	.num_resources  = ARRAY_SIZE(jz_uart##NO##_resources),			\
	.resource       = jz_uart##NO##_resources,				\
};
DEF_UART(0);
DEF_UART(1);
DEF_UART(2);
DEF_UART(3);
DEF_UART(4);


