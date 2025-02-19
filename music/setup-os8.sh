#!/bin/bash
#
#  Create bootable OS/8 disk with music files
#
cd `dirname $0`
set -e
disk=~/music-rkab0.rk05
if [ ! -f $disk ]
then
    tsdisk=os8-rkab0.rk05
    if [ ! -f $tsdisk ]
    then
        echo "downloading RK05 image to $tsdisk"
        rm -f $tsdisk.temp
        wget -nv -O $tsdisk.temp https://www.pdp8online.com/ftp/images/os8/diag-games-kermit.rk05
        mv -f $tsdisk.temp $tsdisk
    fi
    echo "creating $disk from $tsdisk"
    rm -f $disk.temp
    cp $tsdisk $disk.temp
    chmod u+w $disk.temp
    mv -f $disk.temp $disk
    echo ""
    echo "  The MUSIC.SV program on the disk does not work."
    echo "  You must follow the instructions in compiling.txt"
    echo "    to create a working version."
    echo ""
else
    echo "already have a $disk file"
    echo "remove it first if you want to start over"
fi
