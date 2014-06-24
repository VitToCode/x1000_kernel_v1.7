
obj-y += misc.o
obj-y += mmc.o
obj-$(CONFIG_REGULATOR_DA9024_V1_2) += regulator.o

obj-$(CONFIG_LCD_TRULY_TFT240240_2_E) += truly_tft240240.o
obj-$(CONFIG_BM347WV_F_8991FTGF_HX8369) += lcd-8991.o
obj-$(CONFIG_LCD_CV90_M5377_P30) += lcd-mirasol.o
obj-$(CONFIG_LCD_OTM8012A_TFT1P6449) += lcd-otm8012a.o
obj-$(CONFIG_LCD_HX8389_B11_G) += lcd-hx8389_b11_g.o
obj-$(CONFIG_LCD_LH155) += lcd-lh155.o

obj-$(CONFIG_SOUND_OSS_XBURST_V1_2)		    += sound.o
obj-$(CONFIG_VIDEO_OVISP) += camera.o

obj-$(CONFIG_BCM_PM_CORE) += bcm_power_control.o

ifdef CONFIG_SLCD_SUSPEND_ALARM_WAKEUP_REFRESH
obj-y += pm_slcd.o
else
obj-y += pm.o
endif
