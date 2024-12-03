puts ""
puts "d05b: random jmp/jms test"

stopandreset
openttypipes
loadbin ../alltapes/maindec-08-d05b-pb.bin
setsw sr 00200
flicksw ldad
setsw sr 04000
flicksw start

puts "d05b: waiting for 3 <CR><LF>05s on the tty, should take about 45 seconds"
for {set i 0} {$i < 3} {incr i} {
    for {set started [clock seconds]} {[clock seconds] - $started < 30} { } {
        set ch [readttychartimed 100]
        if {$ch < " "} continue
        if {$ch == "\177"} continue
        if {$ch == "0"} break
    }
    if {$ch != "0"} {
        puts "d05b: too long waiting for <CR><LF>05"
        exit 1
    }
    waitforttypr 1000 "5"
    puts "d05b: got a <CR><LF>05"
}
puts "SUCCESS!"
