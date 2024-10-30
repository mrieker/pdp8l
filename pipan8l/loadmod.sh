#!/bin/bash
cd `dirname $0`
if grep -q 'Xilinx Zynq' /proc/cpuinfo
then
    if ! lsmod | grep -q ^zynqpdp8l
    then
        if [ ! -f km-zynqpdp8l/zynqpdp8l.`uname -r`.ko ]
        then
            cd km-zynqpdp8l
            rm -f zynqpdp8l.ko zynqpdp8l.mod* zynqpdp8l.o modules.order Module.symvers .zynqpdp8l* .modules* .Module*
            make
            mv zynqpdp8l.ko zynqpdp8l.`uname -r`.ko
            cd ..
        fi
        sudo insmod km-zynqpdp8l/zynqpdp8l.`uname -r`.ko
    fi
fi
