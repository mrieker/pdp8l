setsw mprt 0    ;# force loader init
puts ""
puts "= = = = = = = = = = = = = = = = ="
puts "  D01B"
stopandreset
source run-d01b.tcl
puts ""
puts "= = = = = = = = = = = = = = = = ="
puts "  D02B"
stopandreset
source run-d02b.tcl
puts ""
puts "= = = = = = = = = = = = = = = = ="
puts "  D04B"
stopandreset
source run-d04b.tcl
puts ""
puts "= = = = = = = = = = = = = = = = ="
puts "  D05B"
stopandreset
source run-d05b.tcl
puts ""
puts "= = = = = = = = = = = = = = = = ="
puts "  D07B"
stopandreset
source run-d07b.tcl
puts ""
puts "= = = = = = = = = = = = = = = = ="
puts "  D1EB"
stopandreset
source run-d1eb.tcl
puts ""
puts "= = = = = = = = = = = = = = = = ="
puts "  D1GB"
stopandreset
source run-d1gb.tcl
puts ""
puts "= = = = = = = = = = = = = = = = ="
puts "  D3RA"
stopandreset
source run-d3ra.tcl
puts ""
puts "= = = = = = = = = = = = = = = = ="
puts "  OS8DPACK"
stopandreset
source testos8dpack.tcl
puts ""
puts "= = = = = = = = = = = = = = = = ="
puts "  OS8DTAPE"
stopandreset
source testos8dtape.tcl
puts ""
puts "= = = = = = = = = = = = = = = = ="
puts "ALL TESTS SUCCESSFUL!"
stopandreset
exit 0
