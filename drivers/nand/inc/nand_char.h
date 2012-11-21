/*
 * nand/inc/nand_dug.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) Ingenic Semiconductor Co., Ltd.
 */

#ifndef __NAND_CHAR_H__
#define __NAND_CHAR_H__

enum nand_char_cmd {
		CMD_PARTITION_ERASE = 98,
		CMD_ERASE_ALL = 99,
};

int Register_NandCharDriver(unsigned int interface,unsigned int partarray);
#endif /* __NAND_CHAR_H__ */
