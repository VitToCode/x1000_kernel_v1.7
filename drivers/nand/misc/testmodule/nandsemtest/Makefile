CC = gcc
CXX = g++
CFLAGS  = -Wall -pipe -DDEBUG -D_NEW_LIC -g -D_GNU_SOURCE -shared -D_REENTRANT
LIB     = -ldl -lrt  -g
TOP:=../..
MANAGER:=$(TOP)/manager
DRIVER:=$(TOP)/driver

INCLUDE = -I$(TOP)/include -I$(MANAGER)/inc -I$(MANAGER)/utils -I../inc
OBJS    = testNandSemaphore.o ../app/NandThread.c ../app/NandSemaphore.c
TARGETS = ../testsem.so
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
