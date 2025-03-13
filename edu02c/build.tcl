#
#  Build EDU20C from source using OS/8
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

# start with tangentsoft OS/8 disk if we don't already have an edu20c disk set up
set home [getenv HOME /tmp]
set edudisk "$home/edu20c-rkab0.rk05"
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

# delete any trace of EDU20C from the disk
lookforprompt "."
checkttyprompt "" "DEL EDU20C.*"
lookforprompt "."

# download EDU20C source file
checkttyprompt "" "R PIP"
checkttyprompt "*" "EDU20C.PA<PTR:"
exec cp edu20c.pal /tmp/edu20c.pal
exec $Z8LHOME/z8lptr -7bit -cps 900 -inscr -killit /tmp/edu20c.pal &
checkttyprompt "^" ""
lookforprompt "*" 300000
sendchartottykb "\003"
lookforprompt "."

# compile and link EDU20C
checkttyprompt "" "PAL EDU20C,EDU20C<EDU20C" 300000
checkttyprompt "." "LOAD EDU20C=12000" 300000
checkttyprompt "." "SAVE SYS:EDU20C"
checkttyprompt "." "DIR EDU20C.*"
lookforprompt "."

# retrieve listing file
checkttyprompt "" "R PIP"
set ptppid [exec $Z8LHOME/z8lptp -7bit -clear -cps 900 -remcr -remdel -remnul -killit $home/edu20c.lis &]
checkttyprompt "*" "PTP:<EDU20C.LS"
checkttyprompt "*" "" 750000
exec kill -INT $ptppid
sendchartottykb "\003"
lookforprompt "."

