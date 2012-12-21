/*
 * LCD driver data for hhx070ml208cp21
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _HHX070ML208CP21_H
#define _HHX070ML208CP21_H

/**
 * @gpio_rest: global reset pin, active low to enter reset state
 */
struct platform_hhx070ml208cp21_data {
	unsigned int gpio_rest;
    void (*notify_on)(int on);
};

#endif /* _HHX070ML208CP21_H */
