puts ""
puts "testos8: test running OS/8"

stopandreset
openttypipes

startat [loadbin rk8jeboot.bin]

# check tty output for match
# always ignore control characters and whitespace
# skip up to nskip printables at the beginning
# return string skipped
proc checkttymatch {nskip line {tmsec 30000}} {
    set len [string length $line]
    set nowtsp ""
    for {set i 0} {$i < $len} {incr i} {
        set ch [string index $line $i]
        if {$ch > " "} {append nowtsp $ch}
    }
    set len [string length $nowtsp]
    set nmatched 0
    set skipped ""
    while {$nmatched < $len} {
        set ch [readttychartimed $tmsec]
        if {$ch == ""} {
            puts ""
            puts "checkttymatch: timed out waiting for '$nowtsp'"
            exit 1
        }
        puts -nonewline $ch
        flush stdout
        if {$ch <= " "} continue
        if {$ch == [string index $nowtsp $nmatched]} {
            incr nmatched
            continue
        }
        append skipped [string range $nowtsp 0 [expr {$nmatched - 1}]]
        append skipped $ch
        if {[string length $skipped] > $nskip} {
            puts ""
            puts "checkttymatch: failed to match '$nowtsp'"
            exit 1
        }
    }
    return $skipped
}

# check tty output for match
# always ignore control characters
# skip up to nskip printables at the beginning
# return string skipped
proc checkttyprompt {prompt reply} {
    # check prompt string
    checkttymatch 0 $prompt
    # there may be a CR/LF following the prompt (like 'READY' in BASIC)
    # so wait for a brief time with no characters printed
    while {true} {
        set rb [readttychartimed 1234]
        if {$rb == ""} break
        puts -nonewline $rb
        flush stdout
        if {$rb > " "} {
            puts checkttyprompt: extra char [escapechr $rb] after prompt"
        }
    }
    # send reply string to keyboard incl spaces
    # check for each of each one
    global wrkbpipe
    set len [string length $reply]
    for {set i 0} {$i < $len} {incr i} {
        after 456
        set ch [string index $reply $i]
        puts -nonewline $wrkbpipe $ch
        chan flush $wrkbpipe
        set rb [readttychartimed 1234]
        puts -nonewline $rb
        flush stdout
        if {$rb != $ch} {
            puts ""
            puts "checkttyprompt: char [escapechr $ch] echoed as [escapechr $rb]"
            exit 1
        }
    }
    # send CR at the end, don't bother checking echo in case it echoes something different
    after 1234
    puts -nonewline $wrkbpipe "\r"
    chan flush $wrkbpipe
}

puts "testos8*: starting"

checkttymatch 0 "PiDP-8/I trunk:id\[76133aacd0\] - OS/8 V3D Combined Kit - KBM V3T - CCL V3"
checkttymatch 0 "A"
checkttymatch 0 "Built from source by tangent@pidp8i on 2021.02.14 at 15:37:06 MST"
checkttymatch 0 "Restart address = 07600"
checkttymatch 0 "Type:"
checkttymatch 0 "   .DIR                -  to get a list of files on DSK:"
checkttymatch 0 "   .DIR SYS:           -  to get a list of files on SYS:"
checkttymatch 0 "   .R PROGNAME         -  to run a system program"
checkttymatch 0 "   .HELP FILENAME      -  to type a help file"
checkttyprompt "." "R BASIC"
checkttyprompt "NEW OR OLD--" "OLD"
checkttyprompt "FILE NAME--" "BLKJAK"
checkttyprompt "READY" "RUN"
checkttymatch 0 "BLKJAK  BA    5B    "
checkttymatch 0 "WELCOME TO DIGITAL EDUSYSTEM COMPUTER BLACKJACK!!"
checkttymatch 0 "YOUR DEALER TONIGHT IS PETEY P. EIGHT."
checkttymatch 0 "WATCH HIM CLOSELY.... HE HAS A REPUTATION FOR"
checkttymatch 0 "DEALING OFF THE BOTTOM OF THE DECK."
checkttymatch 0 "QUESTIONS REQUIRING A YES OR NO ANSWER"
checkttymatch 0 "SHOULD BE ANSWERED WITH A 'Y' FOR YES, 'N' FOR NO."
checkttymatch 0 "DON'T START PLAYING WITH LESS THAN \$100.. HAVE FUN!"
checkttyprompt "HOW MANY DOLLARS ARE YOU STARTING WITH?" "500"
checkttyprompt "WHAT IS YOUR WAGER THIS TIME?" "100"
checkttymatch 0 "YOUR FIRST CARD IS "
set firstcard [string trim [checkttymatch 5 "YOUR SECOND CARD IS "]]
set secondcard [string trim [checkttymatch 5 "DEALER SHOWS A "]]
set dealershows [string trim [checkttymatch 5 "YOU HAVE"]]
set mytotal [string trim [checkttymatch 5 "SHOWING."]]
checkttyprompt "DO YOU WANT A HIT??" "N"
checkttymatch 0 "DEALER HAS  "
set dealerhas [string trim [checkttymatch 5 "YOU"]]
checkttymatch 0 "LOSE. YOU NOW HAVE \$ 400 "
checkttyprompt "DO YOU WISH TO PLAY AGAIN??" "N"
checkttymatch 0 "TOO BAD! YOU LOST 100 DOLLARS AT THE EDUSYSTEM CASINO."
checkttymatch 0 "HOPE YOU ENJOYED YOURSELF.  THANKS FOR PLAYING."
checkttymatch 0 "READY"
checkttymatch 0 "BYE"
checkttymatch 0 "."
exit
