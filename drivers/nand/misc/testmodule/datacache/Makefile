CC = gcc
CXX = g++
CFLAGS  = -Wall -pipe -DDEBUG -D_NEW_LIC -g -D_GNU_SOURCE -shared -D_REENTRANT
LIB     = -ldl -lrt  -g
INCLUDE = -I../inc -I../../../inc -I../../../manager/inc/
OBJS    = testDataCache.c ../../datacache/datacache.c
TARGETS = ../testDataCache.so
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
