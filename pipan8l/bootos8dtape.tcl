#
#  Boot OS/8 from DECtape
#    ./z8lpanel bootos8dtape.tcl    ... to run on real pdp
#    ./z8lsim bootos8dtape.tcl      ... to run on simulator
#
global Z8LHOME
source $Z8LHOME/os8lib.tcl
os8dtapeloadandboot
exec -ignorestderr $Z8LHOME/z8ltty -cps 960 -killit -upcase < /dev/tty > /dev/tty
flicksw stop
exit
