
# run kaleidoscope on zynq with simulated PDP-8/L
#  ./z8lsim kaleid.tcl e|i

set vt [lindex $argv 0]
if {($vt != "e") && ($vt != "i")} {
    puts "bad or missing 'e' or 'i' argument"
    exit 1
}
exec -ignorestderr make kaleid-$vt.bin
set startaddr [loadbin kaleid-$vt.bin]
setsw sr 0
startat $startaddr
exec -ignorestderr ./z8lvc8 -pms 120 -size [getenv vcsize 512] $vt
exit
