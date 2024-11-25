#!/bin/bash
#
#  copy zturn36 to zturn35 or zturn34
#  $1 = zturn35 or zturn34
#

function process
{
    set -x -e
    while read line
    do
        rm -rf x.tmp
        sed "s/zturn36/$name/g" $line > x.tmp
        rm -f $line
        mv -f x.tmp ${line/zturn36/$name}
    done
}

set -x -e
name=$1
rm -rf $name
cp -a zturn36 $name
rm -rf $name/gerber
rm -rf $name/zturn36-backups
find $name -type f | process
