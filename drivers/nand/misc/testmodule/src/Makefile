CC = gcc
CXX = g++

TARGET = ../test
OBJS=main.o
INC=-I../inc
C_FLAGS += -g -Wall $(INC)
LIB_FLAGS = -pthread -ldl

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LIB_FLAGS) $(LIB) $(C_FLAGS) 

.cpp.o:
	$(CXX) -c -o $*.o $(INC) $(C_FLAGS) $*.cpp
.c.o:
	$(CC) -c -o $*.o $(INC) $(C_FLAGS) $*.c
clean:
	rm -f *.o $(TARGET)

