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
exec ./z8ltc08 -killit -loadrw 1 tape1.tu56 -loadrw 2 tape2.tu56 -loadrw 3 tape3.tu56 &
after 2000
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
exec ./z8ltty -killit -nokb &
while {[getreg run]} {
    if {[ctrlcflag]} return
    after 100
}
puts "halted: [dumpit]"
