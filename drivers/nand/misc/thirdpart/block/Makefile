
obj-m		+= nand_block.o

#KERNELDIR = /lib/modules/$(shell uname -r)/build
KERNELDIR = /home/jbbi/work/android-mr1.1-verify/kernel
PWD := $(shell pwd)

all:
	make -C $(KERNELDIR) M=$(PWD) modules

clean:
	rm -rf *.o *.ko *.mod.c *.order *.symvers *~ .tmp_versions
	find . -name *.cmd | xargs rm -vf