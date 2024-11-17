#
#  Test OS/8 booting from DECtape
#    ./z8lsim testos8dtape.tcl
#
puts ""
puts "testos8dtape: test running OS/8"

stopandreset

set home [getenv HOME /tmp]
exec cp dta0.tu56 $home/dta0.tu56
exec chmod o+w $home/dta0.tu56
exec ./z8ltc08 -killit -loadrw 0 $home/dta0.tu56 &
after 1000

openttypipes

flicksw stop

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

puts "testos8dtape: starting"

checkttyprompt "." "COPY BUILD.XX<BUILD.SV"
checkttymatch 0 "FILES COPIED:"
checkttymatch 0 "BUILD.SV"

checkttyprompt "." "DIR" 100000     ;# takes about 90sec to copy file
checkttymatch 0 "ABSLDR.SV   5"     ;# takes about 20sec to start listing
checkttymatch 0 "CCL   .SV  18"
checkttymatch 0 "FOTP  .SV   8"
checkttymatch 0 "DIRECT.SV   7"
checkttymatch 0 "EDIT  .SV  10"
checkttymatch 0 "PAL8  .SV  19"
checkttymatch 0 "CREF  .SV  13"
checkttymatch 0 "PIP   .SV  11"
checkttymatch 0 "BOOT  .SV   5"
checkttymatch 0 "LOADER.SV  12"
checkttymatch 0 "SABR  .SV  24"
checkttymatch 0 "FORT  .SV  25"
checkttymatch 0 "RESORC.SV  10"
checkttymatch 0 "LIBSET.SV   5"
checkttymatch 0 "BUILD .SV  33"
checkttymatch 0 "SET   .SV  14"
checkttymatch 0 "SRCCOM.SV   5"
checkttymatch 0 "BITMAP.SV   5"
checkttymatch 0 "DTCOPY.SV   5"
checkttymatch 0 "TDCOPY.SV   7"
checkttymatch 0 "DTFRMT.SV   7"
checkttymatch 0 "TDFRMT.SV   9"
checkttymatch 0 "RXCOPY.SV   6"
checkttymatch 0 "MCPIP .SV  13"
checkttymatch 0 "CAMP  .SV  13"
checkttymatch 0 "EPIC  .SV  14"
checkttymatch 0 "PIP10 .SV  17"
checkttymatch 0 "HELP  .SV   8"
checkttymatch 0 "RKLFMT.SV   9"
checkttymatch 0 "LIB8  .RL  29"
checkttymatch 0 "BUILD .XX  33"
checkttymatch 0 " 282 FREE BLOCKS"
checkttyprompt "." "R PIP"
checkttymatch 0 "*"
sendchartottykb "\003"
checkttymatch 0 "^C."
puts ""
puts "SUCCESS!"
exit
