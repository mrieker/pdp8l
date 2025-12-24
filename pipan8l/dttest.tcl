#
#  Single-drive DECTape test
#
#   ./z8lpanel -real dttest.tcl     real PDP
#   ./z8lsim dttest.tcl             simulator
#
set home [getenv HOME /tmp]
exec -ignorestderr ./z8ltc08 -killit -loadrw 0 $home/dttest.tu56 &
set sa [loadbinptr dttest.bin]
setsw mprt 0
setsw sr $sa
flicksw ldad
setsw sr 0234
flicksw start
exec -ignorestderr ./z8ltty -cps 120 < /dev/tty > /dev/tty
