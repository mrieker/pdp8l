
# Various simple TTY test programs

#   ./pipan8l  (or z8lsim)
#   pipan8l> source testtty.tcl
#   pipan8l> <name-of-test> [<port-number>]
#     port number defaults to 3, other values typically 040, 042, 044, 046

;# continuously read char from keyboard into accumulator and echo, inc top bits
proc testecho {{port 03}} {
    flicksw stop
    set KSF [expr {06031 - 030 + 8 * $port}]
    set KRB [expr {06036 - 030 + 8 * $port}]
    set TSF [expr {06041 - 030 + 8 * $port}]
    set TLS [expr {06046 - 030 + 8 * $port}]
    assem 0200 iot $KRB     ;# KRB - read char clear flag
    assem 0201 tad 0222     ;# restore AC top bits
    assem 0202 iot $TLS     ;# start printing char, clear flag
    assem 0203 iot $TSF     ;# TSF - skip if printer ready
    assem 0204 jmp 0203
    assem 0205 iot $KSF     ;# KSF - skip if keyboard ready
    assem 0206 jmp 0205
    assem 0207 and 0220     ;# wipe old char from AC
    assem 0210 tad 0221     ;# increment top AC bits
    assem 0211 dca 0222     ;# save top bits
    assem 0212 jmp 0200
    wrmem 0220 07000
    wrmem 0221 01000
    wrmem 0222 00000
    disas 0200 0212
    startat 0200
}

;# continuously read char from keyboard into accumulator, inc top bits
proc testkb {{port 03}} {
    flicksw stop
    set KSF [expr {06031 - 030 + 8 * $port}]
    set KRB [expr {06036 - 030 + 8 * $port}]
    assem 0200 iot $KRB     ;# KRB - read char clear flag
    assem 0201 tad 0222     ;# restore AC top bits
    assem 0202 iot $KSF     ;# KSF - skip if keyboard ready
    assem 0203 jmp 0202
    assem 0204 and 0220     ;# wipe old char from AC
    assem 0205 tad 0221     ;# increment top AC bits
    assem 0206 dca 0222     ;# save top bits
    assem 0207 jmp 0200
    wrmem 0220 07000
    wrmem 0221 01000
    wrmem 0222 00000
    disas 0200 0207
    startat 0200
}

;# continuously print char in sr
proc testtt {{port 03}} {
    flicksw stop
    set TSF [expr {06041 - 030 + 8 * $port}]
    set TLS [expr {06046 - 030 + 8 * $port}]
    assem 0220 cla osr      ;# get char to print
    assem 0221 and 0243     ;# just 8 bits
    assem 0222 iot $TLS     ;# clear flag, start printing
    assem 0223 tad 0240     ;# get some top bits while waiting
    assem 0224 tad 0241     ;# increment before waiting
    assem 0225 iot $TSF     ;# TSF - skip if printer ready
    assem 0226 jmp 0225
    assem 0227 and 0242     ;# clear bottom bits before saving
    assem 0230 dca 0240     ;# save top bits before reading sr
    assem 0231 jmp 0220
    wrmem 0240 00000
    wrmem 0241 01000
    wrmem 0242 07000
    wrmem 0243 00377
    disas 0220 0231
    setsw sr 0015
    startat 0220
}

;# continuously print incrementing characters
proc testttinc {{port 03}} {
    flicksw stop
    set TSF [expr {06041 - 030 + 8 * $port}]
    set TLS [expr {06046 - 030 + 8 * $port}]
    assem 0260 iot $TLS     ;# clear flag, start printing
    assem 0261 iot $TSF     ;# TSF - skip if printer ready
    assem 0262 jmp 0261
    assem 0263 iac          ;# increment to next character
    assem 0264 jmp 0260
    startat 0260
}

puts ""
puts "  testecho  - continuously read from kb and echo to tt"
puts "              ...and increment top ac bits"
puts "  testkb    - continuously read char from kb into ac"
puts "              ...and increment top ac bits"
puts "  testtt    - continuously print char in sr"
puts "              ...and increment top ac bits"
puts "  testttinc - continuously print incrementing characters"
