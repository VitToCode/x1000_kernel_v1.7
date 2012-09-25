CC = gcc
CXX = g++
CFLAGS  = -pipe -DDEBUG -D_NEW_LIC -g -D_GNU_SOURCE -shared -D_REENTRANT
LIB     = -ldl -lrt  -g
INCLUDE = -I../inc -I../../../inc -I../../../manager/inc
OBJS    = testvNand.o ../common/initnand.c ../../driver/src/APP_vNand.c ../../../manager/vnand/vNand.c ../app/NandSemaphore.c ../../../manager/l2p/hashnode.c ../../../manager/l2p/hash.c ../../../manager/utils/bufflistmanager.c ../../../manager/utils/zonememory.c  ../../../manager/vnand/l2vNand.c
TARGETS = ../testvNand.so
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
