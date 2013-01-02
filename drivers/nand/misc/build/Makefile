STATIC=1

TOPDIR=../..
LIBPATH=lib
CFLAGS=-Wall -static -pthread -lrt -ldl -DALLOC_DEBUG -DNO_ERROR -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Wno-format-security -fno-delete-null-pointer-checks -fno-stack-protector -fomit-frame-pointer -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow -m32
LDFLAGS=-I$(LIBPATH) -L$(LIBPATH) -lfunc

ifeq ($(STATIC), 1)
LIB=$(LIBPATH)/libnm.a
else
LIB=$(LIBPATH)/libnm.so
CFLAGS+=-fPIC
endif

#TARGET=main
#SRC=main.c

LIBSRC = l2vNand.c \
	 vNand.c \
	 bufflistmanager.c \
	 zonememory.c \
	 hash.c  \
	 hashnode.c  \
	 recycle.c  \
	 zone.c  \
	 zonemanager.c \
	 taskmanager.c  \
	 cachedata.c  \
	 cachelist.c  \
	 cachemanager.c  \
	 l2pconvert.c    \
	 nandmanagerinterface.c  \
	 simpleblockmanager.c    \
	 splblockmanager.c    \
	 NandAlloc.c \
	 NandSemaphore.c  \
	 NandThread.c \
	 clib.c \
	 junkzone.c \
	 errhandle.c \
         pageinfodebug.c \
	 ncrc16.c	\
	ndfifo.c
#	 badblockinfo.c


MVPATH= $(TOPDIR)/manager/lib     \
	$(TOPDIR)/manager/lib/vnand \
	$(TOPDIR)/manager/lib/zone_manager \
	$(TOPDIR)/manager/lib/simple_manager \
	$(TOPDIR)/manager/lib/spl_manager \
	$(TOPDIR)/manager/lib/utils \
	$(TOPDIR)/manager/lib/os/app

VPATH = %.c:$(MVPATH)

INC= $(TOPDIR)/manager/inc $(TOPDIR)/manager/lib/inc $(TOPDIR)/misc/testmodule/inc $(MVPATH)

INC_H := $(foreach subdir,$(INC),$(wildcard $(subdir)/*.h))

MOBJS = $(LIBSRC:.c=.o)

OBJS = $(MOBJS)

CFLAGS+=$(addprefix -I,$(INC))

all: $(LIB) $(OBJS) $(INC_H)

lib: $(LIB)

%.o:%.c
	$(CC) $(CFLAGS) -o $@ -c $<

ifeq ($(STATIC), 1)
$(LIB): $(OBJS)
	$(AR) -rcs $@ $^
else

$(LIB): $(OBJS)
	$(CC) -shared -o $@ $^
endif

#$(TARGET): $(SRC)
#    $(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	find $(TOPDIR) -name "*.o" | xargs rm -vf;
	find $(TOPDIR) -name "*.o.cmd" | xargs rm -vf;
	find $(TOPDIR) -name "*.o.d" | xargs rm -vf;
	find $(TOPDIR) -name "*~" | xargs rm -vf;
	rm -f $(TARGET) $(LIB) $(OBJS)
