# Makefile for nand block
#

EXTRA_CFLAGS	+= -Idrivers/nand/manager/inc/ \
		-Idrivers/nand/inc/ \
		-Idrivers/nand/manager/inc/linux/

obj-y		+= manager/ nand_block.o driver/
#obj-y		+= driver/
obj-$(CONFIG_NAND_DUG) += nand_driver_debug.o
all:
	find . -name *.o | xargs rm -vf;
	find . -name *.o.cmd | xargs rm -vf;
	find . -name *.o.d | xargs rm -vf;
	find . -name *~ | xargs rm -vf;
	cd ../../;make uImage;cd -
