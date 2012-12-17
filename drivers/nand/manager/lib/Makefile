#
# Makefile for nand manager
#

EXTRA_CFLAGS	+= -Idrivers/nand/manager/inc/ \
		-Idrivers/nand/manager/lib/inc/ \

obj-y		+= os/kernel/NandAlloc.o os/kernel/NandSemaphore.o os/kernel/NandThread.o os/kernel/clib.o

obj-y		+= utils/bufflistmanager.o utils/zonememory.o utils/timerdebug.o utils/ncrc16.o

obj-y		+= vnand/vNand.o vnand/l2vNand.o

obj-y		+= simple_manager/simpleblockmanager.o

obj-y		+= spl_manager/splblockmanager.o

obj-y		+= zone_manager/errhandle.o zone_manager/taskmanager.o zone_manager/hash.o \
		zone_manager/hashnode.o zone_manager/recycle.o zone_manager/zone.o \
		zone_manager/zonemanager.o zone_manager/cachedata.o zone_manager/cachelist.o \
		zone_manager/cachemanager.o zone_manager/l2pconvert.o zone_manager/junkzone.o zone_manager/pageinfodebug.o

obj-y		+= nandmanagerinterface.o
