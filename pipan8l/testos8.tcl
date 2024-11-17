puts ""
puts "testos8: test running OS/8"

stopandreset
openttypipes

startat [loadbin rk8jeboot.bin]

puts "testos8: starting"

checkttymatch 0 "PiDP-8/I trunk:id\[76133aacd0\] - OS/8 V3D Combined Kit - KBM V3T - CCL V3A"
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
set firstcard [string trim [getrestofttyline]]
checkttymatch 0 "YOUR SECOND CARD IS "
set secondcard [string trim [getrestofttyline]]
checkttymatch 0 "DEALER SHOWS A "
set dealershows [string trim [getrestofttyline]]
checkttymatch 0 "YOU HAVE"
set mytotal [string trim [checkttymatch 5 "SHOWING."]]
checkttyprompt "DO YOU WANT A HIT??" "N"
while true {
    checkttymatch 0 "DEALER HAS  "
    set dealerhas [string trim [getrestofttyline]]
    if {$dealerhas > 21} {
        checkttymatch 0 "DEALER BUSTED."
        break
    }
    if {$dealerhas >= 17} break
    checkttymatch 0 "DEALER GETS A "
    set dealergets [string trim [getrestofttyline]]
}
set ilost [expr {($dealerhas >= $mytotal) && ($dealerhas <= 21)}]
if {$ilost} {
    checkttymatch 0 "YOU LOSE. YOU NOW HAVE \$ 400 "
} else {
    checkttymatch 0 "YOU WIN. YOU NOW HAVE \$ 600 "
}
checkttyprompt "DO YOU WISH TO PLAY AGAIN??" "N"
if {$ilost} {
    checkttymatch 0 "TOO BAD! YOU LOST 100 DOLLARS AT THE EDUSYSTEM CASINO."
} else {
    checkttymatch 0 "NOT BAD! YOU WON 100 DOLLARS AT THE EDUSYSTEM CASINO."
}
checkttymatch 0 "HOPE YOU ENJOYED YOURSELF.  THANKS FOR PLAYING."
checkttyprompt "READY" "BYE"
checkttymatch 0 "."
puts ""
puts "SUCCESS!"
exit
