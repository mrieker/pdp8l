puts ""
puts "d02b: instruction test 2"
openttypipes
loadbin ../alltapes/maindec-8i-d02b-pb.bin
setsw sr 07777
flushit
startat 0201

puts "d02b: waiting for 3 bells, should be about 15 seconds"
set nbells 0
for {set started [clock seconds]} {[clock seconds] - $started < 30} {} {
    set ch [readttychartimed 100]
    if {$ch == ""} continue
    if {$ch == "\177"} continue
    if {$ch != "\007"} {
        puts "d02b: unexpected output char [escapechr $ch]"
        exit 1
    }
    puts [format "%*sDING!" $nbells ""]
    incr nbells
    if {$nbells > 2} {
        puts "SUCCESS!"
        exit 0
    }
}
puts "d02b: took too long for 3 bells"
dumpit
exit 1
