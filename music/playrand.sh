#!/bin/bash
#
#  Run music program on real PDP or simulator
#  Play random .MU files on RKB0:
#
set -e
if [ "$1" == "" ]
then
    echo "missing 'sim' or 'real' argument"
    exit 1
fi
diskfile=~/music-rkab0.rk05
if [ ! -f $diskfile ]
then
    echo "no $diskfile - run setup-os8.sh"
    exit 1
fi
../pipan8l/z8lrk8je -killit -loadrw 0 $diskfile &
sleep 2
case $1 in
    real)../pipan8l/z8lpanel playrand.tcl ;;
    sim) ../pipan8l/z8lsim playrand.tcl ;;
esac
