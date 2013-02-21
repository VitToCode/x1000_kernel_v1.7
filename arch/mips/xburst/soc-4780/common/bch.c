/*
 *  Copyright (C) 2013 Fighter Sun <wanmyqawdr@126.com>
 *  HW ECC-BCH support functions
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>

static int __init bch_init(void)
{
	return 0;
}
postcore_initcall(bch_init);

