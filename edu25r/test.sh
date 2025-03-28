#!/bin/bash
#
#  Run simple EDU25R test on real or sim
#
case $1 in
    real) exec ../pipan8l/z8lpanel -log ~/test.log test.tcl ;;
    sim)  exec ../pipan8l/z8lsim -log ~/test.log test.tcl   ;;
esac
