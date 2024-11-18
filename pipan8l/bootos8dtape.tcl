#
#  Boot OS/8 from DECtape
#    ./z8lsim bootos8dtape.tcl
#
source "os8lib.tcl"
os8dtapeloadandboot
exec -ignorestderr ./z8ltty -cps 960 -killit < /dev/tty > /dev/tty
flicksw stop
exit
