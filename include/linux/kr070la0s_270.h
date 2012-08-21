/*
 * LCD driver data for KR070LA0S_270
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _KR070LA0S_270_H
#define _KR070LA0S_270_H

/**
 * @gpio_lr: scan direction, 0: right to left, 1: left to right
 * @gpio_ud: scan direction, 0: top to bottom, 1: bottom to top
 * @gpio_selb: mode select, H: 6bit, L: 8bit
 * @gpio_stbyb: standby mode, normally pull high. 1: normal operation
 * @gpio_rest: global reset pin, active low to enter reset state
 */
struct platform_kr070la0s_270_data {
	unsigned int gpio_lr;
	unsigned int gpio_ud;
	unsigned int gpio_selb;
	unsigned int gpio_stbyb;
	unsigned int gpio_rest;
};

#endif /* _KR070LA0S_270_H */
