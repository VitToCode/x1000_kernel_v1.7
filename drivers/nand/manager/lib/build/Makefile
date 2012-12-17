#
# Makefile for nand manager
#
BUILD_TYPE = kernel
#BUILD_TYPE = app

PUBDIR = ../..
TOPDIR = ..

CC = mips-linux-gnu-gcc
AR = mips-linux-gnu-ar
LD = mips-linux-gnu-ld
RM = rm -vf

INCLUDE = -I$(TOPDIR)/inc/ -I$(PUBDIR)/inc/

ARCHFLAG = -mips32 -mabi=32 -mno-abicalls -EL -O2 -G 0 -msoft-float

CFLAGS = $(INCLUDE) $(CONFIG) $(ARCHFLAG) \
	-isystem $(shell $(CC) -print-file-name=include) \
	-Wall -Wundef -Wstrict-prototypes -Wno-trigraphs \
	-Wdeclaration-after-statement -Wno-pointer-sign \
	-Wa,-mips32 -Wa,--trap -Wno-format-security \
	-Werror-implicit-function-declaration \
	-fno-strict-aliasing -fno-common -fno-pic \
	-fno-stack-protector -fomit-frame-pointer \
	-fno-strict-overflow -fno-delete-null-pointer-checks \
	-pipe -ffreestanding

ARFLAG = -rcs

# TOPDIR/os/kernel/
lib_src	= NandAlloc.c NandSemaphore.c NandThread.c clib.c
# TOPDIR/utils/
lib_src	+= bufflistmanager.c zonememory.c timerdebug.c ncrc16.c
# TOPDIR/zone_manager/
lib_src	+= errhandle.c taskmanager.c hash.c hashnode.c \
	recycle.c zone.c zonemanager.c cachedata.c \
	cachelist.c cachemanager.c l2pconvert.c junkzone.c \
	pageinfodebug.c
# TOPDIR/simple_manager/
lib_src	+= simpleblockmanager.c
# TOPDIR/spl_manager/
lib_src	+= splblockmanager.c
# TOPDIR/vnand/
lib_src	+= vNand.c l2vNand.c
# TOPDIR/
lib_src	+= nandmanagerinterface.c

MVPATH = $(TOPDIR)/os/$(BUILD_TYPE)/ \
	$(TOPDIR)/utils/ \
	$(TOPDIR)/zone_manager/ \
	$(TOPDIR)/simple_manager/ \
	$(TOPDIR)/spl_manager/ \
	$(TOPDIR)/vnand/ \
	$(TOPDIR)/

lib_obj = $(patsubst %.c, %.o, $(lib_src))

include ./Makefile.$(BUILD_TYPE)

VPATH = %.c:$(MVPATH)

%.o:%.c
	$(CC) $(CFLAGS) -o $@ -c $^

all: $(LIB)

clean:
	$(RM) *.a *.o