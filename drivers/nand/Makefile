# Makefile for nand block
#

EXTRA_CFLAGS	+= -Idrivers/nand/manager/inc/ \
		-Idrivers/nand/inc/ \
		-Idrivers/nand/manager/inc/linux/

obj-y		+= manager/ driver/ nand_block.o

all:
	find . -name *.o | xargs rm -vf;
	find . -name *.o.cmd | xargs rm -vf;
	find . -name *.o.d | xargs rm -vf;
	find . -name *~ | xargs rm -vf;
	cd ../../;make vmlinux;cd -
