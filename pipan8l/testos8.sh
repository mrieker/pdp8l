#!/bin/bash
echo = = = = = = = =
echo loading disk
cp rkab0.rk05 ~/rkab0.rk05
./z8lrk8je -loadrw 0 ~/rkab0.rk05 &
echo = = = = = = = =
echo testing
./z8lsim testos8.tcl
exst=$?
echo = = = = = = = =
killall z8lrk8je.`uname -m` 2> /dev/null
killall z8ltty.`uname -m` 2> /dev/null
exit $exst
