/*
 * common.h
 */
#ifndef __COMMON_H__
#define __COMMON_H__

#include <linux/compiler.h>

#define MCU_TEST_INTER
#ifdef MCU_TEST_INTER
#define MCU_TEST_DATA 0xF40037C0  //TCSM_BANK7 - 0x40
#endif

#define __bank4		__section(.tcsm_bank4_1)
#define __bank5		__section(.tcsm_bank5_1)
#define __bank6		__section(.tcsm_bank6_1)
#define __bank7		__section(.tcsm_bank7_1)

#define TCSM_BANK0	0xF4000000
#define TCSM_BANK1	0xF4000800
#define TCSM_BANK2	0xF4001000
#define TCSM_BANK3	0xF4001800
#define TCSM_BANK4	0xF4002000
#define TCSM_BANK5	0xF4002800
#define TCSM_BANK6	0xF4003000
#define TCSM_BANK7	0xF4003800

#define NULL		0
#define ALIGN_ADDR_WORD(addr)	(void *)((((unsigned int)(addr) >> 2) + 1) << 2)

#define MCU_SOFT_IRQ		(1 << 2)
#define MCU_CHANNEL_IRQ		(1 << 1)
#define MCU_INTC_IRQ		(1 << 0)

typedef		char s8;
typedef	unsigned char	u8;
typedef		short s16;
typedef unsigned short	u16;
typedef		int s32;
typedef unsigned int	u32;

#endif /* __COMMON_H__ */
