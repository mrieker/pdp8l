
# help for commands defined herein
proc helpini {} {
    puts ""
    puts "  assem <addr> <opcd> ... - assemble and write to memory"
    puts "  chartoint               - convert character to integer"
    puts "  checkttymatch           - check for matching tty printer characters"
    puts "  checkttyprompt          - check tty for prompt and send reply string"
    puts "  closettypipes           - close pipes opened by openttypipes"
    puts "  disas <start> <stop>    - disassemble given range"
    puts "  dumpit                  - dump registers and switches"
    puts "  dumpmem <start> <stop>  - dump memory in octal"
    puts "  escapechr               - convert character to escape"
    puts "  flicksw <switch>        - flick momentary switch on then off"
    puts "  getenv                  - get environment variable"
    puts "  getrestofttyline        - read rest of line from tty"
    puts "  inttochar               - convert integer to character"
    puts "  loadbin <filename>      - load bin file, verify, return start address"
    puts "  loadrim <filename>      - load rim file, verify"
    puts "  loop52                  - set up and start 5252: jmp 5252"
    puts "  octal <val>             - convert value to 4-digit octal string"
    puts "  openttypipes            - access tty device pipes"
    puts "  postinc <var>           - increment var but return its previous value"
    puts "  rdmem <addr>            - read memory at the given address"
    puts "  readloop <addr> ...     - continuously read the addresses until CTRLC"
    puts "  rimloader <lo|hi>       - deposit rim loader in memory"
    puts "  sendchartottykb         - send char to tty kb, don't check for echo"
    puts "  sendtottykb             - send string to tty keyboard and check echo"
    puts "  startat <addr>          - load pc and start at given address"
    puts "  stepit                  - step one cycle then dump"
    puts "  steploop                - step one cycle, dump, then loop on enter key"
    puts "  stopandreset            - stop and reset cpu"
    puts "  wait                    - wait for CTRLC or STOP"
    puts "  waitforttypr            - wait for prompt string on tty"
    puts "  wrmem <addr> <data>     - write memory at the given address"
    puts "  zeromem <start> <stop>  - zero the given address range"
    puts ""
    puts "  global vars"
    puts "    bncyms                - milliseconds for de-bouncing circuit"
    puts ""
}

# assemble instruction and write to memory
# - assem 0200 tadi 0376
# - assem 0201 sna cla
proc assem {addr opcd args} {
    wrmem $addr [assemop $addr $opcd $args]
}

# convert character to integer
proc chartoint {cc} {
    if {$cc == ""} {return -1}
    scan $cc "%c" ii
    return [expr {$ii & 0377}]
}

# check tty output for match
# - call openttypipes first
# - ignores control characters and whitespace
# - skips up to nskip printables at the beginning
# - returns string skipped over
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

# check tty prompt string for match then send given reply followed by <CR>
# - call openttypipes first
# - always ignore control and whitespace characters in prompt string
proc checkttyprompt {prompt reply {tmsec 30000}} {
    # check prompt string
    checkttymatch 0 $prompt $tmsec
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
    after 456
    puts -nonewline $wrkbpipe "\r"
    chan flush $wrkbpipe
}

# close the tty pipes (see openttypipes)
proc closettypipes {} {
    global wrkbpipe rdprpipe
    chan close $wrkbpipe
    while {[readttychartimed 100] != ""} { }
    chan close $rdprpipe
    set wrkbpipe ""
    set rdprpipe ""
}

# disassemble block of memory
# - disas addr         ;# disassemble in vacinity of given address
# - disas start stop   ;# disassemble the given range
proc disas {start {stop ""}} {
    # if just one arg, make the range start-8..start+8
    if {$stop == ""} {
        set stop  [expr {(($start & 007777) > 07767) ? ($start | 000007) : ($start + 010)}]
        set start [expr {(($start & 007777) < 00010) ? ($start & 077770) : ($start - 010)}]
    }
    # loop through the range, inclusive
    for {set pc $start} {$pc <= $stop} {incr pc} {
        set op [rdmem $pc]
        set as [disasop $op $pc]
        puts [format "%04o  %04o  %s" $pc $op $as]
    }
}

# dump block of memory
# - dumpmem start stop
proc dumpmem {start stop} {
    set start [expr {$start & 077770}]
    for {set addr $start} {$addr <= $stop} {incr addr 8} {
        puts -nonewline [format "  %05o " $addr]
        for {set i 0} {$i < 8} {incr i} {
            puts -nonewline [format " %04o" [rdmem [expr {$addr+$i}]]]
        }
        puts ""
    }
}

