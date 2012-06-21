/*
 *  Copyright (C) 2010 Ingenic Semiconductor Inc.
 *
 *   In this file, here are some macro/device/function to
 * to help the board special file to organize resources
 * on the chip.
 */

#ifndef __SOC_4770_H__
#define __SOC_4770_H__

/* devio define list */
#define UART0_PORTF							\
	{ .name = "uart0", .port = GPIO_PORT_F, .func = GPIO_FUNC_0, .pins = 0x0f, },	\
#define UART1_PORTD							\
	{ .name = "uart1", .port = GPIO_PORT_D, .func = GPIO_FUNC_0, .pins = 0xf<<26, },\
#define UART2_PORTD							\
	{ .name = "uart2", .port = GPIO_PORT_D, .func = GPIO_FUNC_1, .pins = 0xf<<4, },	\
#define UART3_PORTDE							\
	{ .name = "uart3-pd-f0", .port = GPIO_PORT_D, .func = GPIO_FUNC_0, .pins = 0x1<<12, },\
	{ .name = "uart3-pe-f1", .port = GPIO_PORT_E, .func = GPIO_FUNC_1, .pins = 0x1<<5, },	\
	{ .name = "uart3-pe-f0", .port = GPIO_PORT_E, .func = GPIO_FUNC_0, .pins = 0x3<<7, },	\
#define UART4_PORTC							\
	{ .name = "uart4", .port = GPIO_PORT_C, .func = GPIO_FUNC_2, .pins = 1<<10 | 1<<20, },	\
/*******************************************************************************************************************/

#define MSC0_PORTA							\
	{ .name = "msc0-port-a-func1", .port = GPIO_PORT_A, .func = GPIO_FUNC_1, .pins = 0x3b<<18, }, \
	{ .name = "msc0-port-a-func0", .port = GPIO_PORT_A, .func = GPIO_FUNC_0, .pins = 0x1<<20, }
#define MSC1_PORTD							\
	{ .name = "msc1", .port = GPIO_PORT_D, .func = GPIO_FUNC_0, .pins = 0x3f<<20, }
#define MSC2_PORTE							\
	{ .name = "msc2", .port = GPIO_PORT_E, .func = GPIO_FUNC_2, .pins = 0x3ff<<20, }

/*******************************************************************************************************************/
#define I2C0_PORTD							\
	{ .name = "i2c0", .port = GPIO_PORT_D, .func = GPIO_FUNC_0, .pins = 0x3<<30, }
#define I2C1_PORTE							\
	{ .name = "i2c1", .port = GPIO_PORT_E, .func = GPIO_FUNC_0, .pins = 0x3<<30, }
#define I2C2_PORTF							\
	{ .name = "i2c2", .port = GPIO_PORT_F, .func = GPIO_FUNC_2, .pins = 0x3<<16, }
#define I2C3_PORTD							\
	{ .name = "i2c3", .port = GPIO_PORT_D, .func = GPIO_FUNC_1, .pins = 0x3<<10, }
#define I2C4_PORTE_OFF3							\
	{ .name = "i2c4-port-e-func1-off3", .port = GPIO_PORT_E, .func = GPIO_FUNC_1, .pins = 0x3<<3, }
#define I2C4_PORTE_OFF12						\
	{ .name = "i2c4-port-e-func1-off12", .port = GPIO_PORT_E, .func = GPIO_FUNC_1, .pins = 0x3<<12, }
#define I2C4_PORTF							\
	{ .name = "i2c4-port-f-func1", .port = GPIO_PORT_F, .func = GPIO_FUNC_1, .pins = 0x3<<24, }

/*******************************************************************************************************************/

#define LCD_PORTC							\
	{ .name = "lcd", .port = GPIO_PORT_C, .func = GPIO_FUNC_0, .pins = 0x0fffffff, }

#define PWM1_PORTE							\
	{ .name = "pwm1", .port = GPIO_PORT_E, .func = GPIO_FUNC_0, .pins = 0x2, }

#define MII_PORTBDF							\
	{ .name = "mii-0", .port = GPIO_PORT_B, .func = GPIO_FUNC_2, .pins = 0x10, },	\
	{ .name = "mii-1", .port = GPIO_PORT_D, .func = GPIO_FUNC_1, .pins = 0x3c000000, }, \
	{ .name = "mii-2", .port = GPIO_PORT_F, .func = GPIO_FUNC_0, .pins = 0xfff0, }

/* JZ SoC on Chip devices list */
extern struct platform_device jz_msc0_device;
extern struct platform_device jz_msc1_device;
extern struct platform_device jz_msc2_device;

extern struct platform_device jz_i2c0_device;
extern struct platform_device jz_i2c1_device;
extern struct platform_device jz_i2c2_device;
extern struct platform_device jz_i2c3_device;
extern struct platform_device jz_i2c4_device;

extern struct snd_dev_data i2s0_data;
extern struct snd_dev_data i2s1_data;
extern struct snd_dev_data pcm0_data;
extern struct snd_dev_data pcm1_data;

extern struct platform_device jz_i2s0_device;
extern struct platform_device jz_i2s1_device;
extern struct platform_device jz_pcm0_device;
extern struct platform_device jz_pcm1_device;
extern struct platform_device jz_codec_device;

extern struct platform_device jz_fb0_device;
extern struct platform_device jz_fb1_device;

extern struct platform_device jz_uart0_device;
extern struct platform_device jz_uart1_device;
extern struct platform_device jz_uart2_device;
extern struct platform_device jz_uart3_device;
extern struct platform_device jz_uart4_device;

int jz_device_register(struct platform_device *pdev,void *pdata);

/* register function for a 8250 console */
#ifdef CONFIG_SERIAL_8250
int __init jzsoc_register_8250serial(int id);
#else
#define jzsoc_register_8250serial(id) (0)
#endif

#endif
