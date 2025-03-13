#
#  Boot OS/8 from RK05
#
#    real PDP:  ./z8lpanel bootos8dpack.tcl
#    simulator: ./z8lsim bootos8dpack.tcl
#
global Z8LHOME
source $Z8LHOME/os8lib.tcl
os8dpackloadandboot
exec -ignorestderr $Z8LHOME/z8ltty -cps 960 -killit -upcase < /dev/tty > /dev/tty
flicksw stop
exit