# dump registers and switches
proc dumpit {} {
    flushit

    set ac   [getreg ac]
    set ir   [getreg ir]
    set ma   [getreg ma]
    set mb   [getreg mb]
    set st   [getreg state]
    set ea   [getreg ea]
    set ion  [getreg ion]
    set link [getreg link]
    set run  [getreg run]

    set sw "SR=[octal [getsw sr]]"
    set sw [expr {[getsw cont]  ? "$sw CONT"  : "$sw"}]
    set sw [expr {[getsw dep]   ? "$sw DEP"   : "$sw"}]
    set sw [expr {[getsw dfld]  ? "$sw DFLD"  : "$sw"}]
    set sw [expr {[getsw exam]  ? "$sw EXAM"  : "$sw"}]
    set sw [expr {[getsw ifld]  ? "$sw IFLD"  : "$sw"}]
    set sw [expr {[getsw mprt]  ? "$sw MPRT"  : "$sw"}]
    set sw [expr {[getsw start] ? "$sw START" : "$sw"}]
    set sw [expr {[getsw step]  ? "$sw STEP"  : "$sw"}]
    set sw [expr {[getsw stop]  ? "$sw STOP"  : "$sw"}]

    set mne [string range "ANDTADISZDCAJMSJMPIOTOPR" [expr {$ir * 3}] [expr {$ir * 3 + 2}]]

    return [format "  %s %s ST=%-2s AC=%o.%04o MA=%o.%04o MB=%04o IR=%o  %s  %s" \
        [expr {$run ? "RUN" : "   "}] [expr {$ion ? "ION" : "   "}] $st $link $ac $ea $ma $mb $ir $mne $sw]
}

# dump the raw gpio pins
proc dumppins {} {
    puts "       U1            U2            U3            U4            U5"
    for {set row 0} {$row < 8} {incr row} {
        for {set col 0} {$col < 10} {incr col} {
            set pinofs [expr {($col & 1) ? 7 - $row : 8 + $row}]
            set pinnum [expr {($col >> 1) * 16 + $pinofs}]
            set pinval [getpin $pinnum]
            set pinwrt [setpin $pinnum $pinval]
            if {! ($col & 1)} {puts -nonewline "  "}
            puts -nonewline [format " %2d=%d%s" $pinnum $pinval [expr {($pinwrt < 0) ? " " : "*"}]]
        }
        puts ""
    }
}

;# convert given character to escaped form if necessary
proc escapechr {ch} {
    if {$ch == ""} {return "\\d"}
    set ii [chartoint $ch]
    if {$ii == 10} {return "\\n"}
    if {$ii == 13} {return "\\r"}
    if {($ii <= 31) || ($ii >= 127)} {return [format "\\%03o" $ii]}
    return [string range $ch 0 0]
}

# flick momentary switch on then off
proc flicksw {swname} {
    global bncyms
    setsw $swname 1
    flushit
    setsw bncy 1
    flushit
    after $bncyms
    setsw bncy 0
    flushit
    setsw $swname 0
    flushit
    after $bncyms
}

# get environment variable, return default value if not defined
proc getenv {varname {defvalu ""}} {
    return [expr {[info exists ::env($varname)] ? $::env($varname) : $defvalu}]
}

# read rest of line from tty printer, terminated by <CR> or <LF>
# - call openttypipes first
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

# convert integer to character
proc inttochar {ii} {
    if {$ii < 0} {return ""}
    return [format "%c" [expr {$ii & 0377}]]
}

