#!/bin/sh
CURPATH=`pwd`
COMMAND=
if [ $# -eq 1 ]; then
if [ $1 = "clean" ]; then
    COMMAND=clean
else
    echo $0 clean
    exit 1
fi
fi
echo $COMMAND
for d in `find -type d -name "*"`;do
    if [ -f $d/Makefile ];then
	make -C $d $COMMAND
    fi
done
rm test.ini nand.bin -f
if [ -z $COMMAND ]; then 
touch test.ini
for f in `ls *.so`;do
    echo $CURPATH/$f >> test.ini 
done
fi
