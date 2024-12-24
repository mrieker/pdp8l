
# help for commands defined herein
proc helpini {} {
    puts ""
    puts "  getenv                - get environment variable"
    puts "  octal <val>           - convert value to 4-digit octal string"
    puts "  splitcsvline <line>   - split line into list"
    puts ""
    puts "  loopintest <conn> <side> - test input-to-cpu pins"
    puts "                             eg, loopintest C36 1"
    puts ""
    puts "  loopoutest <conn> <side> - test output-from-cpu pins"
    puts "                             eg, loopoutest C36 1"
    puts ""
    puts "  loopdumpac            - continually dump AC as seen on PIOBUS"
    puts ""
    puts "  cmemrd <xaddr>        - read memory via DMA cycle"
    puts "  cmemwr <xaddr> <data> - write memory via DMA cycle"
    puts ""
    puts "  DMA bus"
    puts "    sendmadr            - send DMA ADDR over DMA bus"
    puts "    sendmdat            - send DMA DATA over DMA bus"
    puts ""
    puts "  MEM bus"
    puts "    readma              - read MA from MEM bus"
    puts "    sendmd              - send MD over MEM bus"
    puts ""
    puts "  PIO bus"
    puts "    readac              - read AC from PIO bus"
    puts "    readmb              - read MB from PIO bus"
    puts "    sendio              - send IO data over PIO bus"
    puts ""
    puts "  loadbin <filename>    - load bin-format file into FPGA-provided extmem"
    puts "  loadrim <filename>    - load rim-format file into FPGA-provided extmem"
    puts ""
}

# convert character to integer
proc chartoint {cc} {
    if {$cc == ""} {return -1}
    scan $cc "%c" ii
    return [expr {$ii & 0377}]
}

# loop continually dumping the AC as it appears on the PIOBUS
proc loopdumpac {} {
    pin set bareit 1 x_DMAADDR 1 x_DMADATA 1 x_INPUTBUS 1 x_MEM 1 r_MA 1 r_BMB 1 r_BAC 0
    while {! [ctrlcflag]} {
        after 200
        puts "  PIOBUS/BAC=[octal [pin get bPIOBUS]]  BAC=[octal [pin get oBAC]]"
    }
    pin set bareit 1 x_DMAADDR 1 x_DMADATA 1 x_INPUTBUS 1 x_MEM 1 r_MA 1 r_BMB 1 r_BAC 1
}

# get environment variable, return default value if not defined
proc getenv {varname {defvalu ""}} {
    return [expr {[info exists ::env($varname)] ? $::env($varname) : $defvalu}]
}

