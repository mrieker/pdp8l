#!/bin/bash
#
#  Run music program on real PDP or simulator
#  Use music-real.sh or music-sim.sh
#
set -e
if [ "$1" == "" ]
then
    echo "use music-real.sh or music-sim.sh"
    exit 1
fi
diskfile=~/music-rkab0.rk05
if [ ! -f $diskfile ]
then
    echo "no $diskfile - run setup-os8.sh"
    exit 1
fi
../pipan8l/z8l$1 <<END
    hardreset
END
../pipan8l/z8lrk8je -killit -loadrw 0 $diskfile &
sleep 2
../pipan8l/z8l$1 bootos8.tcl
../pipan8l/z8lpbit.armv7l   # enable ISZAC instruction
echo ""
echo "  to hear music"
echo "    * turn on AM radio and put near backplane"
echo "    * connect amplified speaker to D36-B2"
echo "        with pullup to D36-A2 (+5V)"
echo "        and ground to such as D36-C1 or C2"
echo "    * run these programs:"
echo "        on a PC with sound card: ./z8lpbit -server &"
echo "        on this zturn: ./z8lpbit -sound <ipaddressofpc> &"
echo ""
echo "  at . prompt type DIR RKB0:*.MU"
echo ""
echo "  at . prompt type R MUSIC"
echo "  at * prompt type RKB0:FIFTH1"
echo ""
echo "  press control-\\ when all done"
echo ""
exec ../pipan8l/z8ltty -cps 120 -killit -upcase
