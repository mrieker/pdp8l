#!/bin/bash
function runtest {
    rm -f /tmp/testos8_rk8je_pipe
    ./z8lsim testos8.tcl
    exst=$?
    echo exit >&200
    return $exst
}
echo = = = = = = = =
echo loading disk
rm -f /tmp/testos8_rk8je_pipe
mkfifo /tmp/testos8_rk8je_pipe
cp rkab0.rk05 ~/rkab0.rk05
./z8lrk8je -loadrw 0 ~/rkab0.rk05 < /tmp/testos8_rk8je_pipe &
rkpid=$!
echo = = = = = = = =
echo testing
runtest 200>/tmp/testos8_rk8je_pipe
exst=$?
echo = = = = = = = =
echo exiting
wait $rkpid
exit $exst
