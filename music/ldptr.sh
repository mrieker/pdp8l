#!/bin/bash
#
#  Load text file in the paper tape reader
#
#  Get this script running with a Linux-style text file
#  Then in OS/8:
#
#    .R PIP
#    *somefile<PTR:
#    ^   wait for ^ prompt then press ENTER
#    *   get * prompt when read is complete
#
dd=`dirname $0`
exec $dd/../pipan8l/z8lptr -text $1
