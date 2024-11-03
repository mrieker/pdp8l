loadbin ../alltapes/maindec-08-d1gb-pb.bin
setsw sr 0200
flicksw ldad
setsw sr 0017
flushit
puts ""
puts "  extended memory control"
puts "  start tty (./z8ltty) on another terminal"
puts "  do 'flicksw start' to start test"
puts "  sends a BEL to tty every pass (45 seconds)"
