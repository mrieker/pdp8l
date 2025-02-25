#!/bin/bash
#
#  Run on zturn plugged into PDP-8/L
#
set -e
../pipan8l/z8lrk8je -killit -loadrw 0 ~/music-rkab0.rk05 &
sleep 2
../pipan8l/z8lpanel -real bootos8.tcl
../pipan8l/z8lpbit.armv7l
echo ""
echo "  at . prompt type DIR RKB0:*.MU"
echo ""
echo "  at . prompt type R MUSIC"
echo "  at * prompt type RKB0:FIFTH1"
echo ""
exec ../pipan8l/z8ltty -cps 120 -killit -upcase
