
# help for commands defined herein
proc helpini {} {
    puts ""
    puts "  assem <addr> <opcd> ... - assemble and write to memory"
    puts "  disas <start> <stop>    - disassemble given range"
    puts "  dumpit                  - dump registers and switches"
    puts "  flicksw <switch>        - flick momentary switch on then off"
    puts "  loadbin <filename>      - load bin file, verify, return start address"
    puts "  loadrim <filename>      - load rim file, verify"
    puts "  octal <val>             - convert value to 4-digit octal string"
    puts "  postinc <var>           - increment var but return its previous value"
    puts "  rdmem <addr>            - read memory at the given address"
    puts "  readloop <addr> ...     - continuously read the addresses until CTRLC"
    puts "  startat <addr>          - load pc and start at given address"
    puts "  stepit                  - step one cycle then dump"
    puts "  steploop                - step one cycle, dump, then loop on enter key"
    puts "  wait                    - wait for CTRLC or STOP"
    puts "  wrmem <addr> <data>     - write memory at the given address"
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

# disassemble block of memory
# - disass addr         ;# disassemble in vacinity of given address
# - disass start stop   ;# disassemble the given range
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

# load bin format tape file, return start address
#  returns
#   string: error message
#       -1: successful, no start address
#     else: successful, start address
proc loadbin {fname} {

    set fp [open $fname rb]

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
        scan $ch "%c" ch

        # ignore anything between pairs of rubouts
        if {$ch == 0377} {
            set rubbingout [expr 1 - $rubbingout]
            continue
        }
        if $rubbingout continue

        # 03x0 sets field to 'x'
        # not counted in checksum
        if {($ch & 0300) == 0300} {
            set field [expr ($ch & 0070) << 9]
            setsw ifld $field
            setsw dfld $field
            set nextaddr -1
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
        # and it means the next word is the first of a data pair
        if {$state == 4} {
            dict set verify $addr $data
            puts -nonewline [format "  %o.%04o / %04o\r" $field $addr $data]
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
            set actea $field ;##;;TODO; [getreg ea]
            set actma [getreg ma]
            set actmb [getreg mb]
            if {($actea != $field) || ($actma != $addr) || ($actmb != $data)} {
                close $fp
                puts ""
                return [format "%o.%04o %04o showed %o.%04o %04o" $field $addr $data $actea $actma $actmb]
            }

            # set up for next byte
            set addr [expr {($addr + 1) & 07777}]
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
                set addr [expr {$ch << 6}]
                set state 1
            }

            1 {
                # bottom 6 bits of address are followed by top 6 bits data
                # it is also the start address if it is last address on tape and is not followed by any data other than checksum
                incr addr $ch
                set start [expr {$field | $addr}]
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
        scan $ch "%c" ch

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
        scan $ch "%c" ch
        set addr [expr {$addr | ($ch & 077)}]

        incr offset
        set ch [read $fp 1]
        if {[eof $fp]} {
            close $fp
            puts ""
            return "eof reading loadfile at $offset"
        }
        scan $ch "%c" ch
        set data [expr {($ch & 077) << 6}]

        incr offset
        set ch [read $fp 1]
        if {[eof $fp]} {
            close $fp
            puts ""
            return "eof reading loadfile at $offset"
        }
        scan $ch "%c" ch
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
    dict for {addr expect} $verify {
        set actual [rdmem $addr]
        puts -nonewline [format "  %04o / %04o\r" $addr $actual]
        flush stdout
        if {$actual != $expect} {
            puts ""
            return [format "%04o was %04o expected %04o" $addr $actual $expect]
        }
    }
    puts ""
    puts "loadverify: verify ok"
    return $retifok
}

# convert integer to 4-digit octal string
proc octal {val} {
    return [format %04o $val]
}

# increment a variable but return its previous value
# http://www.tcl.tk/man/tcl8.6/TclCmd/upvar.htm
proc postinc name {
    upvar $name x
    set oldval $x
    incr x
    return $oldval
}

# read memory location
# - does loadaddress which reads the location
proc rdmem {addr} {
    setsw sr $addr
    flicksw ldad
    return [getreg mb]
}

# load PC and start at given address
proc startat {addr} {
    set savesr [getsw sr]
    setsw sr $addr
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

# write memory location
# - does loadaddress, then deposit to write
proc wrmem {addr data args} {
    setsw sr $addr
    flicksw ldad
    setsw sr $data
    flicksw dep
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

# milliseconds to hold a momentary switch on
set bncyms 120

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
puts ""
puts [dumpit]

# message displayed as part of help command
return "also, 'helpini' will print help for pipan8lini.tcl commands"
