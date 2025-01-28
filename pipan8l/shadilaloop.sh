#!/bin/bash
#
#  Use pdp8lshad.v and zynq.v ilaarray to detect errors in PDP-8/L
#  Freeze processor and dump state whenever error found, then restart
#
cd `dirname $0`
set -x
while true
do
    while true
    do
        sleep 12
        if ./z8lpanel <<END
            while {[pin get simit]} {after 3456}
            hardreset
            pin set xm_enlo4k 1 sh_frzonerr 1 sh_clearit 1
END
        then
            break
        fi
    done
    date
    ./z8lila
    ./z8ldump -once
    date
done
