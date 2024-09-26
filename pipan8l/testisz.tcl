set beg 024                     ;# where in memory to start
set tmp [expr {$beg - 1}]       ;# location for tmp counter
set dot $beg                    ;# where we are writing to
assem [postinc dot] cla cll     ;# clear ac and link
assem [postinc dot] dca $tmp    ;# clear memory
set loop $dot
assem [postinc dot] iac         ;# inc ac and link
assem [postinc dot] isz $tmp    ;# inc memory
assem [postinc dot] jmp $loop   ;# repeat if mem non-zero
assem [postinc dot] sna         ;# mem zero, check ac zero
assem [postinc dot] jmp $loop   ;# ac zero, repeat test
assem [postinc dot] hlt         ;# mismatch, halt
assem [postinc dot] jmp $beg    ;# start over
disas $beg [expr {$dot - 1}]
setsw sr $beg
flicksw ldad
flicksw start
while true {
    after 100
    if [ctrlcflag] {
        puts "control-C"
        break
    }
    flushit
    puts [dumppan]
    if {! [getreg run]} {
        puts "stopped"
        break
    }
}
