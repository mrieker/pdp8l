#!/bin/bash
#
#  use vivado to create .bit file from .v and .vhd files
#
cd `dirname $0`
rm -f *.runs/*/runme.log
/tools/Xilinx/Vivado/2018.3/bin/vivado -mode batch -source makebitfile.tcl
while ( ps | grep -v grep | grep -q loader )
do
    sleep 1
done
find * -name \*.bit -ls
