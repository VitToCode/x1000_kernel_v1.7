STATIC=1

TOPDIR=../..
LIBPATH=lib
CFLAGS=-Wall -static -pthread -lrt -ldl -DALLOC_DEBUG -DNO_ERROR -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Wno-format-security -fno-delete-null-pointer-checks -fno-stack-protector -fomit-frame-pointer -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow
LDFLAGS=-I$(LIBPATH) -L$(LIBPATH) -lfunc 

ifeq ($(STATIC), 1)
LIB=$(LIBPATH)/libnm.a
else
LIB=$(LIBPATH)/libnm.so
CFLAGS+=-fPIC
endif

#TARGET=main
#SRC=main.c

LIBSRC = vNand.c \
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
	 NandSemaphore.c  \
	 NandThread.c \
	 nandalloc.c \
	 junkzone.c \
	 errhandle.c \
	 badblockinfo.c	


MVPATH= $(TOPDIR)/manager/     \
	$(TOPDIR)/manager/vnand \
	$(TOPDIR)/manager/utils \
	$(TOPDIR)/manager/l2p \
	$(TOPDIR)/manager/simple/  \
	$(TOPDIR)/misc/testmodule/app

VPATH = %.c:$(MVPATH)

INC=$(TOPDIR)/inc $(TOPDIR)/manager/inc $(TOPDIR)/misc/testmodule/inc $(MVPATH)

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
