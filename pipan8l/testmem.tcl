# ./pipan8l testmem.tcl

# wrap one of the test procs below to loop continuously
# eg, looptest testrands
proc looptest {testfunc {step 040}} {
    set errors 0
    for {set pass 1} true {incr pass} {
        puts ""
        puts "PASS $pass"
        puts ""
        for {set z 0} {$z < 07777} {incr z $step} {
            set err [$testfunc $z [expr {$z + $step - 1}]]
            if {$err < 0} return
            incr errors $err
        }
        puts ""
        puts "ERRORS: $errors"
        puts ""
    }
}

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
    set errors 0

    puts "write zeroes [octal $beg]..[octal $end]..."
    setsw sr $beg
    flicksw ldad
    setsw sr 0
    for {set addr $beg} {$addr <= $end} {incr addr} {
        if [ctrlcflag] {return -1}
        if {($addr & 077) == 0} {puts -nonewline " [octal $addr]" ; flush stdout}
        flicksw dep
        set ma [getreg ma]
        set mb [getreg mb]
        if {$ma != $addr} {
            puts "expected ma [octal $addr] but got [octal $ma]"
            return -1
        }
        if {$mb != 0} {
            puts "[octal $addr]: expected mb 0000 but got [octal $mb]"
            return -1
        }
    }
    puts ""
    puts "verify zeroes..."
    setsw sr $beg
    flicksw ldad
    for {set addr $beg} {$addr <= $end} {incr addr} {
        if [ctrlcflag] {return -1}
        flicksw exam
        set ma [getreg ma]
        set mb [getreg mb]
        if {$ma != $addr} {
            puts "expected ma [octal $addr] but got [octal $ma]"
            return -1
        }
        if {($addr & 017) == 000} {puts -nonewline " [octal $addr]:"}
        puts -nonewline " [octal $mb]"
        if {$mb != 0} {incr errors}
        if {($addr & 017) == 017} {puts ""}
    }
    return $errors
}

proc testones {{beg 0} {end 07777}} {
    flicksw stop
    setsw dfld 0
    setsw ifld 0
    set errors 0

    puts "write ones [octal $beg]..[octal $end]..."
    setsw sr $beg
    flicksw ldad
    setsw sr 07777
    for {set addr $beg} {$addr <= $end} {incr addr} {
        if [ctrlcflag] {return -1}
        if {($addr & 077) == 0} {puts -nonewline " [octal $addr]" ; flush stdout}
        flicksw dep
        set ma [getreg ma]
        set mb [getreg mb]
        if {$ma != $addr} {
            puts "expected ma [octal $addr] but got [octal $ma]"
            return -1
        }
        if {$mb != 07777} {
            puts "[octal $addr]: expected mb 7777 but got [octal $mb]"
            return -1
        }
    }
    puts ""
    puts "verify ones..."
    setsw sr $beg
    flicksw ldad
    setsw sr 07777
    for {set addr $beg} {$addr <= $end} {incr addr} {
        if [ctrlcflag] {return -1}
        flicksw exam
        set ma [getreg ma]
        set mb [getreg mb]
        if {$ma != $addr} {
            puts "expected ma [octal $addr] but got [octal $ma]"
            return -1
        }
        if {($addr & 017) == 000} {puts -nonewline " [octal $addr]:"}
        puts -nonewline " [octal $mb]"
        if {$mb != 07777} {incr errors}
        if {($addr & 017) == 017} {puts ""}
    }
    return $errors
}

proc testrands {{beg 0} {end 07777}} {
    flicksw stop
    setsw dfld 0
    setsw ifld 0
    set errors 0

    puts "write randoms [octal $beg]..[octal $end]..."
    setsw sr $beg
    flicksw ldad
    for {set addr $beg} {$addr <= $end} {incr addr} {
        if [ctrlcflag] {return -1}
        if {($addr & 077) == 0} {puts -nonewline " [octal $addr]" ; flush stdout}
        set r [expr {int (rand () * 010000)}]
        set rands($addr) $r
        setsw sr $r
        flicksw dep
        set ma [getreg ma]
        set mb [getreg mb]
        if {$ma != $addr} {
            puts "expected ma [octal $addr] but got [octal $ma]"
            return -1
        }
        if {$mb != $r} {
            puts "[octal $addr]: expected mb [octal $r] but got [octal $mb]"
            return -1
        }
    }
    puts ""
    puts "verify randoms..."
    setsw sr $beg
    flicksw ldad
    for {set addr $beg} {$addr <= $end} {incr addr} {
        if [ctrlcflag] {return -1}
        flicksw exam
        set ma [getreg ma]
        set mb [getreg mb]
        if {$ma != $addr} {
            puts "expected ma [octal $addr] but got [octal $ma]"
            return -1
        }
        if {($addr & 007) == 000} {puts -nonewline " [octal $addr]:"}
        puts -nonewline " [octal $mb]"
        set r $rands($addr)
        if {$mb != $r} {
            puts -nonewline "!=[octal $r]"
            incr errors
        } else {
            puts -nonewline "      "
        }
        if {($addr & 007) == 007} {puts ""}
    }
    return $errors
}

puts ""
puts "  looptest - wrap one of the below test to run continuously"
puts "  testzeroes \[beg \[end\]\] - writes zeroes to all memory then verifies"
puts "  testones \[beg \[end\]\] - writes ones to all memory then verifies"
puts "  testrands \[beg \[end\]\] - writes randoms to all memory then verifies"

