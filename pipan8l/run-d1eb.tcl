puts ""
puts "d1eb: extended memory checkerboard"

stopandreset
openttypipes
loadbin ../alltapes/maindec-08-d1eb-pb.bin
setsw sr 0200
flicksw ldad
flushit
flicksw start

waitforttypr 15000 "TEST LIMITS\r\n"
sendtottykb "0,7\r"
waitforttypr 15000 "SETUP SR"
setsw sr 0
flushit
sendtottykb "\r"
while true {
    set ch [readttychartimed 250]
    if {$ch == ""} break
    puts "d1eb: tossing char [escapechr $ch]"
}

set nmin 3
puts "d1eb: letting it run for $nmin minutes"
set error 0
for {set started [clock seconds]} {[clock seconds] - $started < $nmin * 60} {} {
    if {[ctrlcflag]} return
    if {! [getreg run]} {
        puts "d1eb: processor halted"
        puts [dumpit]
        return
    }
    if {![getreg run]} {
        puts "d1eb: processor halted - failed"
        exit 1
    }
    set ch [readttychartimed 250]
    if {$ch != ""} {
        puts "d1eb: error char [escapechr $ch]"
        set error 1
    } elseif {$error} {
        puts "d1eb: got error output - failed"
        exit 1
    }
}
puts "d1eb: no output for $nmin minutes we assume is success"
puts "SUCCESS!"
