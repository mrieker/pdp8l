#
#  Boot OS/8 from DECtape
#    ./z8lsim bootos8dtape.tcl
#
stopandreset

set home [getenv HOME /tmp]
exec cp dta0.tu56 $home/dta0.tu56
exec chmod o+w $home/dta0.tu56
set tc08pid [exec ./z8ltc08 -killit -loadrw 0 $home/dta0.tu56 &]
after 1000

;# from OS8 Handbook p41

wrmem 07613 06774   ;# 7613 load status register B (with zeroes)
wrmem 07614 01222   ;# 7614 TAD 7622 / 0600 (rewind tape)
wrmem 07615 06766   ;# 7615 clear and load status register A
wrmem 07616 06771   ;# 7616 skip on flag
wrmem 07617 05216   ;# 7617 JMP .-1
wrmem 07620 01223   ;# 7620 TAD 7623 / 0220 (read tape)
wrmem 07621 05215   ;# 7621 JMP 7615
wrmem 07622 00600   ;# 7622
wrmem 07623 00220   ;# 7623
wrmem 07754 07577   ;# 7754
wrmem 07755 07577   ;# 7755

startat 07613

puts "bootos8dtape: starting"
exec -ignorestderr ./z8ltty -cps 960 -killit < /dev/tty > /dev/tty
exec kill $tc08pid
exit
