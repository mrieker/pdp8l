loadbin ../alltapes/maindec-08-d1eb-pb.bin
setsw sr 0200
flicksw ldad
flushit
puts ""
puts "  extended memory checkerboard"
puts "  start tty (./z8ltty) on another terminal"
puts "  do 'flicksw start' to start test"
puts "  prints 'TEST LIMITS' prompt"
puts "  enter low,high memory stack numbers eg 0,7<CR>"
puts "  prints 'SETUP SR'"
puts "  do 'setsw sr 0 ; flushit'"
puts "  press <CR> on tty"
