
# run kaleidoscope on zynq with simulated PDP-8/L
#  ./z8lsim kaleid-i.tcl

exec -ignorestderr make kaleid-i.bin
set startaddr [loadbin kaleid-i.bin]
setsw sr 0
startat $startaddr
exec -ignorestderr ./z8lvc8 i -pms 120 -size 512
exit
