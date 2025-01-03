;# get regular pipan8l init file
set ret [source "[file dirname [info script]]/pipan8lini.tcl"]

;# wait 20mS for flick switches
;# - 20mS in case z8lmctrace is running
;#   10mS misses some flicks
set bncyms 20

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
    set ttypid [exec ./z8ltty -cps 30 -killit <@ $rdkbpipe >@ $wrprpipe &]
    atexit "exec kill $ttypid"
}

return $ret
