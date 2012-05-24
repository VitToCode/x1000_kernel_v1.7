/*
 * JZSOC GPIO port, usually used in arch code.
 *
 * Copyright (C) 2010 Ingenic Semiconductor Co., Ltd.
 */

#ifndef __JZSOC_JZ_GPIO_H__
#define __JZSOC_JZ_GPIO_H__

enum gpio_function {
	GPIO_FUNC_0 	= 0x0,  //0000, GPIO as function 0 / device 0
	GPIO_FUNC_1 	= 0x1,  //0001, GPIO as function 1 / device 1
	GPIO_FUNC_2 	= 0x2,  //0010, GPIO as function 2 / device 2
	GPIO_FUNC_3 	= 0x3,  //0011, GPIO as function 3 / device 3
	GPIO_OUTPUT0 	= 0x4,  //0100, GPIO output low  level
	GPIO_OUTPUT1 	= 0x5,  //0101, GPIO output high level
	GPIO_INPUT 	= 0x6,  //0110, GPIO as input
	GPIO_INT_LO 	= 0x8,  //1000,	Low  Level trigger interrupt
	GPIO_INT_HI 	= 0x9,  //1001,	High Level trigger interrupt
	GPIO_INT_FE 	= 0xa,  //1010,	Fall Edge trigger interrupt
	GPIO_INT_RE 	= 0xb,  //1011,	Rise Edge trigger interrupt
};
#define GPIO_AS_FUNC(func)  (! ((func) & 0xc))

enum gpio_port {
	GPIO_PORT_A, GPIO_PORT_B,
	GPIO_PORT_C, GPIO_PORT_D,
	GPIO_PORT_E, GPIO_PORT_F,

	/* this must be last */
	GPIO_NR_PORTS,
};

#define GPIO_PA(n) 	(0*32 + n)
#define GPIO_PB(n) 	(1*32 + n)
#define GPIO_PC(n) 	(2*32 + n)
#define GPIO_PD(n) 	(3*32 + n)
#define GPIO_PE(n) 	(4*32 + n)
#define GPIO_PF(n) 	(5*32 + n)

struct jz_gpio_func_def {
	short port,func;
	unsigned long pins;
};

/* 
 * must define this array in board special file.
 * define the gpio pins in this array, use GPIO_DEF_END
 * as end of array. it will inited in function
 * setup_gpio_pins()
 */

extern struct jz_gpio_func_def platform_devio_array[];
extern int platform_devio_array_size;

/* 
 * This functions are used in special driver which need
 * operate the device IO pin.
 */
int jzgpio_set_func(enum gpio_port port,
		    enum gpio_function func,unsigned long pins);

int jzgpio_ctrl_pull(enum gpio_port port, int enable_pull,
		     unsigned long pins);

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

#endif
