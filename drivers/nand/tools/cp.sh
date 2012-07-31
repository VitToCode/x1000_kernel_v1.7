#!/bin/bash

if [ $# -eq 1 ]; then
    SRC=$1
    DST=`pwd`
    #echo "find $DST/../ -name \"*.[hc]\""
    for i in `find $DST/../ -name "*.[hc]" | grep -v "inc/linux"`; do
	DFILE=$i;
	if [ ! -d $DFILE ]; then
	    #echo "find $SRC -name $(basename $DFILE) | grep -v 'thirdpart' | grep -v 'testmodule'"
	    let t=0;
	    for j in `find $SRC -name $(basename $DFILE) | grep -v "thirdpart"  | grep -v "testmodule"`; do
		let t=t+1
		if [ $t -ge 2 ]; then
		    echo "error"
		    exit
		fi
		echo "cp -f "$j $DFILE
		cp -f $j $DFILE
	    done
	fi
    done
else
    echo "./cp.sh <source dir>"
fi
