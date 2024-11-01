#!/bin/bash
#
#  Start pipan8l on the zturn board programmed with the PDP-8/L simulator
#
dd=`dirname $0`
$dd/loadmod.sh
export pipan8lini=$dd/z8lpanini.tcl
exec ./pipan8l -z8l "$@"
