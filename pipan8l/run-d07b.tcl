puts ""
puts "d07b: random isz test"

stopandreset
openttypipes
loadbin ../alltapes/maindec-08-d07b-pb.bin
setsw sr 00037
flicksw ldad
setsw sr 04000
flicksw start

puts "d07b: waiting for 3 07<CR><LF><LF>s, should be about 15 seconds"
for {set i 0} {$i < 3} {incr i} {
    for {set started [clock seconds]} {[clock seconds] - $started < 10} { } {
        set ch [readttychartimed 100]
        if {$ch < " "} continue
        if {$ch == "\177"} continue
        if {$ch == "0"} break
    }
    if {$ch != "0"} {
        puts "d07b: too long waiting for 07<CR><LF><LF>"
        exit 1
    }
    waitforttypr 1000 "7\r\n\n"
    puts "d07b: got a 07<CR><LF><LF>"
}
puts "SUCCESS!"
