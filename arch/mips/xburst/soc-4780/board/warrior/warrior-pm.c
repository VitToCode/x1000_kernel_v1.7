/*
 * Copyright (c) 2006-2010  Ingenic Semiconductor Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <gpio.h>

// default gpio state is input pull;
__initdata int gpio_ss_table[][2] = {
	{32 * 0 +  0,	GSS_INPUT_NOPULL}, /* NC */
	{32 * 0 +  1,	GSS_INPUT_PULL	}, /* ACC_INT2 input pull*/
	{32 * 0 +  2,	GSS_IGNORE	}, /* FVDD_EN */
	{32 * 0 +  3,	GSS_INPUT_NOPULL}, /* SCLK */
	{32 * 0 +  4,	GSS_INPUT_NOPULL}, /* NC */
	{32 * 0 +  5,	GSS_INPUT_NOPULL}, /* NC */
	{32 * 0 +  6,	GSS_INPUT_NOPULL}, /* PA6/SDATA */
	{32 * 0 +  7,	GSS_INPUT_NOPULL}, /* NC */
	{32 * 0 + 16,	GSS_INPUT_PULL	}, /* DC_DETE */
	{32 * 0 + 17,	GSS_INPUT_NOPULL}, /* OTG_POWER_EN */
	{32 * 0 + 18,	GSS_IGNORE	}, /* NF_RE/MSC0_CLK */
	{32 * 0 + 19,	GSS_IGNORE	}, /* NF_WE/MSC0_CMD */
	{32 * 0 + 20,	GSS_IGNORE	}, /* NF_RB/MSC0_D0 */
	{32 * 0 + 21,	GSS_IGNORE	}, /* NF_CS1/MSC0_D1 */
	{32 * 0 + 22,	GSS_IGNORE	}, /* NF_CS2/MSC0_D2 */
	{32 * 0 + 23,	GSS_IGNORE	}, /* NF_CS3/MSC0_D3 */
	{32 * 0 + 24,	GSS_INPUT_PULL	}, /* NC */
	{32 * 0 + 25,	GSS_INPUT_PULL	}, /* NC */
	{32 * 0 + 26,	GSS_INPUT_PULL  }, /* NC */
	{32 * 0 + 27,	GSS_INPUT_PULL 	}, /* WAIT_N */
	{32 * 0 + 28,	GSS_IGNORE	}, /* PMU_IRQ_N */
	{32 * 0 + 29,	GSS_IGNORE	}, /* NC */
	{32 * 0 + 30,	GSS_IGNORE	}, /* WAKE_UP*/

	/* GPIO Group - B */
	{32 * 1 +  0,	GSS_IGNORE	}, /* SA0_CLE */
	{32 * 1 +  1,	GSS_IGNORE	}, /* SA1_ALE */
	{32 * 1 +  2,	GSS_IGNORE	}, /* CHARG_CURENT_SET */
	{32 * 1 +  3,	GSS_IGNORE	}, /* CHARG_DET */
	{32 * 1 +  4,	GSS_INPUT_NOPULL}, /* KEY0 */
	{32 * 1 +  5,	GSS_INPUT_NOPULL}, /* KEY1 */
	{32 * 1 +  6,	GSS_OUTPUT_LOW	}, /* CIM_PCLK */
	{32 * 1 +  7,	GSS_OUTPUT_LOW	}, /* CIM_HS */
	{32 * 1 +  8,	GSS_OUTPUT_LOW	}, /* CIM_VS */
	{32 * 1 +  9,	GSS_OUTPUT_LOW	}, /* CIM_MCLK */
	{32 * 1 + 10,	GSS_OUTPUT_LOW	}, /* CIM_D0 */
	{32 * 1 + 11,	GSS_OUTPUT_LOW	}, /* CIM_D1 */
	{32 * 1 + 12,	GSS_OUTPUT_LOW	}, /* CIM_D2 */
	{32 * 1 + 13,	GSS_OUTPUT_LOW	}, /* CIM_D3 */
	{32 * 1 + 14,	GSS_OUTPUT_LOW	}, /* CIM_D4 */
	{32 * 1 + 15,	GSS_OUTPUT_LOW	}, /* CIM_D5 */
	{32 * 1 + 16,	GSS_OUTPUT_LOW	}, /* CIM_D6 */
	{32 * 1 + 17,	GSS_OUTPUT_LOW	}, /* CIM_D7 */
	{32 * 1 + 18,   GSS_OUTPUT_LOW	}, /* PWDN_0.3M */
	{32 * 1 + 19,	GSS_OUTPUT_LOW	}, /* PWDN_2M */
	{32 * 1 + 22,	GSS_INPUT_NOPULL}, /* LCD_RESET_EN */
	{32 * 1 + 23,	GSS_OUTPUT_LOW	}, /* LCD_VCC_EN */
	{32 * 1 + 25,	GSS_OUTPUT_LOW	}, /* MOTOR_EN */
	{32 * 1 + 26,	GSS_OUTPUT_LOW	}, /* CIM_RST */
	{32 * 1 + 27,	GSS_OUTPUT_LOW	}, /* CIM_VCC_EN */

	/* GPIO Group - C */
	{32 * 2 +  0,	GSS_OUTPUT_LOW	}, /* LCD_B0 */
	{32 * 2 +  1,	GSS_OUTPUT_LOW	}, /* LCD_B1*/
	{32 * 2 +  2,	GSS_OUTPUT_LOW	}, /* LCD_B2 */
	{32 * 2 +  3,	GSS_OUTPUT_LOW	}, /* LCD_B3 */
	{32 * 2 +  4,	GSS_OUTPUT_LOW	}, /* LCD_B4 */
	{32 * 2 +  5,	GSS_OUTPUT_LOW	}, /* LCD_B5 */
	{32 * 2 +  6,	GSS_OUTPUT_LOW	}, /* LCD_B6 */
	{32 * 2 +  7,	GSS_OUTPUT_LOW	}, /* LCD_B7 */
	{32 * 2 +  8,	GSS_OUTPUT_LOW	}, /* LCD_PCLK */
	{32 * 2 +  9,	GSS_OUTPUT_LOW	}, /* LCD_DE */
	{32 * 2 + 10,	GSS_OUTPUT_LOW	}, /* LCD_G0 */
	{32 * 2 + 11,	GSS_OUTPUT_LOW	}, /* LCD_G1 */
	{32 * 2 + 12,	GSS_OUTPUT_LOW	}, /* LCD_G2 */
	{32 * 2 + 13,	GSS_OUTPUT_LOW	}, /* LCD_G3 */
	{32 * 2 + 14,	GSS_OUTPUT_LOW	}, /* LCD_G4 */
	{32 * 2 + 15,	GSS_OUTPUT_LOW	}, /* LCD_G5 */
	{32 * 2 + 16,	GSS_OUTPUT_LOW	}, /* LCD_G6 */
	{32 * 2 + 17,	GSS_OUTPUT_LOW	}, /* LCD_G7 */
	{32 * 2 + 18,	GSS_OUTPUT_LOW 	}, /* LCD_HS */
	{32 * 2 + 19,	GSS_OUTPUT_LOW 	}, /* LCD_VS */
	{32 * 2 + 20,	GSS_OUTPUT_LOW 	}, /* LCD_R0 */
	{32 * 2 + 21,	GSS_OUTPUT_LOW 	}, /* LCD_R1 */
	{32 * 2 + 22,	GSS_OUTPUT_LOW 	}, /* LCD_R2 */
	{32 * 2 + 23,	GSS_OUTPUT_LOW 	}, /* LCD_R3 */
	{32 * 2 + 24,	GSS_OUTPUT_LOW 	}, /* LCD_R4 */
	{32 * 2 + 25,	GSS_OUTPUT_LOW	}, /* LCD_R5 */
	{32 * 2 + 26,	GSS_OUTPUT_LOW	}, /* LCD_R6 */
	{32 * 2 + 27,	GSS_OUTPUT_LOW	}, /* LCD_R7 */

	/* GPIO Group - D */
	{32 * 3 +  0,	GSS_OUTPUT_LOW  }, /* WL_PCM_DO */
	{32 * 3 +  1,	GSS_IGNORE	}, /* NC */
	{32 * 3 +  2,	GSS_IGNORE	}, /* NC */
	{32 * 3 +  3,	GSS_OUTPUT_LOW	}, /* WL_PCM_DI */
	{32 * 3 +  4,	GSS_INPUT_NOPULL}, /* PS2_MCLK */
	{32 * 3 +  5,	GSS_INPUT_NOPULL}, /* PS2_MDATA */
	{32 * 3 +  6,	GSS_INPUT_NOPULL}, /* PS2_KCLK */
	{32 * 3 +  7,	GSS_INPUT_NOPULL}, /* PS2_KDATA */
	{32 * 3 +  8,	GSS_INPUT_NOPULL}, /* NC */
	{32 * 3 +  9,	GSS_OUTPUT_LOW	}, /* NC */
	{32 * 3 + 10,	GSS_INPUT_NOPULL}, /* I2C3_SDA */
	{32 * 3 + 11,	GSS_INPUT_NOPULL}, /* I2C3_SCK */
	{32 * 3 + 13,	GSS_OUTPUT_HIGH	}, /* HDMI_I2S_WS  */
	{32 * 3 + 14,	GSS_IGNORE	}, /* CLK32 */
	{32 * 3 + 15,	GSS_INPUT_NOPULL}, /* NC */
	{32 * 3 + 17,	GSS_INPUT_NOPULL}, /* BOOT_SEL0/VOL- */
	{32 * 3 + 18,	GSS_INPUT_NOPULL}, /* BOOT_SEL1/VOL+ */
	{32 * 3 + 19,	GSS_INPUT_NOPULL}, /* BOOT_SEL2 */
	{32 * 3 + 20,	GSS_INPUT_NOPULL}, /* WL_MSC1_D0 */
	{32 * 3 + 21,	GSS_INPUT_NOPULL}, /* WL_MSC1_D1 */
	{32 * 3 + 22,	GSS_INPUT_NOPULL}, /* WL_MSC1_D2*/
	{32 * 3 + 23,	GSS_INPUT_NOPULL}, /* WL_MSC1_D3 */
	{32 * 3 + 24,	GSS_INPUT_NOPULL}, /* WL_MSC1_CLK */
	{32 * 3 + 25,	GSS_INPUT_NOPULL}, /* WL_MSC1_CMD */
	{32 * 3 + 30,	GSS_INPUT_NOPULL}, /* I2C0_SDA */
	{32 * 3 + 31,	GSS_INPUT_NOPULL}, /* I2C0_SCK */

	/* GPIO Group - E */
	{32 * 4 +  0,	GSS_OUTPUT_LOW	}, /* NC */
	{32 * 4 +  1,	GSS_INPUT_NOPULL}, /* LCD_PWM/LED_EN */
	{32 * 4 +  2,	GSS_INPUT_NOPULL}, /* ID */
	{32 * 4 +  3,	GSS_IGNORE	}, /* SD0_VCC_SW_EN */
	{32 * 4 +  4,	GSS_INPUT_NOPULL}, /* I2C4_SCK */
	{32 * 4 +  6,	GSS_OUTPUT_LOW	}, /* AMPEN */
	{32 * 4 +  7,	GSS_INPUT_NOPULL}, /* HDMI_I2S_D0 */
	{32 * 4 + 10,	GSS_OUTPUT_LOW	}, /* DRVVBUS */
	{32 * 4 + 12,	GSS_INPUT_NOPULL}, /* TP_DRV_EN */
	{32 * 4 + 13,	GSS_INPUT_NOPULL}, /*  */
	{32 * 4 + 14,	GSS_INPUT_NOPULL}, /* NC */
	{32 * 4 + 15,	GSS_INPUT_NOPULL}, /* NC */
	{32 * 4 + 16,	GSS_INPUT_NOPULL}, /* NC */
	{32 * 4 + 17,	GSS_OUTPUT_LOW	}, /* NC */
	{32 * 4 + 18,	GSS_INPUT_NOPULL}, /* WL_RST_N */
	{32 * 4 + 19,	GSS_IGNORE	}, /* PMU_IRQ_N */
	{32 * 4 + 20,	GSS_INPUT_NOPULL}, /* NC */
	{32 * 4 + 21,	GSS_INPUT_NOPULL}, /* NC */
	{32 * 4 + 22,	GSS_INPUT_NOPULL}, /* NC */
	{32 * 4 + 23,	GSS_INPUT_NOPULL}, /* NC */
	{32 * 4 + 28,	GSS_IGNORE	}, /* NC */
	{32 * 4 + 29,	GSS_IGNORE	}, /* NC */
	{32 * 4 + 30,	GSS_INPUT_NOPULL}, /* I2C1_SDA */
	{32 * 4 + 31,	GSS_INPUT_NOPULL}, /* I2C1_SCK */

	/* GPIO Group - F */
	{32 * 5 +  4,	GSS_INPUT_PULL	}, /* BT_REG_ON */
	{32 * 5 +  5,	GSS_INPUT_PULL	}, /* BT_WAKE */
	{32 * 5 +  6,	GSS_INPUT_PULL	}, /* BT_INT */
	{32 * 5 +  7,	GSS_OUTPUT_LOW  }, /* WL_REG_ON */
	{32 * 5 +  8,	GSS_INPUT_PULL	}, /* BT_RST_N */
	{32 * 5 +  9,	GSS_OUTPUT_LOW	}, /* SENSOR_INT1 */
	{32 * 5 + 10,	GSS_OUTPUT_LOW	}, /* SENSOR_INT2 */
	{32 * 5 + 13,	GSS_INPUT_NOPULL}, /* USB_DETE */
	{32 * 5 + 16,	GSS_INPUT_NOPULL}, /* I2C2_SDA */
	{32 * 5 + 17,	GSS_INPUT_NOPULL}, /* I2C2_SCK */
	{32 * 5 + 18,	GSS_OUTPUT_LOW	}, /* CTP_WAKE_UP */
	{32 * 5 + 19,	GSS_OUTPUT_LOW	}, /*  */
	{32 * 5 + 20,	GSS_INPUT_NOPULL}, /* SD0_CD_N */
	{32 * 5 + 21,	GSS_OUTPUT_LOW	}, /* SD0_WP_N */
	{32 * 5 + 22,	GSS_OUTPUT_LOW	}, /* FWP_N */
	{32 * 5 + 23,	GSS_OUTPUT_LOW	}, /*HDMI_CEC *///HIGH
	{32 * 5 + 24,	GSS_INPUT_NOPULL}, /*HDMI_SCL*/
	{32 * 5 + 25,	GSS_INPUT_NOPULL}, /*HDMI_SDA*/

	/* GPIO Group Set End */
	{GSS_TABLET_END,GSS_TABLET_END	}
};

