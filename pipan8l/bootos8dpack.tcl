#
#  Boot OS/8 from RK05
#
#    real PDP:  ./z8lpanel bootos8dpack.tcl
#    simulator: ./z8lsim bootos8dpack.tcl
#
source "os8lib.tcl"
os8dpackloadandboot
exec -ignorestderr ./z8ltty -cps 960 -killit < /dev/tty > /dev/tty
flicksw stop
exit
