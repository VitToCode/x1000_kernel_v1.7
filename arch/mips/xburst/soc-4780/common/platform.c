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
#include <mach/jznand.h>

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
#ifdef CONFIG_NAND_JZ4780_COMMON
        NAND_PORTAB_COMMON,
#endif
#ifdef CONFIG_NAND_JZ4780_CS1
        NAND_PORTA_CS1,
#endif
#ifdef CONFIG_NAND_JZ4780_CS2
        NAND_PORTA_CS2,
#endif
#ifdef CONFIG_NAND_JZ4780_CS3
        NAND_PORTA_CS3,
#endif
#ifdef CONFIG_NAND_JZ4780_CS4
        NAND_PORTA_CS4,
#endif
#ifdef CONFIG_NAND_JZ4780_CS5
        NAND_PORTA_CS5,
#endif
#ifdef CONFIG_NAND_JZ4780_CS6
        NAND_PORTA_CS6,
#endif
#ifdef CONFIG_JZ_EXTERNAL_CODEC
        I2S0_PORTE,	
#endif
        LCD_PORTC,
	HDMI_PORTF, 
        PWM1_PORTE,
#ifdef CONFIG_JZ_MAC
        MII_PORTBDF,
#endif
#if defined(USB_DWC_OTG_DUAL) || defined(USB_DWC_HOST_ONLY)
        OTG_DRVVUS,
#endif
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
		.name  = "irq",
                .flags = IORESOURCE_IRQ,
                .start = IRQ_PDMA,
        },
        [2] = {
		.name  = "pdmam",
                .flags = IORESOURCE_IRQ,
                .start = IRQ_PDMAM,
        },
        [3] = {
		.name  = "mcu",
                .flags = IORESOURCE_IRQ,
                .start = IRQ_MCU,
        },
};

static struct jzdma_platform_data jzdma_pdata = {
	.irq_base = IRQ_MCU_BASE,
	.irq_end = IRQ_MCU_END,
        .map = {
                JZDMA_REQ_NAND0,
                JZDMA_REQ_NAND1,
                JZDMA_REQ_NAND2,
                JZDMA_REQ_NAND3,
                JZDMA_REQ_NAND4,
                JZDMA_REQ_I2S1,
                JZDMA_REQ_I2S1,
                JZDMA_REQ_I2S0,
                JZDMA_REQ_I2S0,
                JZDMA_REQ_UART1,
                JZDMA_REQ_UART0,
                JZDMA_REQ_SSI0,
                JZDMA_REQ_SSI1,
                JZDMA_REQ_PCM0,
                JZDMA_REQ_PCM0,
                JZDMA_REQ_PCM1,
                JZDMA_REQ_PCM1,
                JZDMA_REQ_I2C0,
                JZDMA_REQ_I2C0,
                JZDMA_REQ_I2C1,
                JZDMA_REQ_I2C1,
                JZDMA_REQ_I2C2,
                JZDMA_REQ_I2C2,
                JZDMA_REQ_I2C3,
                JZDMA_REQ_I2C3,
                JZDMA_REQ_I2C4,
                JZDMA_REQ_I2C4,
                JZDMA_REQ_DES,
        },
};

