loadbin ../alltapes/maindec-08-d05b-pb.bin
setsw sr 00200
flicksw ldad
setsw sr 04000
puts ""
puts "  random JMP/JMS test"
puts "  start tty (./z8ltty) on another terminal"
puts "  do 'flicksw start' to start test"
puts "  prints <CR><LF>05 on the tty every 15 seconds"
