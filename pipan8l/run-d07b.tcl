loadbin ../alltapes/maindec-08-d07b-pb.bin
setsw sr 00037
flicksw ldad
setsw sr 04000
puts ""
puts "  random JMP/JMS test"
puts "  start tty (./z8ltty) on another terminal"
puts "  do 'flicksw start' to start test"
puts "  prints 07<CR><LF><CR><LF> on the tty every 5 seconds"