# load bin format tape file, return start address
#  returns
#   string: error message
#       -1: successful, no start address
#     else: successful, start address
proc loadbin {fname} {
    set fp [open $fname rb]

    stopandreset

    set rubbingout 0
    set inleadin 1
    set state -1
    set addr 0
    set data 0
    set chksum 0
    set offset -1
    set start -1
    set nextaddr -1
    set verify [dict create]

    set field 0
    setsw ifld 0
    setsw dfld 0

    puts "loadbin: loading $fname..."

    while {true} {
        if {[ctrlcflag]} {
            return "control-C"
        }

        # read byte from tape
        incr offset
        set ch [read $fp 1]
        if {[eof $fp]} {
            close $fp
            puts ""
            return "eof reading loadfile at $offset"
        }
        set ch [chartoint $ch]

        # ignore anything between pairs of rubouts
        if {$ch == 0377} {
            set rubbingout [expr 1 - $rubbingout]
            continue
        }
        if $rubbingout continue

        # 03x0 sets field to 'x'
        # not counted in checksum
        if {($ch & 0300) == 0300} {
            set field [expr {($ch & 0070) >> 3}]
            continue
        }

        # leader/trailer is just <7>
        if {$ch == 0200} {
            if $inleadin continue
            break
        }
        set inleadin 0

        # no other frame should have <7> set
        if {$ch & 0200} {
            close $fp
            puts ""
            return [format "bad char %03o at %d" $ch $offset]
        }

        # add to checksum before stripping <6>
        incr chksum $ch

        # state 4 means we have a data word assembled ready to go to memory
        # it also invalidates the last address as being a start address
        # and it means the next byte is the first of a data pair
        if {$state == 4} {
            dict set verify $addr $data
            puts -nonewline [format "  %05o / %04o\r" $addr $data]
            flush stdout

            # do 'load address' if not sequential
            if {$nextaddr != $addr} {
                setsw dfld [expr {$addr >> 12}]
                setsw ifld [expr {$addr >> 12}]
                setsw sr [expr {$addr & 07777}]
                flicksw ldad
            }

            # deposit the data
            setsw sr $data
            flicksw dep

            # verify resultant lights
            set actma [getreg ma]
            set actmb [getreg mb]
            if {($actma != ($addr & 007777)) || ($actmb != $data)} {
                close $fp
                puts ""
                return [format "%04o %04o showed %04o %04o" $addr $data $actma $actmb]
            }

            # set up for next byte
            set addr [expr {($addr & 070000) | (($addr + 1) & 007777)}]
            set start -1
            set state 2
            set nextaddr $addr
        }

        # <6> set means this is first part of an address
        if {$ch & 0100} {
            set state 0
            incr ch -0100
        }

        # process the 6 bits
        switch $state {
            -1 {
                close $fp
                puts ""
                return [format "bad leader char %03o at %d" $ch $offset]
            }

            0 {
                # top 6 bits of address are followed by bottom 6 bits
                set addr [expr {($field << 12) | ($ch << 6)}]
                set state 1
            }

            1 {
                # bottom 6 bits of address are followed by top 6 bits data
                # it is also the start address if it is last address on tape and is not followed by any data other than checksum
                incr addr $ch
                set start $addr
                set state 2
            }

            2 {
                # top 6 bits of data are followed by bottom 6 bits
                set data [expr {$ch << 6}]
                set state 3
            }

            3 {
                # bottom 6 bits of data are followed by top 6 bits of next word
                # the data is stored in memory when next frame received,
                # as this is the checksum if it is the very last data word
                incr data $ch
                set state 4
            }

            default abort
        }
    }

    close $fp
    puts ""

    # trailing byte found, validate checksum
    set chksum [expr {$chksum - ($data & 63)}]
    set chksum [expr {$chksum - ($data >> 6)}]
    set chksum [expr {$chksum & 07777}]
    if {$chksum != $data} {
        return [format "checksum calculated %04o, given on tape %04o" $chksum $data]
    }

    # verify what was loaded
    return [loadverify $verify $start]
}

