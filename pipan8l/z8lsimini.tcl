;# get regular pipan8l init file
set ret [source "[file dirname [info script]]/pipan8lini.tcl"]
;# only wait 1mS for flick switches
set bncyms 1

;# function to open tty port available as pipes
;# overrides pipan8lini.tcl's openttypipes
proc openttypipes {{z8lttyopts {}}} {
    global wrkbpipe rdprpipe
    lassign [chan pipe] rdkbpipe wrkbpipe
    lassign [chan pipe] rdprpipe wrprpipe
    chan configure $rdkbpipe -translation binary
    chan configure $wrkbpipe -translation binary
    chan configure $rdprpipe -translation binary
    chan configure $wrprpipe -translation binary
    chan configure $rdprpipe -blocking 0
    exec ./z8ltty -cps 30 -killit <@ $rdkbpipe >@ $wrprpipe &
}

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
            puts "checkttymatch: timed out waiting for '$line'"
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
            while true {
                set ch [readttychartimed $tmsec]
                if {$ch == ""} break
                puts -nonewline $ch
                flush stdout
                if {$ch == "\n"} break
            }
            puts ""
            puts "checkttymatch: failed to match '$line'"
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

# read rest of line from tty printer
proc getrestofttyline {{tmsec 30000}} {
    set skipped ""
    while true {
        set ch [readttychartimed $tmsec]
        if {$ch == ""} {
            puts ""
            puts "getrestofttyline: timed out reading to end of line"
            exit 1
        }
        puts -nonewline $ch
        flush stdout
        if {$ch == "\r"} break
        if {$ch == "\n"} break
        append skipped $ch
    }
    return $skipped
}

return $ret
