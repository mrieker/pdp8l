#!/bin/bash
#
#  Create bootable OS/8 disk with music files
#
cd `dirname $0`
set -e
disk=~/music-rkab0.rk05
if [ ! -f $disk ]
then
    gzdisk=os8-rkab0.rk05.gz
    if [ ! -f $gzdisk ]
    then
        echo "downloading RK05 image to $gzdisk"
        rm -f $gzdisk.temp
        wget -nv -O $gzdisk.temp https://www.pdp8online.com/ftp/images/os8/diag-games-kermit.rk05
        gzip $gzdisk.temp
        chmod a-w $gzdisk.temp.gz
        mv $gzdisk.temp.gz $gzdisk
    fi
    echo "creating $disk from $gzdisk"
    rm -f $disk.temp
    gunzip -c $gzdisk > $disk.temp
    mv -f $disk.temp $disk
    echo ""
    echo "  The MUSIC.SV program on the disk does not work."
    echo "  You must follow the instructions in readme.txt"
    echo "    to create a working version."
    echo ""
else
    echo "already have a $disk file"
    echo "remove it first if you want to start over"
fi
