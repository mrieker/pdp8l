
# run kaleidoscope on zynq with real or simulated PDP-8/L
#  [vcsize=1024] ./z8lpanel kaleid.tcl e|i      real PDP-8/L
#  [vcsize=1024] ./z8lsim kaleid.tcl e|i        simulator

set vt [lindex $argv 0]
if {($vt != "e") && ($vt != "i")} {
    puts "bad or missing 'e' or 'i' argument"
    exit 1
}
exec -ignorestderr make kaleid-$vt.bin
set startaddr [loadbin kaleid-$vt.bin]
setsw sr 0
startat $startaddr
relsw all
exec -ignorestderr ./z8lvc8 -pms 120 -size [getenv vcsize 512] $vt
exit
