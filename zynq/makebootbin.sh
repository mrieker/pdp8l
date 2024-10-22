#!/bin/bash
#
#  create the BOOT.BIN file for pdp8l
#  assumes the .bit bitstream file is up-to-date
#  assumes hostname zturn points to zturn board all booted up with an existing BOOT.BIN file
#  assumes directories ~/MYIR_HW/petadevel and ~/MYIR_HW/petakernel exist and are filled in
#
set -e -v
cd `dirname $0`
mydir=`pwd`
cd ~/MYIR_HW
source petadevel/settings.sh
source /tools/Xilinx/Vivado/2018.3/settings64.sh
cd petakernl/zturn
rm -f images/linux/BOOT.BIN
petalinux-package --boot --force --fsbl ./images/linux/zynq_fsbl.elf --fpga $mydir/pdp8l.runs/impl_1/myboard_wrapper.bit --u-boot
ls -l `pwd`/images/linux/BOOT.BIN
scp images/linux/BOOT.BIN root@zturn:/boot/BOOT.BIN
set +e
ssh root@zturn reboot
ping zturn

# ssh zturn
# cd nfs/pdp8/driver
# ./raspictl -zynqlib [-paddles] -randmem -mintimes

