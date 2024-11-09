#!/bin/bash
echo = = = = = = = =
echo loading disk
cp rkab0.rk05 ~/rkab0.rk05
./z8lrk8je -loadrw 0 ~/rkab0.rk05 < /dev/tty &
rkpid=$!
sleep 3
echo = = = = = = = =
echo booting
./z8lsim bootos8.tcl
echo = = = = = = = =
echo opening tty
./z8ltty -cps 30
kill $!
