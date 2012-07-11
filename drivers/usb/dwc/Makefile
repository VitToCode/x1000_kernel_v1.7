
# Makefile for DWC_otg Highspeed USB controller driver
#
# Use one of the following flags to compile the software in host-only or
# device-only mode.
ifeq ($(CONFIG_USB_DWC_DEV_ONLY), y)
	EXTRA_CFLAGS        += -DDWC_DEVICE_ONLY
endif
ifeq ($(CONFIG_USB_DWC_HOST_ONLY), y)
	EXTRA_CFLAGS        += -DDWC_HOST_ONLY
endif

EXTRA_CFLAGS    += -I$(PWD)/drivers/usb/dwc/dwc_common_port
EXTRA_CFLAGS   	+= -DLM_INTERFACE
EXTRA_CFLAGS    += -DDWC_LINUX

obj-$(CONFIG_USB_DWC_OTG)	+= dwc_common_port/

dwc_otg-objs	:= dwc_otg_driver.o dwc_otg_attr.o 
dwc_otg-objs	+= dwc_otg_cil.o dwc_otg_cil_intr.o 
dwc_otg-objs	+= dwc_otg_pcd_linux.o dwc_otg_pcd.o dwc_otg_pcd_intr.o 
dwc_otg-objs	+= dwc_otg_hcd.o dwc_otg_hcd_linux.o dwc_otg_hcd_intr.o dwc_otg_hcd_queue.o dwc_otg_hcd_ddma.o
dwc_otg-objs	+= dwc_otg_adp.o


obj-$(CONFIG_USB_DWC_OTG)	+= dwc_otg.o


#EXTRA_CFLAGS	+= -Dlinux -DDWC_HS_ELECT_TST
#EXTRA_CFLAGS	+= -DDWC_EN_ISOC
#EXTRA_CFLAGS  	+= $(CFI)
#EXTRA_CFLAGS	+= -DDWC_DEV_SRPCAP

ifneq ($(CFI),)
dwc_otg-objs	+= dwc_otg_cfi.o
endif
