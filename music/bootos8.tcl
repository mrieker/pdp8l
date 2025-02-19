#
#  boot os/8 disk loaded in rk05 drive 0
#
#  runs on z8lpanel or z8lsim
#
puts "resetting processor"
hardreset
puts "loading os8lib.tcl"
source ../pipan8l/os8lib.tcl
puts "toggling in bootloader"
set sa [os8dpack0toggleinboot]
puts "booting os/8"
setsw sr $sa
flicksw ldad
pin set sh_clearit 1
relsw all
flicksw start
puts "finished"
