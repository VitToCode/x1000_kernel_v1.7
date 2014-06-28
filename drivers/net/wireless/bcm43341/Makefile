# bcmdhd
#
#
#
#
#

DHDCFLAGS = -DDBG=1 -Wall -Wstrict-prototypes -Dlinux -DBCMDRIVER               \
	-DBCMDONGLEHOST -DUNRELEASEDCHIP -DBCMDMA32 -DBCMFILEIMAGE            \
	-DDHDTHREAD -DDHD_DEBUG -DSDTEST -DBDC -DTOE                          \
	-DDHD_BCMEVENTS -DSHOW_EVENTS -DPROP_TXSTATUS -DBCMDBG                \
	-DBCMSDIO -DBCMLXSDMMC -DBCMPLATFORM_BUS -DWLP2P     \
	-DWIFI_ACT_FRAME -DARP_OFFLOAD_SUPPORT                                \
	-DKEEP_ALIVE -DPKT_FILTER_SUPPORT             \
#	-DGET_CUSTOM_MAC_ENABLE             \
#	-DENABLE_INSMOD_NO_FW_LOAD
#	-DEMBEDDED_PLATFORM -DPNO_SUPPORT          \

DHDCFLAGS +=	-DDHD_USE_IDLECOUNT -DSET_RANDOM_MAC_SOFTAP -DROAM_ENABLE -DVSDB      \
	-DWL_CFG80211_VSDB_PRIORITIZE_SCAN_REQUEST                            \
	-DESCAN_RESULT_PATCH -DHT40_GO -DPASS_ARP_PACKET -DSDIO_CRC_ERROR_FIX \
	-DDHD_DONOT_FORWARD_BCMEVENT_AS_NETWORK_PKT -DAMPDU_HOSTREORDER       \
	-DCUSTOM_SDIO_F2_BLKSIZE=128 -DSUPPORT_PM2_ONLY	-DWL_SDO -DWLTDLS     \
	-Idrivers/net/wireless/bcm43341	-Idrivers/net/wireless/bcm43341/include   \
#	-DCONFIG_BCMDHD_FW_PATH=\"/vendor/firmware/fw_43341.bin\"       \
#        -DCONFIG_BCMDHD_NVRAM_PATH=\"/vendor/firmware/nvram_43341.txt\" \
#	-DWL_SUPPORT_BACKPORTED_KPATCHES \ junyang marked
#	-Idrivers/net/wireless/bcmdhd	-Idrivers/net/wireless/bcmdhd/include   \
#	-Idrivers/net/wireless/bcmdhd/common/include

DHDOFILES = aiutils.o bcmsdh_sdmmc_linux.o dhd_linux.o siutils.o bcmutils.o   \
	dhd_linux_sched.o dhd_sdio.o bcmwifi_channels.o bcmevent.o hndpmu.o   \
	bcmsdh.o dhd_cdc.o bcmsdh_linux.o dhd_common.o linux_osl.o            \
	bcmsdh_sdmmc.o dhd_custom_gpio.o sbutils.o wldev_common.o wl_android.o

##ifeq ($(CONFIG_BCMDHD_WIFI_CONTROL_FUNC),y)
#DHDCFLAGS += -DCONFIG_WIFI_CONTROL_FUNC
##else
#DHDCFLAGS += -DCUSTOM_OOB_GPIO_NUM=2
##endif

#ifeq ($(CONFIG_BCMDHD_HW_OOB),y)
#DHDCFLAGS += -DCUSTOMER_HW2 -DHW_OOB -DOOB_INTR_ONLY -DMMC_SDIO_ABORT
#else
DHDCFLAGS += -DSDIO_ISR_THREAD
#endif
#Debug junyang
#CONFIG_BCMDHD=m
#CONFIG_BCMDHD=y
obj-$(CONFIG_BCM43341) += bcmdhd.o
bcmdhd-objs += $(DHDOFILES)
ifneq ($(CONFIG_WIRELESS_EXT),)
bcmdhd-objs += wl_iw.o
DHDCFLAGS += -DSOFTAP -DWL_WIRELESS_EXT -DUSE_IW
endif

ifeq ($(KBUILD_SRC),)
DHD_SRC = $(src)
else
DHD_SRC = $(KBUILD_SRC)/$(src)
endif
ifneq ($(wildcard $(DHD_SRC)/dhd_nvram.obj),)
include $(src)/makefile.my
endif

ifneq ($(CONFIG_CFG80211),)
bcmdhd-objs += wl_cfg80211.o wl_cfgp2p.o wl_linux_mon.o dhd_cfg80211.o
DHDCFLAGS += -DWL_CFG80211 
DHDCFLAGS += -DWL_CFG80211_STA_EVENT -DWL_ENABLE_P2P_IF
DHDCFLAGS += -DCUSTOM_ROAM_TRIGGER_SETTING=-65
DHDCFLAGS += -DCUSTOM_ROAM_DELTA_SETTING=15
DHDCFLAGS += -DCUSTOM_KEEP_ALIVE_SETTING=28000
DHDCFLAGS += -DCUSTOM_PNO_EVENT_LOCK_xTIME=7
endif
ifneq ($(CONFIG_DHD_USE_SCHED_SCAN),)
DHDCFLAGS += -DWL_SCHED_SCAN
endif
EXTRA_CFLAGS = $(DHDCFLAGS)
#DEBUG JUNYANG
ifeq ($(CONFIG_BCM43341),m)
#DHDCFLAGS += -I$(M) -I$(M)/include
DHDCFLAGS += -Idrivers/net/wireless/bcm43341 -Idrivers/net/wireless/bcm4341/include
EXTRA_LDFLAGS += --strip-debug
endif

#KERNEL_PATH?=/home/shlin//Desktop/My_ISSUE/IMX61/imx-android-13.4.1/myandroid/kernel_imx
#My_ARCH?=arm
#My_CROSS_COMPILE?=/home/shlin/Desktop/My_ISSUE/IMX61/imx-android-13.4.1/myandroid/prebuilt/linux-x86/toolchain/arm-eabi-4.4.3/bin/arm-eabi-
# mark by yangjun
#My_ARCH?=mips
#KERNEL_PATH?=/home/shlin/Desktop/My_ISSUE/ISSUE-Ingenic/Android-4.3_Source/mensa_4.3/kernel
#My_CROSS_COMPILE?=/home/shlin/Desktop/My_ISSUE/ISSUE-Ingenic/Android-4.3_Source/mensa_4.3/prebuilts/gcc/linux-x86/mips/mipsel-linux-android-4.6/bin/mipsel-linux-android-


#all:
#	make -C $(KERNEL_PATH) M=`pwd` ARCH=$(My_ARCH) CROSS_COMPILE=$(My_CROSS_COMPILE) modules
#clean:
#	make -C $(KERNEL_PATH) M=`pwd` ARCH=$(My_ARCH) CROSS_COMPILE=$(My_CROSS_COMPILE) clean
#	rm *.o

