#!/bin/bash
#
#  Run simple EDU20C test on real or sim
#
case $1 in
    real) exec ../pipan8l/z8lpanel -log ~/test.log test.tcl ;;
    sim)  exec ../pipan8l/z8lsim -log ~/test.log test.tcl   ;;
esac
