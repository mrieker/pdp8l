#!/bin/bash
#
#  create the BOOT.BIN file for pdp8l
#  assumes the .bit bitstream file is up-to-date (compiled with vivado)
#  assumes hostname zturn points to zturn board all booted up with an existing BOOT.BIN file
#    zturn root .ssh/authorized_keys should have this user's .ssh public key so don't need to enter its password
#  assumes directories ../zplin/petadevel and ../zplin/petakernel exist and are filled in
#    follow howto file in ../zplin
#
set -e -v
cd `dirname $0`
mydir=`pwd`
cd ../zplin
source petadevel/settings.sh
source /tools/Xilinx/Vivado/2018.3/settings64.sh
cd petakernl/zturn
rm -f images/linux/BOOT.BIN
petalinux-package --boot --force --fsbl ./images/linux/zynq_fsbl.elf --fpga $mydir/pdp8l.runs/impl_1/myboard_wrapper.bit --u-boot
ls -l `pwd`/images/linux/BOOT.BIN
scp images/linux/BOOT.BIN root@zturn:/boot/BOOT.BIN
set +e
ssh root@zturn reboot
ping -c 70 zturn

# ssh zturn
# cd .../pdp8l/pipan8l
# ./z8lsim run-d01b.tcl

