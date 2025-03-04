#
#  boot os/8 disk loaded in rk05 drive 0
#  runs on z8lpanel or z8lsim
#
puts "manual: resetting processor"
hardreset
puts "manual: toggling in bootloader"
source ../pipan8l/os8lib.tcl
set sa [os8dpack0toggleinboot]
puts "manual: booting os/8"
setsw sr $sa    ;# set start address in switch register
flicksw ldad    ;# load address into program counter
flicksw start   ;# start os/8
relsw all       ;# release all switches
exec ../pipan8l/z8lpbit     ;# enable ISZAC opcode for our patched music.pal program
