#!/bin/bash
echo = = = = = = = =
echo loading disk
cp rkab0.rk05 ~/rkab0.rk05
./z8lrk8je -loadrw 0 ~/rkab0.rk05 < /dev/tty &
rkpid=$!
sleep 3
echo = = = = = = = =
echo testing
./z8lpan.sh testos8.tcl
echo = = = = = = = =
kill $!
