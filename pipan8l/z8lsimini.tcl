;# get regular pipan8l init file
set ret [source "[file dirname [info script]]/pipan8lini.tcl"]
;# only wait 1mS for flick switches
set bncyms 1

;# function to open tty port available as pipes
;# overrides pipan8lini.tcl's openttypipes
proc openttypipes {} {
    global wrkbpipe rdprpipe
    lassign [chan pipe] rdkbpipe wrkbpipe
    lassign [chan pipe] rdprpipe wrprpipe
    chan configure $rdkbpipe -translation binary
    chan configure $wrkbpipe -translation binary
    chan configure $rdprpipe -translation binary
    chan configure $wrprpipe -translation binary
    chan configure $rdprpipe -blocking 0
    exec ./z8ltty -cps [getenv z8ltty_cps 10] -killit <@ $rdkbpipe >@ $wrprpipe &
}

return $ret
