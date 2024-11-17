#
#  Boot OS/8 from RK05
#    ./z8lsim bootos8dpack.tcl
#
puts ""
puts "bootos8dpack: test running OS/8"

stopandreset

set home [getenv HOME /tmp]
exec cp rkab0.rk05 $home/rkab0.rk05
exec chmod o+w $home/rkab0.rk05
set rk8pid [exec ./z8lrk8je -killit -loadrw 0 $home/rkab0.rk05 &]
after 1000

startat [loadbin rk8jeboot.bin]

puts "bootos8dpack: starting"
exec -ignorestderr ./z8ltty -cps 960 -killit < /dev/tty > /dev/tty
exec kill $rk8pid
exit
