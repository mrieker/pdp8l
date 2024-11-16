#!/bin/bash
#
#  Test z8ltc08.cc and pdp8ltc.v
#
set -e
if [ dttest.bin -ot dttest.asm ]
then
    pdp8v/asm/assemble dttest.asm dttest.obj > dttest.lis
    pdp8v/asm/link -o dttest.oct dttest.obj > dttest.map
    pdp8v/asm/octtobin < dttest.oct > dttest.bin
fi
export z8ltc08_debug=0
./z8ltc08 -killit -loadrw 0 ~/dttest.tu56 &
tcid=$!
./z8ltty -cps 960 -killit -nokb &
ttid=$!
./z8lsim dttest.tcl
kill $tcid
kill $ttid
