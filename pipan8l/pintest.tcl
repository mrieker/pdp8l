
# test pins coming out of zynq/zturn board
# toggles each on/off/on/off in sequence by J pin number

#  ./z8lpanel
#  z8lpanel> source pintest.tcl
#  z8lpanel> intest
#  z8lpanel> outest

proc splitcsvline {csvline} {
    set len [string length $csvline]
    set csvlist {}
    set quoted 0
    set strsofar ""
    for {set i 0} {$i < $len} {incr i} {
        set ch [string index $csvline $i]
        if {! $quoted && ($ch == ",")} {
            lappend csvlist $strsofar
            set strsofar ""
            continue
        }
        if {$ch == "\""} {
            set quoted [expr {! $quoted}]
            continue
        }
        if {$ch == "\\"} {
            incr i
            set ch [string index $csvline $i]
        }
        set strsofar "$strsofar$ch"
    }
    lappend csvlist $strsofar
    return $csvlist
}

# test 'input-to-processor' pins
proc intest {} {

    # read signal names for each ZTURN connector pin from .csv file
    for {set j 11} {$j <= 12} {incr j} {
        for {set p 1} {$p <= 80} {incr p} {
            set jp [expr {$j * 100 + $p}]
            set js($jp) "-"
        }
    }
    set csvfile [open "../zturn/signals.csv" "r"]
    while true {
        set csvline [gets $csvfile]
        if {$csvline == ""} break
        set columns [splitcsvline $csvline]
        set signame [lindex $columns 0]
        set jpiname [lindex $columns 4]
        if {[string index $jpiname 0] == "J"} {
            if {([string length $jpiname] != 6) || ([string index $jpiname 3] != "-")} {
                puts "intest: bad csv jpiname $jpiname in $csvline"
                exit 1
            }
            set j [string range $jpiname 1 2]
            set p [string range $jpiname 4 5]
            set js($j$p) $signame
        }
    }
    close $csvfile

    # step through each pin sequentially
    puts "  starting, control-C to go on to next pin, double-control-C to abort"
    for {set j 11} {$j <= 12} {incr j} {
        for {set p 1} {$p <= 80} {incr p} {
            set jp [expr {$j * 100 + $p}]
            set sig $js($jp)
            if {[pin test $sig] > 0} {
                # alternate pin on/off/on/off... until control-C
                set on 1
                while {! [ctrlcflag 0]} {
                    puts "    J$j P$p = $on  $sig"
                    pin set bareit 1 $sig $on
                    after 1000
                    set on [expr {1 - $on}]
                }
                pin set $sig 0
            }
        }
    }
    puts "  finished"
}

# test 'output-from-processor' pins
proc outest {} {

    # read signal names for each ZTURN connector pin from .csv file
    for {set j 11} {$j <= 12} {incr j} {
        for {set p 1} {$p <= 80} {incr p} {
            set jp [expr {$j * 100 + $p}]
            set js($jp) "-"
        }
    }
    set csvfile [open "../zturn/signals.csv" "r"]
    while true {
        set csvline [gets $csvfile]
        if {$csvline == ""} break
        set columns [splitcsvline $csvline]
        set signame [lindex $columns 0]
        set jpiname [lindex $columns 4]
        if {[string index $jpiname 0] == "J"} {
            if {([string length $jpiname] != 6) || ([string index $jpiname 3] != "-")} {
                puts "intest: bad csv jpiname $jpiname in $csvline"
                exit 1
            }
            set j [string range $jpiname 1 2]
            set p [string range $jpiname 4 5]
            set js($j$p) $signame
        }
    }
    close $csvfile

    # step through each pin sequentially
    puts "  starting, control-C to go on to next pin, double-control-C to abort"
    for {set j 11} {$j <= 12} {incr j} {
        for {set p 1} {$p <= 80} {incr p} {
            set jp [expr {$j * 100 + $p}]
            set sig $js($jp)
            if {[pin test $sig] == 0} {
                # read pin until control-C
                while {! [ctrlcflag 0]} {
                    set on [pin set bareit 1 get $sig]
                    puts "    J$j P$p = $on  $sig"
                    after 1000
                }
            }
        }
    }
    puts "  finished"
}

puts ""
puts "  intest - test input-to-processor pins"
puts "  outest - test output-from-processor pins"

