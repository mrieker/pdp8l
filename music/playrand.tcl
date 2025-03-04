#
#  boot os/8 disk loaded in rk05 drive 0
#  then tell it to play random songs
#
#  runs on z8lpanel or z8lsim
#
puts "playrand: resetting processor"
hardreset
puts "playrand: toggling in bootloader"
source ../pipan8l/os8lib.tcl
set sa [os8dpack0toggleinboot]
puts "playrand: booting os/8"
setsw sr $sa    ;# set start address in switch register
flicksw ldad    ;# load address into program counter
openttypipes    ;# access tty via pipes
flicksw start   ;# start os/8
relsw all       ;# release all switches

exec ../pipan8l/z8lpbit     ;# enable ISZAC opcode for our patched music.pal program

proc readttyline {} {
    set line ""
    while true {
        set ch [readttychartimed 5000]
        if {$ch == ""} {
            error "timed out reading tty line"
        }
        puts -nonewline $ch
        flush stdout
        if {$ch == "\n"} {return $line}
        set line "$line$ch"
        if {$line == "."} {return $line}
    }
}

# wait for 'FIELD SER' message then send control-C
checkttymatch 0 "FIELD SER"
sendchartottykb "\003"

# read a few more lines until we see '.' prompt
while true {
    set line [readttyline]
    if {$line == "."} break
}

# get listing of all music files on RKB0
checkttyprompt "" "DIR RKB0:*.MU"

# get their names into songs list
set songs [list]
while true {
    # read line then stop if '.' prompt
    set line [readttyline]
    if {$line == "."} break
    # search line for '.MU' files and save corresponding names
    set len [string length $line]
    set i 0
    while {$i < $len} {
        set j [string first ".MU" $line $i]
        if {$j < 0} break
        set title [string range $line [expr {$j - 6}] [expr {$j - 1}]]
        lappend songs [string trim $title]
        set i [expr {$j + 1}]
    }
}

# make sure we found something
set nsongs [llength $songs]
if {$nsongs == 0} {
    error "no .MU files found on RKB0:"
}

# tell os/8 to start the music program
checkttyprompt "" "R MUSIC"

# play forever
set first true
while true {

    # randomly shuffle song list
    puts ""
    if {$first} {
        puts "playrand: control-C once to skip to next song"
        puts "playrand: control-C twice to terminate player"
        set first false
    }
    puts "playrand: shuffling playlist"
    for {set i 0} {$i < $nsongs} {incr i} {
        set j [expr {int (rand () * $nsongs)}]
        if {$j != $i} {
            set temp  [lindex $songs $j]
            set songs [lreplace $songs $j $j [lindex $songs $i]]
            set songs [lreplace $songs $i $i $temp]
        }
    }

    # loop through song list
    for {set i 0} {$i < $nsongs} {incr i} {
        set song [lindex $songs $i]

        # wait for '*' prompt
        # could be waiting for music program to finish starting up
        # or could be waiting for it to finish compiling/playing last song
        set atbol true
        while true {

            # if ctrl-C, tell music program to stop playing song by sending ctrl-O then ctrl-Q
            if {[ctrlcflag]} {
                puts "playrand: cancelling song"
                sendchartottykb "\017"
                after 1000
                sendchartottykb "\021"
                after 1000
                ctrlcflag 0
                continue
            }

            # check for char printed by PDP
            # if '*' at beginning of line, it is ready for new song file
            set ch [readttychartimed 1000]
            if {$ch == ""} continue
            puts -nonewline $ch
            flush stdout
            if {$atbol && ($ch == "*")} break
            set atbol [expr {$ch == "\n"}]
        }

        # got '*' prompt, send song filename
        checkttyprompt "" "RKB0:$song" 1000
    }
}
