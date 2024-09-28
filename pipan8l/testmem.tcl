# ./pipanel testmem.tcl

# continuously read the given list of memory addresses
proc readloop {args} {
    while {! [ctrlcflag]} {
        foreach addr $args {
            if [ctrlcflag] break
            puts -nonewline [format " %04o" $addr]
            flush stdout
            setsw sr $addr
            flicksw ldad
            puts -nonewline [format "=%04o" [getreg mb]]
        }
        puts ""
    }
}

proc testzeroes {{beg 0} {end 07777}} {
    flicksw stop
    setsw dfld 0
    setsw ifld 0

    puts "write zeroes [octal $beg]..[octal $end]..."
    setsw sr $beg
    flicksw ldad
    setsw sr 0
    for {set addr $beg} {$addr <= $end} {incr addr} {
        if [ctrlcflag] return
        if {($addr & 077) == 0} {puts -nonewline " [octal $addr]" ; flush stdout}
        flicksw dep
        set ma [getreg ma]
        set mb [getreg mb]
        if {$ma != $addr} {
            puts "expected ma [octal $addr] but got [octal $ma]"
            return
        }
        if {$mb != 0} {
            puts "[octal $addr]: expected mb 0000 but got [octal $mb]"
            return
        }
    }
    puts ""
    puts "verify zeroes..."
    setsw sr $beg
    flicksw ldad
    for {set addr $beg} {$addr <= $end} {incr addr} {
        if [ctrlcflag] return
        flicksw exam
        set ma [getreg ma]
        set mb [getreg mb]
        if {$ma != $addr} {
            puts "expected ma [octal $addr] but got [octal $ma]"
            return
        }
        if {($addr & 017) == 000} {puts -nonewline " [octal $addr]:"}
        puts -nonewline " [octal $mb]"
        if {($addr & 017) == 017} {puts ""}
    }
}

proc testones {{beg 0} {end 07777}} {
    flicksw stop
    setsw dfld 0
    setsw ifld 0

    puts "write ones [octal $beg]..[octal $end]..."
    setsw sr $beg
    flicksw ldad
    setsw sr 07777
    for {set addr $beg} {$addr <= $end} {incr addr} {
        if [ctrlcflag] return
        if {($addr & 077) == 0} {puts -nonewline " [octal $addr]" ; flush stdout}
        flicksw dep
        set ma [getreg ma]
        set mb [getreg mb]
        if {$ma != $addr} {
            puts "expected ma [octal $addr] but got [octal $ma]"
            return
        }
        if {$mb != 07777} {
            puts "[octal $addr]: expected mb 7777 but got [octal $mb]"
            return
        }
    }
    puts ""
    puts "verify ones..."
    setsw sr $beg
    flicksw ldad
    setsw sr 07777
    for {set addr $beg} {$addr <= $end} {incr addr} {
        if [ctrlcflag] return
        flicksw exam
        set ma [getreg ma]
        set mb [getreg mb]
        if {$ma != $addr} {
            puts "expected ma [octal $addr] but got [octal $ma]"
            return
        }
        if {($addr & 017) == 000} {puts -nonewline " [octal $addr]:"}
        puts -nonewline " [octal $mb]"
        if {($addr & 017) == 017} {puts ""}
    }
}

proc testrands {{beg 0} {end 07777}} {
    flicksw stop
    setsw dfld 0
    setsw ifld 0

    puts "write randoms [octal $beg]..[octal $end]..."
    setsw sr $beg
    flicksw ldad
    for {set addr $beg} {$addr <= $end} {incr addr} {
        if [ctrlcflag] return
        if {($addr & 077) == 0} {puts -nonewline " [octal $addr]" ; flush stdout}
        set r [expr {int (rand () * 010000)}]
        set rands($addr) $r
        setsw sr $r
        flicksw dep
        set ma [getreg ma]
        set mb [getreg mb]
        if {$ma != $addr} {
            puts "expected ma [octal $addr] but got [octal $ma]"
            return
        }
        if {$mb != $r} {
            puts "[octal $addr]: expected mb [octal $r] but got [octal $mb]"
            return
        }
    }
    puts ""
    puts "verify randoms..."
    setsw sr $beg
    flicksw ldad
    for {set addr $beg} {$addr <= $end} {incr addr} {
        if [ctrlcflag] return
        flicksw exam
        set ma [getreg ma]
        set mb [getreg mb]
        if {$ma != $addr} {
            puts "expected ma [octal $addr] but got [octal $ma]"
            return
        }
        if {($addr & 007) == 000} {puts -nonewline " [octal $addr]:"}
        puts -nonewline " [octal $mb]"
        set r $rands($addr)
        if {$mb != $r} {
            puts -nonewline "!=[octal $r]"
        } else {
            puts -nonewline "      "
        }
        if {($addr & 007) == 007} {puts ""}
    }
}

puts ""
puts "  testzeroes \[beg \[end\]\] - writes zeroes to all memory then verifies"
puts "  testones \[beg \[end\]\] - writes ones to all memory then verifies"
puts "  testrands \[beg \[end\]\] - writes randoms to all memory then verifies"

