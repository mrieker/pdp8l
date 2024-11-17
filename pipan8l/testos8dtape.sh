#!/bin/bash
#!/bin/bash
echo = = = = = = = =
echo loading tape
cp dta0.tu56 ~/dta0.tu56
./z8ltc08 -loadrw 0 ~/dta0.tu56 > ~/z8ltc08.log 2>&1 &
echo = = = = = = = =
echo testing
./z8lsim testos8dtape.tcl
exst=$?
echo = = = = = = = =
killall z8ltc08.`uname -m` 2> /dev/null
killall z8ltty.`uname -m` 2> /dev/null
exit $exst
