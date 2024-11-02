loadbin ../alltapes/maindec-8i-d02b-pb.bin
setsw sr 07777
flushit
puts ""
puts "  start tty (./z8ltty) on another terminal"
puts "  do 'startat 0201' to start test"
puts "  sends BELs to the tty every 5 seconds"
