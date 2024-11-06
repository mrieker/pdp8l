puts ""
puts "d1gb: extended memory control"

openttypipes
loadbin ../alltapes/maindec-08-d1gb-pb.bin
setsw sr 0200
flicksw ldad
setsw sr 0017
flicksw start

puts "d1gb: waiting for 3 bells, should be about 150 seconds"
set nbells 0
for {set started [clock seconds]} {[clock seconds] - $started < 300} {} {
    set ch [readttychartimed 100]
    if {$ch == ""} continue
    if {$ch == "\r"} continue
    if {$ch == "\n"} continue
    if {$ch == "\177"} continue
    if {$ch != "\007"} {
        puts "d1gb: unexpected output char [escapechr $ch]"
        exit 1
    }
    puts [format "%*sDING!" $nbells ""]
    incr nbells
    if {$nbells > 2} {
        puts "SUCCESS!"
        exit 0
    }
}
puts "d1gb: took too long for 3 bells"
dumpit
exit 1
