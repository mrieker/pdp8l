puts ""
puts "d04b: random jmp test"

stopandreset
openttypipes
if {[lindex $argv 0] == "-slow"} {
    loadbin ../alltapes/maindec-08-d04b-pb.bin
} else {
    loadbinptr ../alltapes/maindec-08-d04b-pb.bin
}
setsw sr 00200
flicksw ldad
setsw sr 04000
flicksw start

puts "d04b: waiting for 3 <CR><LF>04s, should be about 45 seconds"
for {set i 0} {$i < 3} {incr i} {
    waitforttypr 30000 "04"
    puts "d04b: got a 04"
}
puts "SUCCESS!"
