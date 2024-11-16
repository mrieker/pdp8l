#!/bin/bash
echo = = = = = = = =
echo loading disk
cp rkab0.rk05 ~/rkab0.rk05
./z8lrk8je -loadrw 0 ~/rkab0.rk05 &
rkpid=$!
echo = = = = = = = =
echo booting
./z8lsim bootos8.tcl
echo = = = = = = = =
echo terminal
./z8ltty -cps 30
kill $rkpid
