#
#  Start EDU20C Basic on disk created by build.tcl
#  Do a simple test then leave it open for use
#

proc lookforprompt {pmt {tmsec 5000}} {
    set atbol 1
    while true {
        set ch [readttychartimed $tmsec]
        if {$ch == ""} {error "timeout waiting for $pmt prompt"}
        puts -nonewline $ch
        flush stdout
        if {$atbol && ($ch == $pmt)} break
        set atbol [expr {$ch == "\n"}]
    }
}

# verify the given prompt string
# send the single-character answer without any <CR>
proc answerprompt {pmt ans} {
    set pmtlen [string length $pmt]
    for {set i 0} {$i < $pmtlen} {incr i} {
        set sb [string index $pmt $i]
        if {$sb > " "} {
            while true {
                set ch [readttychartimed 1000]
                puts -nonewline $ch
                flush stdout
                if {$ch > " "} {
                    if {$ch == $sb} break
                    error "prompt mismatch"
                }
            }
        }
    }

    after 500
    sendchartottykb $ans
    set ch [readttychartimed 1000]
    puts -nonewline $ch
    flush stdout
    if {$ch != $ans} {error "answer echo mismatch - sent <$ans> got <$ch>"}
}

source "$Z8LHOME/os8lib.tcl"

hardreset

set home [getenv HOME /tmp]
set edudisk "$home/edu20c-rkab0.rk05"
if {! [file exists $edudisk]} {
    error "no $edudisk - use ./build.sh to create"
}

exec $Z8LHOME/z8lrk8je -killit -loadrw 0 $edudisk &
after 1000

pin set xm_enlo4k 1
pin set xm_os8zap 1
startat [os8dpacktoggleinboot]

lookforprompt "."
checkttyprompt "" "R EDU20C"

checkttymatch 0 "EDUSYSTEM 20  BASIC"
answerprompt "NUMBER OF USERS (1 TO 8)?" "3"
answerprompt "PDP-8/L COMPUTER (Y OR N)?" "Y"
answerprompt "DO YOU HAVE A HIGH SPEED PUNCH (Y OR N)?" "Y"
answerprompt "DO YOU HAVE A HIGH SPEED READER (Y OR N)?" "Y"
answerprompt "SAME AMOUNT OF STORAGE FOR ALL USERS?" "Y"
answerprompt "IS THE ABOVE CORRECT (Y OR N)?" "Y"
checkttymatch 0 "END OF DIALOGUE"
checkttyprompt "READY" ""
checkttyprompt "" "10 PRINT \"HELLO\""
checkttyprompt "" "LIST"
checkttymatch 0 "10 PRINT \"HELLO\""
checkttyprompt "READY" "RUN"
checkttymatch 0 "HELLO"
puts ""
puts ""
puts "  This terminal is accessible via ../pipan8l/z8ltty -cps 960 -killit -upcase"
puts "  The 2nd is accessible via ../pipan8l/z8ltty -cps 960 -killit -upcase -dc02 0"
puts "  The 3rd is accessible via ../pipan8l/z8ltty -cps 960 -killit -upcase -dc02 1"
puts ""
