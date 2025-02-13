#
#  Boot OS/8 from DECtape
#    ./z8lpanel bootos8dtape.tcl    ... to run on real pdp
#    ./z8lsim bootos8dtape.tcl      ... to run on simulator
#
source "os8lib.tcl"
os8dtapeloadandboot
exec -ignorestderr ./z8ltty -cps 960 -killit -upcase < /dev/tty > /dev/tty
flicksw stop
exit
