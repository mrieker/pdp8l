puts ""
puts "d01b: instruction test 1"

;# open tty, load program
stopandreset
openttypipes
if {[lindex $argv 0] == "-slow"} {
    loadbin ../alltapes/maindec-8i-d01b-pb.bin
} else {
    loadbinptr ../alltapes/maindec-8i-d01b-pb.bin
}

;# program starts at 0144, halts at 0146 with ac=0000
puts "d01b: starting program"
setsw sr 00144
flicksw ldad
setsw sr 07777
flicksw start
puts "d01b: waiting for halt at 0146"
for {set i 0} {[getreg run] || ([getreg ma] != 0146)} {incr i} {
    if {$i > 10} {
        puts "d01b: failed to halt at 0146"
        puts [dumpit]
        exit 1
    }
    after 100
    flushit
}
if {[getreg ac] != 0} {
    puts "d01b: failed to halt at 0146 with 0000 in accumulator"
    puts [dumpit]
    exit 1
}
flicksw cont

;# then program runs forever outputting a BEL every second
puts "d01b: waiting for 3 bells, should be about 3 seconds"
set nbells 0
for {set started [clock seconds]} {[clock seconds] - $started < 6} {} {
    set ch [readttychartimed 100]
    if {$ch == ""} continue
    if {$ch != "\007"} {
        puts "d01b: ignoring unexpected output char [escapechr $ch]"
    } else {
        puts [format "%*sDING!" $nbells ""]
        incr nbells
        if {$nbells > 2} {
            puts "SUCCESS!"
            return
        }
    }
}
puts "d01b: took too long for 3 bells"
puts [dumpit]
exit 1
