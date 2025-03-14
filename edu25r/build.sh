#!/bin/bash
#
#  Build EDU25R using real or sim
#
case $1 in
    real) exec ../pipan8l/z8lpanel -log ~/build.log build.tcl ;;
    sim)  exec ../pipan8l/z8lsim -log ~/build.log build.tcl   ;;
esac
