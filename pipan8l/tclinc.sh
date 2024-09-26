#!/bin/bash
#
#  Caled by makefile to print name of directory where tcl.h resides
#
function process
{
    if ! read shortest
    then
        echo "tclinc.sh: no tcl.h found in /usr/include tree" >&2
        exit 1
    fi
    while read line
    do
        if [ ${#shortest} -gt ${#line} ]
        then
            shortest=$line
        fi
    done
    dirname $shortest
}

find /usr/include -name tcl.h | process