# load rim format tape file
#  returns
#   string: error message
#       "": successful
proc loadrim {fname} {
    set fp [open $fname rb]

    stopandreset

    set addr 0
    set data 0
    set offset -1
    set nextaddr -1
    set verify [dict create]

    setsw ifld 0
    setsw dfld 0

    puts "loadrim: loading $fname..."

    while {true} {
        if {[ctrlcflag]} {
            return "control-C"
        }

        # read byte from tape
        incr offset
        set ch [read $fp 1]
        if {[eof $fp]} break
        set ch [chartoint $ch]

        # ignore rubouts
        if {$ch == 0377} continue

        # keep skipping until we have an address
        if {! ($ch & 0100)} continue

        set addr [expr {($ch & 077) << 6}]

        incr offset
        set ch [read $fp 1]
        if {[eof $fp]} {
            close $fp
            puts ""
            return "eof reading loadfile at $offset"
        }
        set ch [chartoint $ch]
        set addr [expr {$addr | ($ch & 077)}]

        incr offset
        set ch [read $fp 1]
        if {[eof $fp]} {
            close $fp
            puts ""
            return "eof reading loadfile at $offset"
        }
        set ch [chartoint $ch]
        set data [expr {($ch & 077) << 6}]

        incr offset
        set ch [read $fp 1]
        if {[eof $fp]} {
            close $fp
            puts ""
            return "eof reading loadfile at $offset"
        }
        set ch [chartoint $ch]
        set data [expr {$data | ($ch & 077)}]

        dict set verify $addr $data
        puts -nonewline [format "  %04o / %04o\r" $addr $data]
        flush stdout

        # do 'load address' if not sequential
        if {$nextaddr != $addr} {
            setsw sr $addr
            flicksw ldad
        }

        # deposit the data
        setsw sr $data
        flicksw dep

        # verify resultant lights
        set actma [getreg ma]
        set actmb [getreg mb]
        if {($actma != $addr) || ($actmb != $data)} {
            close $fp
            puts ""
            return [format %04o %04o showed %04o %04o" $addr $data $actma $actmb]
        }

        set nextaddr [expr {($addr + 1) & 07777}]
    }

    close $fp
    puts ""

    # verify what was loaded
    return [loadverify $verify ""]
}

# verify memory loaded by bin/rim
#  input:
#   verify = dictionary of addr => data as written to memory
#   retifok = value to return if verification successful
#  output:
#   returns error: error message string
#            else: $retifok
proc loadverify {verify retifok} {
    puts "loadverify: verifying..."
    dict for {xaddr expect} $verify {
        if {[ctrlcflag]} {
            return "control-C"
        }
        set actual [rdmem $xaddr]
        puts -nonewline [format "  %05o / %04o\r" $xaddr $actual]
        flush stdout
        if {$actual != $expect} {
            puts ""
            return [format "%05o was %04o expected %04o" $xaddr $actual $expect]
        }
    }
    puts ""
    puts "loadverify: verify ok"
    return $retifok
}

# set up 5252: jmp 5252 and start it
proc loop52 {} {
    stopandreset
    setsw sr 05252
    flicksw ldad
    flicksw dep
    flicksw ldad
    flicksw start
}

# convert integer to 4-digit octal string
proc octal {val} {
    return [format %04o $val]
}

;# function to open tty port available as pipes
;# - using pipan8l with real PDP-8/L: softlink pipan8l_ttykb and _ttypr to real tty port eg /dev/ttyACM0
;# - using pipan8l with built-in simulator (-sim option): simlib.cc creates named pipes pipan8l_ttykb and _ttypr
;# - using pipan8l on zynq with pdp8ltty.v (started via z8lsim): overridden by z8lsimini.tcl
proc openttypipes {} {
    global wrkbpipe rdprpipe
    set rdprpipe [open "pipan8l_ttypr" "r"]
    set wrkbpipe [open "pipan8l_ttykb" "w"]
    chan configure $rdprpipe -translation binary
    chan configure $wrkbpipe -translation binary
    chan configure $rdprpipe -blocking 0
    while {[readttychartimed 500] != ""} { }
}

# increment a variable but return its previous value
# useful for doing series of assem commands
# http://www.tcl.tk/man/tcl8.6/TclCmd/upvar.htm
proc postinc name {
    upvar $name x
    set oldval $x
    incr x
    return $oldval
}

;# read character from tty with timeout (see openttypipes)
proc readttychartimed {msec} {
    global rdprpipe
    while true {
        set ii [readchar $rdprpipe $msec]
        if {$ii == ""} return ""
        set ii [expr {$ii & 0177}]
        if {$ii != 0} {return [inttochar $ii]}
    }
}

# deposit rim loader in memory
# returns start address
#  speed = "lo" or "hi"
proc rimloader {speed} {
    stopandreset
    switch $speed {
        "lo" {
            wrmem 07756 06032
            wrmem 07757 06031
            wrmem 07760 05357
            wrmem 07761 06036
            wrmem 07762 07106
            wrmem 07763 07006
            wrmem 07764 07510
            wrmem 07765 05357
            wrmem 07766 07006
            wrmem 07767 06031
            wrmem 07770 05367
            wrmem 07771 06034
            wrmem 07772 07420
            wrmem 07773 03776
            wrmem 07774 03376
            wrmem 07775 05356
            wrmem 07776 00000
        }
        "hi" {
            wrmem 07756 06014
            wrmem 07757 06011
            wrmem 07760 05357
            wrmem 07761 06016
            wrmem 07762 07106
            wrmem 07763 07006
            wrmem 07764 07510
            wrmem 07765 05374
            wrmem 07766 07006
            wrmem 07767 06011
            wrmem 07770 05367
            wrmem 07771 06016
            wrmem 07772 07420
            wrmem 07773 03776
            wrmem 07774 03376
            wrmem 07775 05357
            wrmem 07776 00000
        }
        default {
            return "bad speed $speed"
        }
    }
    disas 07756 07775
    return 07756
}

# read memory location
# - does loadaddress which reads the location
proc rdmem {addr} {
    setsw ifld [expr {$addr >> 12}]
    setsw dfld [expr {$addr >> 12}]
    setsw sr [expr {$addr & 07777}]
    flicksw ldad
    return [getreg mb]
}

# send character to tty keyboard pipe (see openttypipes)
# does not wait for echo
proc sendchartottykb {ch} {
    global wrkbpipe
    puts -nonewline $wrkbpipe $ch
    chan flush $wrkbpipe
}

;# function to send the given string to the tty (see openttypipes)
;# also checks for echoing
proc sendtottykb {str} {
    global wrkbpipe
    set len [string length $str]
    for {set i 0} {$i < $len} {incr i} {
        after 1500
        set ex [string index $str $i]
        set ix [chartoint $ex]
        set ch [inttochar [expr {$ix | 0200}]]
        puts -nonewline $wrkbpipe $ch
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
        }
        if {$ch != $ex} {
            puts "sendtottykb: sent char [escapechr $ex] to tty kb but echoed as [escapechr $ch]"
            exit 1
        }
    }
}

