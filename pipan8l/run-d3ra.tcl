#
#  Random DECtape test:
#    ./z8lsim run-d3ra.tcl
#
#  Optionally run status on another screen:
#    ./z8ltc08 -status [<ip-address-of-this-system>]
#
loadbin ../alltapes/d3ra-pb.bin
puts "= = = = = = = = = = = = = = = ="
puts "run-d3ra: loading tape files"
set home [getenv HOME /tmp]
set tcpid [exec ./z8ltc08 -killit -loadrw 1 $home/tape1.tu56 -loadrw 2 $home/tape2.tu56 -loadrw 3 $home/tape3.tu56 &]
atexit "exec kill $tcpid"
after 3000
puts "= = = = = = = = = = = = = = = ="
puts "run-d3ra: starting test"
setsw sr 03400
startat 00200
while {[getreg run]} {
    if {[ctrlcflag]} return
    after 100
}
if {([getreg state] != "") || ([getreg ma] != 00207)} {
    puts "didn't halt at 0207"
    puts [dumpit]
    return
}
setsw sr 00000
flicksw cont
puts "= = = = = = = = = = = = = = = ="
puts "run-d3ra: running (control-C to stop)..."
set ttpid [exec ./z8ltty -killit -nokb &]
atexit "exec kill $ttpid"
while {[getreg run]} {
    if {[ctrlcflag]} return
    after 100
}
puts "halted: [dumpit]"
