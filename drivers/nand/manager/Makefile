#
# Makefile for nand manager
#

EXTRA_CFLAGS	+= -Idrivers/nand/manager/inc/ \
		-Idrivers/nand/inc/ \
		-Idrivers/nand/manager/utils/ \
		-Idrivers/nand/manager/inc/linux/ \
		-DLINUX_KERNEL

obj-y		+= utils/bufflistmanager.o utils/zonememory.o

obj-y		+= vnand/vNand.o

obj-y		+= simple/simpleblockmanager.o

obj-y		+= l2p/errhandle.o l2p/taskmanager.o l2p/hash.o l2p/hashnode.o l2p/recycle.o \
		l2p/zone.o l2p/zonemanager.o l2p/cachedata.o l2p/cachelist.o \
		l2p/cachemanager.o l2p/l2pconvert.o

obj-y		+= nandmanagerinterface.o
