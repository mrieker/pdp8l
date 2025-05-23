#
#  Random DECtape test:
#    ./z8lsim run-d3ra.tcl
#
#  Optionally run status on another screen:
#    ./z8ltc08 -status [<ip-address-of-this-system>]
#
loadbinptr ../alltapes/d3ra-pb.bin
puts "run-d3ra: loading tape files"
set home [getenv HOME /tmp]
set tcpid [exec ./z8ltc08 -killit -loadrw 1 $home/tape1.tu56 -loadrw 2 $home/tape2.tu56 -loadrw 3 $home/tape3.tu56 &]
atexit "exec kill $tcpid"
after 3000
puts "run-d3ra: starting test"
setsw mprt 0    ;# so program can write 7754,7755
setsw sr 03400
startat 00200
while {[getreg run]} {
    if {[ctrlcflag]} return
    after 100
}
if {[getreg ma] != 00207} {
    puts "didn't halt at 0207"
    puts [dumpit]
    return
}
setsw sr 00000
flicksw cont
set nmin 3
if {[llength $argv] > 0} {set nmin [lindex $argv 0]}
puts "run-d3ra: running for $nmin minutes (control-C to stop)..."
set ttpid [exec ./z8ltty -killit -nokb &]
for {set started [clock seconds]} {[clock seconds] - $started < $nmin * 60} {} {
    if {! [getreg run]} {
        puts "halted: [dumpit]"
        exit 1
    }
    if {[ctrlcflag]} return
    after 100
}
puts "d3ra: still running after $nmin minutes we assume is success"
flicksw stop
exec kill $tcpid $ttpid
while {[waitpid $tcpid] == ""} {if [ctrlcflag] break}
while {[waitpid $ttpid] == ""} {if [ctrlcflag] break}
puts "SUCCESS!"
