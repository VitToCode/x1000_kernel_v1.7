/*
 * nand/inc/nand_dug.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Copyright (c) Ingenic Semiconductor Co., Ltd.
 */

#ifndef __NAND_DUG_H__
#define __NAND_DUG_H__

struct nand_dug_msg{
        unsigned char name[20];
        unsigned int  byteperpage;
        unsigned int  pageperblock;
        unsigned int  totalblocks;
};

enum nand_dug_cmd{
        GET_NAND_PTC,  // the count of nand's partition
        GET_NAND_MSG,
        NAND_DUG_READ,
        NAND_DUG_WRITE,
        NAND_DUG_ERASE
};

struct NandInfo{
        int id;
        int bytes;
        int partnum;
        unsigned char *data;
};

#endif /* __NAND_DUG_H__ */
