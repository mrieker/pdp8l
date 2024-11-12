#!/bin/bash
echo = = = = = = = =
echo loading disk
rm -f /tmp/testos8_rk8je_pipe
mkfifo /tmp/testos8_rk8je_pipe
cp rkab0.rk05 ~/rkab0.rk05
./z8lrk8je -loadrw 0 ~/rkab0.rk05 < /tmp/testos8_rk8je_pipe &
rkpid=$!
(
    rm -f /tmp/testos8_rk8je_pipe
    sleep 1
    echo = = = = = = = =
    echo testing
    ./z8lsim testos8.tcl
    echo = = = = = = = =
) 200>/tmp/testos8_rk8je_pipe
wait $rkpid
