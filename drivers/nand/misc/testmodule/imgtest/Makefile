CC = gcc
CXX = g++
LIB     = -ldl -lrt  -g
INCLUDE = -I../inc -I../../../inc -I../../../manager/inc/   -I../../../manager/utils

#test ZONE_MANAGER
CFLAGS  = -Wall -pipe -DDEBUG -D_NEW_LIC -g -D_GNU_SOURCE -shared -D_REENTRANT -DTEST_ZONE_MANAGER

#test DIRECT_MANAGER
#CFLAGS  = -Wall -pipe -DDEBUG -D_NEW_LIC -g -D_GNU_SOURCE -shared -D_REENTRANT

OBJS    = imgtest.c ../../../manager/l2p/taskmanager.c ../app/NandSemaphore.c ../app/NandThread.c ../../../manager/l2p/zone.c ../../../manager/l2p/zonemanager.c ../../../manager/l2p/recycle.c ../../../manager/l2p/hashnode.c ../../../manager/l2p/hash.c ../../../manager/l2p/cachedata.c ../../../manager/l2p/cachelist.c ../../../manager/l2p/cachemanager.c ../../../manager/l2p/l2pconvert.c ../../../manager/vnand/vNand.c ../../../manager/utils/bufflistmanager.c ../../driver/src/APP_vNand.c ../../../manager/nandmanagerinterface.c ../../../manager/simple/simpleblockmanager.c ../../../manager/utils/zonememory.c ../../../manager/l2p/errhandle.c ../../../manager/l2p/junkzone.c ../../../manager/vnand/l2vNand.c

TARGETS = ../imgtest.so 
all: $(TARGETS)
$(TARGETS): $(OBJS)
	$(CC) $(CFLAGS) $(INCLUDE) $(OBJS) -o $@ $(LIBDIR) $(LIB)
.c.o:
	$(CC)  $(CFLAGS) -c $(INCLUDE) $<

.cpp.o:
	$(CXX) $(CFLAGS) -c $(INCLUDE) $<
#%:%.c
#    $(CC) $(CFLAGS) -o $@ $< $(OO) $(LDFLAGS)
#    echo $@
clean:
	rm -f *.o
	rm -f $(TARGETS)
