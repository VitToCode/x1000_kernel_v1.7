#!/bin/bash

echo "make ARCH=mips CROSS_COMPILE=mips-linux-gnu-" $@
make ARCH=mips CROSS_COMPILE=mips-linux-gnu- $@
