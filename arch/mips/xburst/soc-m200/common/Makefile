## WARNING! DO NOT change this lists order because power sleep order depends on it!! ##

obj-y += setup.o
obj-y += reset.o
obj-y += timer.o

obj-y += serial.o
obj-y += tcu.o

obj-y += gpio.o
# only for debug
#obj-y += pmmon.o
obj-y += irq.o

obj-y += platform.o

obj-y += clk/

obj-y += jz_big_little_switch.o cpu_switch.o


ifeq ($(CONFIG_PM_POWERDOWN_P0),y)
obj-y += pm_p0.o
else
obj-y += pm.o
endif

obj-$(CONFIG_USB_SUPPORT) += cpm_usb.o

obj-$(CONFIG_CPU_FREQ) += cpufreq.o

ifeq ($(CONFIG_CPU_FREQ),y)
obj-$(CONFIG_JZ_BIG_LITTLE_SWITCH) += jz_big_little_switch.o cpu_switch.o
endif

obj-$(CONFIG_DWC_LM_INTERFACE)  += lm.o lm-device.o