# load PC and start at given address
# preserves switchregister contents
proc startat {addr} {
    set savesr [getsw sr]
    setsw ifld [expr {$addr >> 12}]
    setsw dfld [expr {$addr >> 12}]
    setsw sr [expr {$addr & 07777}]
    flicksw ldad
    setsw sr $savesr
    flicksw start
}

# step single cycle then dump front panel
proc stepit {} {
    setsw step 1
    flicksw cont
    puts [dumpit]
}

proc steploop {} {
    puts "<enter> to keep stepping, anything<enter> to stop"
    setsw step 1
    while {! [ctrlcflag]} {
        flicksw cont
        puts -nonewline "[dumpit]  > "
        flush stdout
        set line [gets stdin]
        if {$line != ""} break
    }
}

# stop and reset cpu
proc stopandreset {} {
    setsw step 1
    flicksw stop
    for {set i 0} {[getreg run]} {incr i} {
        if {$i > 1000} {
            puts "stopandreset: cpu did not stop"
            exit 1
        }
        flushit
    }
    set oldmem0 [rdmem 0]
    assem 0 HLT
    startat 0
    for {set i 0} {[getreg run]} {incr i} {
        if {$i > 1000} {
            puts "stopandreset: cpu did not halt"
            exit 1
        }
        flushit
    }
    wrmem 0 $oldmem0
    setsw step 0
}

# wait for control-C or processor stopped
# returns "CTRLC" or "STOP"
proc wait {} {
    while {[getreg run]} {
        after 100
        flushit
        if [ctrlcflag] {return "CTRLC"}
    }
    return "STOP"
}

;# function to wait for the given string from the tty (see openttypipes)
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
    }
}

# write memory location
# - does loadaddress, then deposit to write
proc wrmem {addr data args} {
    setsw ifld [expr {$addr >> 12}]
    setsw dfld [expr {$addr >> 12}]
    setsw sr [expr {$addr & 07777}]
    flicksw ldad
    setsw sr $data
    flicksw dep
}

# zero block of memory
# - zeromem start stop
proc zeromem {start stop} {
    setsw sr $start
    flicksw ldad
    setsw sr 0
    for {set addr $start} {$addr <= $stop} {incr addr} {
        if {$addr % 8 == 0} {puts [format "%04o" $addr]}
        flicksw dep
    }
}

# milliseconds to hold a momentary switch on
set bncyms [getenv bncyms [expr {([libname] == "sim") ? 1 : 120}]]

# make sure processor is stopped
# and turn off all the other switches
setsw cont  0
setsw dep   0
setsw dfld  0
setsw exam  0
setsw ifld  0
setsw ldad  0
setsw mprt  0
setsw start 0
setsw step  0
flicksw stop

# message displayed as part of help command
return "also, 'helpini' will print help for pipan8lini.tcl commands"