# load bin format tape file into extmem, return start address
#  returns
#   string: error message
#       -1: successful, no start address
#     else: successful, start address
proc loadbin {fname} {
    set fp [open $fname rb]

    if {! [pin get o_B_RUN]} {return "processor must be halted"}
    pin set XM_ENLO4K 1     ;# fpga will force processor to use extmem for low 4K

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
            pin set em:$addr $data
            dict set verify $addr $data
            puts -nonewline [format "  %05o / %04o\r" $addr $data]
            flush stdout

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

# load rim format tape file into extmem
#  returns
#   string: error message
#       "": successful
proc loadrim {fname} {
    set fp [open $fname rb]

    if {! [pin get o_B_RUN]} {return "processor must be halted"}
    pin set XM_ENLO4K 1     ;# fpga will force processor to use extmem for low 4K

    set addr 0
    set data 0
    set offset -1
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

        pin set em:$addr $data
        dict set verify $addr $data
        puts -nonewline [format "  %04o / %04o\r" $addr $data]
        flush stdout
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
        set actual [pin get em:$xaddr]
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

# convert integer to 4-digit octal string
proc octal {val} {
    return [format %04o $val]
}

# split csv line into a list of columns
proc splitcsvline {line} {
    set len [string length $line]
    set columns [list]
    set column ""
    set quoted false
    for {set i 0} {$i < $len} {incr i} {
        set ch [string index $line $i]
        if {($ch == ",") && ! $quoted} {
            lappend columns $column
            set column ""
            continue
        }
        if {$ch == "\""} {
            set quoted [expr {! $quoted}]
            continue
        }
        if {$ch == "\\"} {
            set ch [string index $line [incr i]]
        }
        append column $ch
    }
    lappend columns $column
    return $columns
}

## -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

# read memory via DMA cycle
proc cmemrd {xaddr} {

    # get access to CMEM interface and wait for it to be idle
    set mypid [pid]
    for {set i 0} true {incr i} {
        pin set CM3 $mypid
        if {([pin get CM3] == $mypid) && ! [pin get CM_BUSY]} break
        if {$i > 50000} {
            error "timed out locking cmem"
        }
    }

    # send address to read memory via DMA cycle
    pin set CM_ADDR $xaddr set CM_WRITE 0 CM_ENAB 1

    # wait for cycle to complete
    for {set i 0} true {incr i} {
        if {[pin get CM_DONE]} break
        if {$i > 50000} {
            pin set CM_ENAB 0 CM3 $mypid
            error "timed out reading cmem"
        }
    }

    # retrieve data and release interface
    set data [pin get CM_DATA]
    pin set CM_ENAB 0 CM3 $mypid
    return $data
}

# write memory via DMA cycle
proc cmemwr {xaddr data} {

    # get access to CMEM interface and wait for it to be idle
    set mypid [pid]
    for {set i 0} true {incr i} {
        pin set CM3 $mypid
        if {([pin get CM3] == $mypid) && ! [pin get CM_BUSY]} break
        if {$i > 50000} {
            error "timed out locking cmem"
        }
    }

    # send address and data to write memory via DMA cycle
    pin set CM_ADDR $xaddr set CM_DATA $data CM_WRITE 1 CM_ENAB 1
    for {set i 0} true {incr i} {
        if {[pin get CM_DONE]} break
        if {$i > 50000} {
            pin set CM_WRITE 0 CM_ENAB 0 CM3 $mypid
            error "timed out reading cmem"
        }
    }

    # release interface
    pin set CM_WRITE 0 CM_ENAB 0 CM3 $mypid
    return $data
}

## -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

## read/write multiplexed bus signals

# read AC as seen on the PIO bus
# oBAC comes directly from the PIOBUS
# so turn on the r_BAC 74T245s and read it
proc readac {} {
    return [pin set r_BAC 1 r_BMB 1 x_INPUTBUS 1 bareit 1 r_BAC 0 get oBAC set r_BAC 1]
}

# read MB as seen on the PIO bus
# there is a flipflop register between PIOBUS and oBMB that clocks whenever not doing an io pulse
# so reset everything so flipflops clock then turn on the r_BMB 74T254s and read it
proc readmb {} {
    pin set softreset 1 nanocontin 1 simit 0 softreset 0
    pin set r_BAC 1 r_MA 1 x_INPUTBUS 1 bareit 1 r_BMB 0
    return [pin get oBMB set r_BMB 1]
}

# read MA as seen on the MEM bus
# there is a flipflop register between MEMBUS and oMA that clocks whenever r_MA is low
# so reset everything and set r_MA=0 so flipflops clock then read it
proc readma {} {
    pin set softreset 1 nanocontin 1 simit 0 softreset 0
    pin set x_MEM 1 r_BMB 1 bareit 1 r_MA 0
    return [pin get oMA set r_MA 1]
}

# send memory read data out to iMEM via the MEM bus
# iMEM gets gated out to MEMBUS as long as we have x_MEM=0
proc sendmd {md} {
    pin set r_MA 1 x_MEM 0 bareit 1 iMEM $md
}

# send dma address out to i_DMAADDR via the DMA bus
# i_DMAADDR gets gated out to DMABUS as long as we have x_DMAADDR=0
proc sendmadr {addr} {
    pin set x_DMADATA 1 x_DMAADDR 0 bareit 1 i_DMAADDR [expr {$addr ^ 07777}]
}

# send dma data out to i_DMADATA via the DMA bus
# i_DMADATA gets gated out to DMABUS as long as we have x_DMADATA=0
proc sendmdat {data} {
    pin set x_DMAADDR 1 x_DMADATA 0 bareit 1 i_DMADATA [expr {$data ^ 07777}]
}

# turn both dmabus drivers off
proc sendmoff {} {
    pin set x_DMAADDR 1 x_DMADATA 1 bareit 1
}

# send io reply data out to iINPUTBUS via the PIO bus
# iINPUTBUS gets gated out to PIOBUS as long as we have x_INPUTBUS=0
proc sendio {io} {
    pin set r_BAC 1 r_BMB 1 x_INPUTBUS 0 bareit 1 iINPUTBUS $io
}

## -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

## test individual edge-connector pins

# alternate connector 'input-to-cpu' pin 1's and 0's
# step through pins with control-C
#  conn = B34 B35 B36 C35 C36 D34 D35 D36
#  side = 1 or 2
proc loopintest {conn side} {

    # turn all tri-states off
    #  bareit=1 : direct control of tri-state enables
    pin set bareit 1 x_DMAADDR 1 x_DMADATA 1 x_INPUTBUS 1 x_MEM 1 r_BAC 1 r_BMB 1 r_MA 1

    # select pins from csv file
    set csvfile [open "../zturn/signals.csv" "r"]
    set foundpins [list]
    while {! [ctrlcflag]} {
        set csvline [gets $csvfile]
        if {$csvline == ""} break
        set columns [splitcsvline $csvline]
        set signame [lindex $columns 0]
        set connpin [lindex $columns 1]
        if {([string index $signame 0] == "i") && ([string length $connpin] == 6) && ([string range $connpin 0 2] == $conn) && ([string index $connpin 5] == $side)} {
            lappend foundpins "$connpin $signame"
        }
    }
    if {[llength $foundpins] == 0} {
        puts "no pins found"
        return
    }

    # loop through pins in sorted order
    set foundpins [lsort $foundpins]
    foreach cs $foundpins {
        set on 0
        set connpin [string range $cs 0 5]
        set signame [string range $cs 7 end]
        if {[string range $signame 0 8] == "i_DMAADDR"} {pin set x_DMAADDR 0}
        if {[string range $signame 0 8] == "i_DMADATA"} {pin set x_DMADATA 0}
        if {[string range $signame 0 8] == "iINPUTBUS"} {pin set x_INPUTBUS 0}
        if {[string range $signame 0 3] == "iMEM"}      {pin set x_MEM 0}
        while {! [ctrlcflag 0]} {
            set on [expr {1 - $on}]
            puts "  $connpin = $on  ($signame)"
            pin set $signame $on
            after 1000
        }
        after 1000
        pin set x_DMAADDR 1 x_DMADATA 1 x_INPUTBUS 1 x_MEM 1 r_BAC 1 r_BMB 1 r_MA 1
        if {[ctrlcflag]} break
    }
}

# continuously display 'output-from-cpu' pin
# step through pins with control-C
#  conn = B34 B35 B36 C35 C36 D34 D35 D36
#  side = 1 or 2
proc loopoutest {conn side} {

    # turn all tri-states off
    #  bareit=1 : direct control of tri-state enables
    pin set bareit 1 x_DMAADDR 1 x_DMADATA 1 x_INPUTBUS 1 x_MEM 1 r_BAC 1 r_BMB 1 r_MA 1

    # select pins from csv file
    set csvfile [open "../zturn/signals.csv" "r"]
    set foundpins [list]
    while {! [ctrlcflag]} {
        set csvline [gets $csvfile]
        if {$csvline == ""} break
        set columns [splitcsvline $csvline]
        set signame [lindex $columns 0]
        set connpin [lindex $columns 1]
        set ztrnpin [lindex $columns 4]
        if {($ztrnpin != "") && ([string index $signame 0] == "o") && ([string length $connpin] == 6) && ([string range $connpin 0 2] == $conn) && ([string index $connpin 5] == $side)} {
            lappend foundpins "$connpin $signame"
        }
    }
    if {[llength $foundpins] == 0} {
        puts "no pins found"
        return
    }

    # loop through pins in sorted order
    set foundpins [lsort $foundpins]
    while {! [ctrlcflag]} {
        puts ""
        foreach cs $foundpins {
            set connpin [string range $cs 0 5]
            set signame [string range $cs 7 end]
            if {[string range $signame 0 3] == "oBAC"} {pin set r_BAC 0}
            if {[string range $signame 0 3] == "oBMB"} {pin set r_BMB 0}
            if {[string range $signame 0 2] == "oMA"}  {pin set r_MA  0}
            set on [pin get $signame]
            puts "  bareit=[pin get bareit] r_BAC=[pin get r_BAC] r_BMB=[pin get r_BMB] x_INPUTBUS=[pin get x_INPUTBUS]"
            puts "  $connpin = $on  ($signame)"
            pin set x_DMAADDR 1 x_DMADATA 1 x_INPUTBUS 1 x_MEM 1 r_BAC 1 r_BMB 1 r_MA 1
        }
        after 1000
    }
}

# message displayed as part of help command
return "also, 'helpini' will print help for z8lpinini.tcl commands"
