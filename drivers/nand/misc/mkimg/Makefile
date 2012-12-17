CC = gcc
CXX = g++
LIBDIR=
LIB     = -ldl  -g -lpthread -lrt
MLIB   = ../build/lib/libnm.a

INCLUDE = -I./inc -I../../manager/inc -I../../manager/lib/inc -I../testmodule/inc

CFLAGS  = -Wall -pipe -DDEBUG  -g -D_GNU_SOURCE  -m32
OBJS    = main.o argument.o driver/APP_vNand.c

TARGETS = mkimg
all: $(TARGETS)
$(TARGETS): $(OBJS) $(MLIB)
	$(CC) $(CFLAGS) $(INCLUDE) $(OBJS) -o $@ $(MLIB) $(LIBDIR) $(LIB) 
.c.o:
	$(CC)  $(CFLAGS) -c $(INCLUDE) $<

.cpp.o:
	$(CXX) $(CFLAGS) -c $(INCLUDE) $<
$(MLIB):
	make -C ../build 
clean:
	rm -f *.o
	rm -f $(TARGETS)

