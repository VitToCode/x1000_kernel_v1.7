CC = gcc
CXX = g++
CFLAGS  = -pipe -DDEBUG -D_NEW_LIC -g -D_GNU_SOURCE -shared -D_REENTRANT -DTEST -DTEST_FORCERECYCLE -DTEST_PARTITION -DTEST -DTEST_L2P
LIB     = -ldl -lrt  -g
INCLUDE = -I../inc -I../../../inc -I../../../manager/inc -I../../../manager/utils
OBJS    = ../../../manager/l2p/zone.c  ../common/initnand.c ../../driver/src/APP_vNand.c ../../../manager/vnand/vNand.c ../app/NandSemaphore.c ../../../manager/utils/bufflistmanager.c ../../../manager/utils/zonememory.c ../../../manager/l2p/zonemanager.c  ../../../manager/l2p/hashnode.c ../../../manager/l2p/hash.c ../../../manager/l2p/recycle.c ./testrecycle.c ../../../manager/l2p/cachemanager.c ../../../manager/l2p/cachedata.c ../../../manager/l2p/cachelist.c ../../../manager/l2p/l2pconvert.c ../../../manager/l2p/taskmanager.c ../app/NandThread.c ../../../manager/nandmanagerinterface.c ../../../manager/l2p/errhandle.c ../../../manager/l2p/junkzone.c ../../../manager/l2p/badblockinfo.c
TARGETS = ../testrecycle.so
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
