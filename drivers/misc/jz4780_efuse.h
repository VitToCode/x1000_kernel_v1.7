/*
 * linux/include/asm-mips/mach-jz4780/jz4780efuse.h
 *
 * JZ4780 DDRC register definition.
 *
 * Copyright (C) 2010 Ingenic Semiconductor Co., Ltd.
 */

#ifndef __JZ4780EFUSE_H__
#define __JZ4780EFUSE_H__

#define EFUSE_CTRL       0xB34100D0
#define EFUSE_CFG        0xB34100D4
#define EFUSE_STATE      0xB34100D8
#define EFUSE_DATA_BASE  0xB34100DC

#define EFUSE_DATA(n)   (EFUSE_DATA_BASE + (n)*4)

#define REG_EFUSE_CTRL      REG32(EFUSE_CTRL)
#define REG_EFUSE_CFG       REG32(EFUSE_CFG)
#define REG_EFUSE_STATE     REG32(EFUSE_STATE)
#define REG_EFUSE_DATA(n)   REG32(EFUSE_DATA(n))

#define RD_ADJ 15
#define RD_STROBE 7
#define WR_ADJ 1
#define WR_STROBE  333

/* EFUSE Status Register  (OTP_STATE) */
#define EFUSE_STATE_GLOBAL_PRT            (1 << 15)
#define EFUSE_STATE_CHIPID_PRT            (1 << 14)
#define EFUSE_STATE_CUSTID_PRT            (1 << 13)
#define EFUSE_STATE_SECWR_EN              (1 << 12)
#define EFUSE_STATE_PC_PRT                (1 << 11)
#define EFUSE_STATE_HDMIKEY_PRT           (1 << 10)
#define EFUSE_STATE_SECKEY_PRT            (1 << 9)
#define EFUSE_STATE_SECBOOT_EN            (1 << 8)
#define EFUSE_STATE_HDMI_BUSY             (1 << 2)
#define EFUSE_STATE_WR_DONE               (1 << 1)
#define EFUSE_STATE_RD_DONE               (1 << 0)
/*EFUSE PROTECT BIT*/
#define GLOBAL_PRT                      (1 << 7)
#define CHIPID_PRT                      (1 << 6)
#define CUSTID_PRT                      (1 << 5)
#define SECWR_EN                        (1 << 4)
#define PC_PRT                          (1 << 3)
#define HDMIKEY_PRT                     (1 << 2)
#define SECKEY_PRT                      (1 << 1)
#define SECBOOT_EN                      (1 << 0)
#endif
