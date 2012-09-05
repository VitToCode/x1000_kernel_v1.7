
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
obj-y += pm.o
obj-y += platform.o
obj-$(CONFIG_CPUFRQU) += cpufreq.o

obj-$(CONFIG_DWC_LM_INTERFACE)  += lm.o lm-device.o
