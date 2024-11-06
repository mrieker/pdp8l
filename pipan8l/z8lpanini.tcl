;# get regular pipan8l init file
set ret [source "[file dirname [info script]]/pipan8lini.tcl"]
;# only wait 1mS for flick switches
set bncyms 1

;# function to open tty port available as pipes
proc openttypipes {} {
    global wrkbpipe rdprpipe
    lassign [chan pipe] rdkbpipe wrkbpipe
    lassign [chan pipe] rdprpipe wrprpipe
    chan configure $rdkbpipe -translation binary
    chan configure $wrkbpipe -translation binary
    chan configure $rdprpipe -translation binary
    chan configure $wrprpipe -translation binary
    chan configure $rdprpipe -blocking 0
    exec ./z8ltty <@ $rdkbpipe >@ $wrprpipe &
}

# close the tty pipes
proc closettypipes {} {
    global wrkbpipe rdprpipe
    chan close $wrkbpipe
    while {[readttychartimed 100] != ""} { }
    chan close $rdprpipe
    set wrkbpipe ""
    set rdprpipe ""
}

;# read character from tty with timeout
proc readttychartimed {msec} {
    global rdprpipe
    for {set i 0} {$i < $msec} {incr i} {
        set ch [chan read $rdprpipe 1]
        if {$ch != ""} {return $ch}
        after 1
    }
    return ""
}

;# function to wait for the given string from the tty
proc waitforttypr {msec str} {
    set len [string length $str]
    for {set i 0} {$i < $len} {incr i} {
        while true {
            set ch [readttychartimed $msec]
            if {$ch > " "} break
            if {$i > 0} break
        }
        set ex [string index $str $i]
        if {$ch != $ex} {
            puts "waitforttypr: expected char [escapechr $ex] on tty but got [escapechr $ch]"
            exit 1
        }
        puts "waitforttypr*: matched char [escapechr $ex] on tty"
    }
}

;# function to send the given string to the tty
;# also checks for echoing
proc sendtottykb {str} {
    global wrkbpipe
    set len [string length $str]
    for {set i 0} {$i < $len} {incr i} {
        after 1500
        set ex [string index $str $i]
        puts -nonewline $wrkbpipe $ex
        chan flush $wrkbpipe
        while true {
            set ch [readttychartimed 1000]
            if {$ch == ""} {
                puts "sendtottykb: timed out receiving echo"
                exit 1
            }
            if {$ch > " "} break
            if {$ch == $ex} break
            if {$i > 0} break
            puts "sendtottykb*: tossing char [escapechr $ch]"
        }
        if {$ch != $ex} {
            puts "sendtottykb: sent char [escapechr $ex] to tty kb but echoed as [escapechr $ch]"
            exit 1
        }
        puts "sendtottykb*: sent char [escapechr $ex] to tty"
    }
}

;# convert given character to escaped form if necessary
proc escapechr {ch} {
    if {$ch == ""} {return "\\z"}
    scan $ch "%c" ii
    if {$ii == 10} {return "\\n"}
    if {$ii == 13} {return "\\r"}
    if {($ii <= 31) || ($ii >= 127)} {return [format "\\%03o" $ii]}
    return $ch
}

return $ret
