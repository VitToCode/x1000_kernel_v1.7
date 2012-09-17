
obj-y += setup.o 

ifeq ($(CONFIG_FPGA_TEST),y)
obj-y += clk_fpga.o
else
obj-y += clk.o
endif

obj-y += reset.o 
obj-y += time.o 
obj-y += serial.o 
obj-y += irq.o 
obj-y += gpio.o 
obj-y += pwm.o tcu.o
ifeq ($(CONFIG_PM_POWERDOWN_P0),y)
obj-y += pm_p0.o
else
obj-y += pm.o
endif
obj-y += platform.o
obj-$(CONFIG_CPUFRQU) += cpufreq.o

obj-$(CONFIG_DWC_LM_INTERFACE)  += lm.o lm-device.o
