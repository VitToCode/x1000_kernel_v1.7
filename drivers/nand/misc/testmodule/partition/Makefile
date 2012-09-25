CC = gcc
CXX = g++
CFLAGS  = -Wall -pipe -DDEBUG -D_NEW_LIC -g -D_GNU_SOURCE -shared -D_REENTRANT -DTEST_PARTITION
LIB     = -ldl -lrt  -g
INCLUDE = -I../inc -I../../../inc -I../../../manager/inc/ -I../../../manager/utils
OBJS    = testpartition.c  ../../../manager/utils/bufflistmanager.c ../app/NandSemaphore.c ../../../manager/utils/zonememory.c   ../../../manager/nandmanagerinterface.c ../../../manager/vnand/vNand.c   ../../../manager/vnand/l2vNand.c
TARGETS = ../testpartition.so 
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
