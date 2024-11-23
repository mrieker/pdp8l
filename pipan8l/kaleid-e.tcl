
# run kaleidoscope on zynq with simulated PDP-8/L
#  ./z8lsim kaleid-e.tcl

exec -ignorestderr make kaleid-e.bin
set startaddr [loadbin kaleid-e.bin]
setsw sr 0
startat $startaddr
exec -ignorestderr ./z8lvc8 e -pms 120 -size 512
exit
