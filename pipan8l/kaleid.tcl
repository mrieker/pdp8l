
# run kaleidoscope on zynq with real or simulated PDP-8/L
#  [vcsize=1024] ./z8lpanel kaleid.tcl e|i      real PDP-8/L
#  [vcsize=1024] ./z8lsim kaleid.tcl e|i        simulator

set vt [lindex $argv 0]
if {($vt != "e") && ($vt != "i")} {
    puts "bad or missing 'e' or 'i' argument"
    exit 1
}
exec -ignorestderr make kaleid-$vt.bin
set startaddr [loadbinptr kaleid-$vt.bin]
set nowtv [gettod]
set initx [expr {int ($nowtv * 1000000) % 1000 + 1234}]
set inity [expr {int ($nowtv * 1000000) / 1000 % 1000 + 2345}]
puts "kaleid: initx=[octal $initx] inity=[octal $inity]"
wrmem 020 $initx
wrmem 021 $inity
setsw sr 0
startat $startaddr
relsw all
exec -ignorestderr ./z8lvc8 -pms 120 -size [getenv vcsize 800] $vt
exit
