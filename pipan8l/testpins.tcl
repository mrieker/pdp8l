#
# test the raw MCP23017 pins
#

puts "\033\[H"   ;# home cursor
puts "\033\[J"   ;# erase to end-of-page

while {! [ctrlcflag 0]} {
    puts "\033\[H"   ;# home cursor
    puts ""
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
    puts ""
    puts "  * marks output pins"
    puts "  use pullup to inject a '1' on the input pins"
    puts "  - it will gradually discharge down"
    puts "  press ctrl-C to move on"
    puts ""
    after 333
}

for {set col 0} {$col < 10} {incr col} {
    set unum [expr {($col >> 1) + 1}]
    puts ""
    if {! ($col & 1)} {
        puts -nonewline "about to scan U$unum > "
        flush stdout
        gets stdin
    }
    if [ctrlcflag 0] break
    for {set row 0} {$row < 8} {incr row} {
        set pinofs [expr {($col & 1) ? 7 - $row : 8 + $row}]
        set pinnum [expr {($col >> 1) * 16 + $pinofs}]
        set pinval [getpin $pinnum]
        set pinwrt [setpin $pinnum $pinval]
        if {$pinwrt >= 0} {
            puts ""
            puts [format "testing %d: U%d output pin %d" $pinnum $unum [expr {($col & 1) ? 28 - $row : 1 + $row}]]
            puts "  ctrl-C to move on  "
            while {! [ctrlcflag 0]} {
                puts -nonewline "0"
                flush stdout
                setpin $pinnum 0
                after 2500
                if [ctrlcflag 0] break
                puts -nonewline "1"
                flush stdout
                setpin $pinnum 1
                after 2500
            }
        }
    }
}

