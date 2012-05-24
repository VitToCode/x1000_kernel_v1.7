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
	UART2_PORTC,
	MSC0_PORTA,
	MSC1_PORTD,
	MSC2_PORTE,
	I2C0_PORTD,
	I2C1_PORTE,
	I2C2_PORTF,
	LCD_PORTC,
	PWM1_PORTE,
	MII_PORTBDF,
};

int platform_devio_array_size = ARRAY_SIZE(platform_devio_array);


