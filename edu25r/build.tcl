#
#  Build EDU25R from source using OS/8
#

# look for single-character prompt 'pmt' at beginning of line
# skip any other output until then
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

# script start here

source "$Z8LHOME/os8lib.tcl"
hardreset

# start with tangentsoft OS/8 disk if we don't already have an edu25r disk set up
set home [getenv HOME /tmp]
set edudisk "$home/edu25r-rkab0.rk05"
if {! [file exists $edudisk]} {
    exec -ignorestderr wget -nv -O $edudisk.temp https://tangentsoft.com/pidp8i/uv/ock.rk05
    exec mv $edudisk.temp $edudisk
}

# load that disk in drive 0
exec $Z8LHOME/z8lrk8je -killit -loadrw 0 $edudisk &
after 1000

# boot disk
pin set xm_enlo4k 1
pin set xm_os8zap 1
startat [os8dpacktoggleinboot]

# delete any trace of edu25r from the disk
lookforprompt "."
checkttyprompt "" "DEL EDU25R.*"
lookforprompt "."

# download edu25r source file
checkttyprompt "" "R PIP"
checkttyprompt "*" "EDU25R.PA<PTR:"
exec cp edu25r.pal /tmp/edu25r.pal
exec $Z8LHOME/z8lptr -cps 900 -killit -text /tmp/edu25r.pal &
checkttyprompt "^" ""
lookforprompt "*" 300000
sendchartottykb "\003"
lookforprompt "."

# compile and link edu25r
checkttyprompt "" "PAL EDU25R,EDU25R<EDU25R" 300000
checkttyprompt "." "LOAD EDU25R" 300000
checkttyprompt "." "SAVE SYS:EDU25R;20200=1000"
checkttyprompt "." "DIR EDU25R.*"
lookforprompt "."

# retrieve listing file
checkttyprompt "" "R PIP"
set ptppid [exec $Z8LHOME/z8lptp -clear -cps 900 -killit -text $home/edu25r.lis &]
checkttyprompt "*" "PTP:<EDU25R.LS"
checkttyprompt "*" "" 750000
exec kill -INT $ptppid
sendchartottykb "\003"
lookforprompt "."

