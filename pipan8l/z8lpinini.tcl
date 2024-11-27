
# help for commands defined herein
proc helpini {} {
    puts ""
    puts "  getenv              - get environment variable"
    puts "  octal <val>         - convert value to 4-digit octal string"
    puts "  splitcsvline <line> - split line into list"
    puts ""
    puts "  loopintest <conn> <side> - test input-to-cpu pins"
    puts "                             eg, loopintest C36 1"
    puts ""
    puts "  loopoutest <conn> <side> - test output-from-cpu pins"
    puts "                             eg, loopoutest C36 1"
    puts ""
    puts "  loopdumpac          - continually dump AC as seen on PIOBUS"
    puts ""
    puts "  DMA bus"
    puts "    sendmadr          - send DMA ADDR over DMA bus"
    puts "    sendmdat          - send DMA DATA over DMA bus"
    puts ""
    puts "  MEM bus"
    puts "    readma            - read MA from MEM bus"
    puts "    sendmd            - send MD over MEM bus"
    puts ""
    puts "  PIO bus"
    puts "    readac            - read AC from PIO bus"
    puts "    readmb            - read MB from PIO bus"
    puts "    sendio            - send IO data over PIO bus"
    puts ""
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
