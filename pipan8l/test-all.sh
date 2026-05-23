#!/bin/bash
#
#  Run all tests
#  - a bunch of internal tests
#  - a bunch of MAINDEC tests
#  - simple OS/8 (disk and tape) scripts
#
#    ./test-all.sh real  - zynq plugged into pdp8/l with front-panel overlay board
#    ./test-all.sh sim   - runs on stand-alone zynq (or can be plugged into pdp)
#
set -e
cd `dirname $0`
mydir=`pwd`

if [ ! -f pdp8v/asm/assemble ]
then
    git clone https://github.com/mrieker/pdp8v.git
    cd pdp8v/asm
    make
    cd $mydir
fi

case $1 in
    real) simit=0 ;;
    sim)  simit=1 ;;
    *) echo "bad 'sim' or 'real' argument"
       exit 1
    ;;
esac
echo "pin set simit $simit ; hardreset" | ./z8lpanel

# let background process run for 1 minute
#  $1 = pid
#  $2 = program name
function onemin
{
    i=60
    while [[ $i -ge 0 ]]
    do
        sleep 1
        i=$(($i-1))
        if [ ! -d /proc/$1 ]
        then
            echo "$2 terminated"
            exit 1
        fi
    done
    kill $1
    sleep 1
}

echo ""
echo "= = = = = = = = = = = = = = = = ="
echo "  CMEMTEST"
./z8lcmemtest -3cycle -cainc -extmem &
onemin $! cmemtest

echo ""
echo "= = = = = = = = = = = = = = = = ="
echo "  PIOTEST"
./z8lpiotest &
onemin $! piotest
echo ""

echo ""
echo "= = = = = = = = = = = = = = = = ="
echo "  SIMTEST"
case $simit in
    0)
        ./z8lsimtest -real | grep '00000  L' &
        simpid=$!
    ;;
    1)
        ./z8lsimtest -half | grep '00000  L' &
        simpid=$!
    ;;
esac
onemin $simpid simtest
echo "hardreset" | ./z8lpanel

exec ./z8lpanel test-all.tcl
