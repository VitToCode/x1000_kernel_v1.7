/*
 * act8600-private.h - Head file for PMU ACT8600.
 *
 * Copyright (C) 2012 Ingenic Semiconductor Co., Ltd.
 * Author: Large Dipper <ykli@ingenic.com>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_MFD_ACT8600_PRIV_H
#define __LINUX_MFD_ACT8600_PRIV_H



#define VCON_OK				(1 << 0)
#define VCON_DIS			(1 << 2)
#define VCON_ON				(1 << 7)

#define ACT8600_APCH_INTR0	0xa1
	#define ACT8600_APCH_INTR0_SUS		(1 << 7)
#define ACT8600_APCH_INTR1	0xa8
	#define ACT8600_APCH_INTR1_CHGDAT 	(1 << 0)
	#define ACT8600_APCH_INTR1_INDAT 	(1 << 1)
	#define ACT8600_APCH_INTR1_TEMPDAT 	(1 << 2)
	#define ACT8600_APCH_INTR1_TIMRDAT 	(1 << 3)
	#define ACT8600_APCH_INTR1_CHGSTAT 	(1 << 4)
	#define ACT8600_APCH_INTR1_INSTAT 	(1 << 5)
	#define ACT8600_APCH_INTR1_TEMPSTAT (1 << 6)
	#define ACT8600_APCH_INTR1_TIMRSTAT (1 << 7)

		
#define ACT8600_APCH_INTR2	0xa9
	#define ACT8600_APCH_INTR2_CHGEOCOUT 	(1 << 0)
	#define ACT8600_APCH_INTR2_INDIS 		(1 << 1)
	#define ACT8600_APCH_INTR2_TEMPOUT 		(1 << 2)
	#define ACT8600_APCH_INTR2_TIMRPRE 		(1 << 3)
	#define ACT8600_APCH_INTR2_CHGEOCIN 	(1 << 4)
	#define ACT8600_APCH_INTR2_INCON 		(1 << 5)
	#define ACT8600_APCH_INTR2_TEMPIN 		(1 << 6)
	#define ACT8600_APCH_INTR2_TIMRTOT 		(1 << 7)


	
#define ACT8600_APCH_STAT	0xaa
	#define ACT8600_APCH_STAT_STATE_MASK	(0x30)
	#define ACT8600_APCH_STAT_STATE_PRE	(0x30)
	#define ACT8600_APCH_STAT_STATE_CHAGE	(0x20)
	#define ACT8600_APCH_STAT_STATE_EOC	(0x10)
	#define ACT8600_APCH_STAT_STATE_SUSPEND	(0x00)

#define ACT8600_OTG_CON		0xb0
	#define ACT8600_OTG_CON_Q1		(1 << 7)
	#define ACT8600_OTG_CON_Q2		(1 << 6)
	#define ACT8600_OTG_CON_Q3		(1 << 5)
	#define ACT8600_OTG_CON_VBUSSTAT (1 << 2)
	#define ACT8600_OTG_CON_DBLIMITQ3	(1 << 1)
	#define ACT8600_OTG_CON_VBUSDAT		(1 << 0)
#define ACT8600_OTG_INTR	0xb2
	#define ACT8600_OTG_INTR_INVBUSR	((1 << 7) | 0x3)
	#define ACT8600_OTG_INTR_INVBUSF	((1 << 6) | 0x3)
	#define ACT8600_OTG_INTR_nVBUSMSK	((1 << 1) | 0x3)
	

#define ACT8600_SYS0		0x00
#define ACT8600_SYS1		0x01


struct act8600 {
	struct device *dev;
	struct i2c_client *client;
};

extern int act8600_read_reg(struct i2c_client *client, unsigned char reg, unsigned char *val);
extern int act8600_write_reg(struct i2c_client *client, unsigned char reg, unsigned char val);

#endif /* __LINUX_MFD_ACT8600_PRIV_H */