struct platform_device jz_pdma_device = {
        .name = "jz-dma",
        .id = -1,
        .dev = {
                .platform_data = &jzdma_pdata,
        },
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
                        .start          = JZDMA_REQ_I2C##NO,				\
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
                [1] = {	\
                        .start			= IRQ_AIC##NO,	\
                        .end			= IRQ_AIC##NO,	\
                        .flags			= IORESOURCE_IRQ,	\
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

static u64 jz_ipu_dmamask = ~(u64)0;

#define DEF_IPU(NO)								\
        static struct resource jz_ipu##NO##_resources[] = {				\
                [0] = {									\
                        .start          = IPU##NO##_IOBASE,				\
                        .end            = IPU##NO##_IOBASE+ 0x8000 - 1,		\
                        .flags          = IORESOURCE_MEM,				\
                },									\
                [1] = {									\
                        .start          = IRQ_IPU##NO,					\
                        .end            = IRQ_IPU##NO,					\
                        .flags          = IORESOURCE_IRQ,				\
                },									\
        };										\
struct platform_device jz_ipu##NO##_device = {					\
        .name = "jz-ipu",							\
        .id = NO,								\
        .dev = {								\
                .dma_mask               = &jz_ipu_dmamask,			\
                .coherent_dma_mask      = 0xffffffff,				\
        },									\
        .num_resources  = ARRAY_SIZE(jz_ipu##NO##_resources),			\
        .resource       = jz_ipu##NO##_resources,				\
};
DEF_IPU(0);
DEF_IPU(1);


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

static struct resource jz_cim_res[] = {
        [0] = {
                .flags = IORESOURCE_MEM,
                .start = CIM_IOBASE,
                .end = CIM_IOBASE + 0x10000 - 1,
        },
        [1] = {
                .flags = IORESOURCE_IRQ,
                .start = IRQ_CIM,
        }
};

struct platform_device jz_cim_device = {
        .name = "jz-cim",
        .id = -1,
        .resource = jz_cim_res,
        .num_resources = ARRAY_SIZE(jz_cim_res),
};

/* OHCI (USB full speed host controller) */
static struct resource jz_ohci_resources[] = {
        [0] = {
                .start		= OHCI_IOBASE,
                .end		= OHCI_IOBASE + 0x10000 - 1,
                .flags		= IORESOURCE_MEM,
        },
        [1] = {
                .start		= IRQ_OHCI,
                .end		= IRQ_OHCI,
                .flags		= IORESOURCE_IRQ,
        },
};

static u64 ohci_dmamask = ~(u32)0;

struct platform_device jz_ohci_device = {
        .name		= "jz-ohci",
        .id		= 0,
        .dev = {
                .dma_mask		= &ohci_dmamask,
                .coherent_dma_mask	= 0xffffffff,
        },
        .num_resources	= ARRAY_SIZE(jz_ohci_resources),
        .resource	= jz_ohci_resources,
};

/* EHCI (USB high speed host controller) */
static struct resource jz_ehci_resources[] = {
        [0] = {
                .start		= EHCI_IOBASE,
                .end		= EHCI_IOBASE + 0x10000 - 1,
                .flags		= IORESOURCE_MEM,
        },
        [1] = {
                .start		= IRQ_EHCI,
                .end		= IRQ_EHCI,
                .flags		= IORESOURCE_IRQ,
        },
};

/* The dmamask must be set for OHCI to work */
static u64 ehci_dmamask = ~(u32)0;

struct platform_device jz_ehci_device = {
        .name		= "jz-ehci",
        .id		= 0,
        .dev = {
                .dma_mask		= &ehci_dmamask,
                .coherent_dma_mask	= 0xffffffff,
        },
        .num_resources	= ARRAY_SIZE(jz_ehci_resources),
        .resource	= jz_ehci_resources,
};

static struct resource	jz_mac_res[] = {
        { .flags = IORESOURCE_MEM,
                .start = ETHC_IOBASE,
                .end = ETHC_IOBASE + 0xfff,
        },
        { .flags = IORESOURCE_IRQ,
                .start = IRQ_ETHC,
        },
};

struct platform_device jz_mac = {
        .name = "jzmac",
        .id = 0,
        .num_resources = ARRAY_SIZE(jz_mac_res),
        .resource = jz_mac_res,
        .dev = {
                .platform_data = NULL,
        },
};

/*  nand device  */
static struct resource jz_nand_res[] ={
        /**  nemc resource  **/
        [0] = {
                .flags = IORESOURCE_MEM,
                .start = NEMC_IOBASE,
                .end = NEMC_IOBASE + 0x10000 -1,
        },
        [1] = {
                .flags = IORESOURCE_IRQ,
                .start = IRQ_NEMC,
        },
        /**  bch resource  **/
        [2] = {
                .flags = IORESOURCE_MEM,
                .start = BCH_IOBASE,
                .end = BCH_IOBASE + 0x10000 -1,
        },
        [3] = {
                .flags = IORESOURCE_IRQ,
                .start = IRQ_BCH,
        },
        /**  pdma resource  **/
        [4] = {
                .flags = IORESOURCE_MEM,
                .start = PDMA_IOBASE,
                .end = PDMA_IOBASE +0x10000 -1,
        },
        [5] = {
                .flags = IORESOURCE_IRQ,
                .start = (32*0)+20,               // GPIOA 20
        },
        [6] = {
                .flags = IORESOURCE_DMA,
                .start = JZDMA_REQ_NAND3,
        },
        [7] = {
                .flags = IORESOURCE_DMA,
                .start = JZDMA_REQ_AUTO,
        },
        /**  csn resource  **/
        [8] = {
                .flags = IORESOURCE_MEM,
                .start = NEMC_CS6_IOBASE,
                .end = NEMC_CS6_IOBASE + 0x6000000 -1,
        }
};

struct platform_device jz_nand_device = {
        .name = "jz_nand",
        .id = -1,
        //	.dev.platform_data = &pisces_nand_chip_data,
        .resource = jz_nand_res,
        .num_resources =ARRAY_SIZE(jz_nand_res),
};

static struct resource jz_hdmi_resources[] = {
        [0] = {
                .flags = IORESOURCE_MEM,
                .start = HDMI_IOBASE,
                .end = HDMI_IOBASE + 0x8000 - 1,
        },
        [1] = {
                .flags = IORESOURCE_IRQ,
                .start = IRQ_HDMI,
        },
};

struct platform_device jz_hdmi = {
        .name = "jz-hdmi",
        .id = -1,
        .num_resources = ARRAY_SIZE(jz_hdmi_resources),
        .resource = jz_hdmi_resources,
};

static struct resource jz4780_rtc_resource[] = {    
        [0] = { 
                .start = RTC_IOBASE,
                .end   = RTC_IOBASE + 0xff,
                .flags = IORESOURCE_MEM,
        },    
        [1] = { 
                .start = IRQ_RTC,
                .end   = IRQ_RTC,
                .flags = IORESOURCE_IRQ
        }    
};    

struct platform_device jz4780_device_rtc = { 
        .name             = "jz4780-rtc",
        .id               = 0, 
        .num_resources    = ARRAY_SIZE(jz4780_rtc_resource),
        .resource         = jz4780_rtc_resource,
};    
