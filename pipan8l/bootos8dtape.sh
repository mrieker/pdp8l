#!/bin/bash
echo = = = = = = = =
echo loading tape
cat dta0.tu56 > ~/dta0.tu56
./z8ltc08 -loadrw 0 ~/dta0.tu56 &
echo = = = = = = = =
echo booting
./z8lsim bootos8dtape.tcl
echo = = = = = = = =
echo opening tty
./z8ltty -cps 30 -killit
killall z8ltc08.`uname -m`
