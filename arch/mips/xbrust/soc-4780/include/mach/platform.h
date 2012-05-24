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
#define	UART0_PORTF							\
	{ .port = GPIO_PORT_F, .func = GPIO_FUNC_0, .pins = 0xf, }
#define UART2_PORTC							\
	{ .port = GPIO_PORT_C, .func = GPIO_FUNC_0, .pins = 0x5<<28, }
	
#define MSC0_PORTA							\
	{ .port = GPIO_PORT_A, .func = GPIO_FUNC_1, .pins = 0x3b<<18, }, \
	{ .port = GPIO_PORT_A, .func = GPIO_FUNC_0, .pins = 0x1<<20, }
#define MSC1_PORTD							\
	{ .port = GPIO_PORT_D, .func = GPIO_FUNC_0, .pins = 0x3f<<20, }
#define MSC2_PORTE							\
	{ .port = GPIO_PORT_E, .func = GPIO_FUNC_2, .pins = 0x3ff<<20, }

#define I2C0_PORTD							\
	{ .port = GPIO_PORT_D, .func = GPIO_FUNC_0, .pins = 0x3<<30, }
#define I2C1_PORTE							\
	{ .port = GPIO_PORT_E, .func = GPIO_FUNC_0, .pins = 0x3<<30, }
#define I2C2_PORTF							\
	{ .port = GPIO_PORT_F, .func = GPIO_FUNC_2, .pins = 0x3<<16, }

#define LCD_PORTC							\
	{ .port = GPIO_PORT_C, .func = GPIO_FUNC_0, .pins = 0x0fffffff, }

#define PWM1_PORTE							\
	{ .port = GPIO_PORT_E, .func = GPIO_FUNC_0, .pins = 0x2, }

#define MII_PORTBDF							\
	{ .port = GPIO_PORT_B, .func = GPIO_FUNC_2, .pins = 0x10, },	\
	{ .port = GPIO_PORT_D, .func = GPIO_FUNC_1, .pins = 0x3c000000, }, \
	{ .port = GPIO_PORT_F, .func = GPIO_FUNC_0, .pins = 0xfff0, }


/* JZ SoC on Chip devices list */
extern struct platform_device jzsoc_pdma_device;
extern struct platform_device jzsoc_msc0_device;
extern struct platform_device jzsoc_msc1_device;
extern struct platform_device jzsoc_msc2_device;
extern struct platform_device jzsoc_i2c0_device;
extern struct platform_device jzsoc_i2c1_device;
extern struct platform_device jzsoc_i2c2_device;
extern struct platform_device jzsoc_lcdc_device;
extern struct platform_device jzsoc_mac_device;


/* register function for a 8250 console */
#ifdef CONFIG_SERIAL_8250
int __init jzsoc_register_8250serial(int id);
#else
#define jzsoc_register_8250serial(id) (0)
#endif

#endif
