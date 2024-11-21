
# run via pipan8l on pipan8l raspi board:
#  ./pipan8l iszloop.tcl
#    startisz
# sim options (no PDP-8/L required):
#  add -sim option to ./pipan8l to run on a PC for testing pipan8l itself
#  use ./z8lsim instead of ./pipan8l to run on zynq card for testing fpga

# on another screen (either on pipan8l/zynq board or some other computer):
#  ./pipan8l -status [<ip-addr-of-pipan8l>]
# and see AC counting

# (optional) on third screen on zynq board:
#  ./z8lreal -enlo4k    # do this before doing the './pipan8l or ./z8lsim iszloop.tcl' command
#  ./z8lpin             # do this anytime
#    set XM_OS8ZAP 0    # makes AC count slowly (default)
#    set XM_OS8ZAP 1    # makes AC count quickly

assem 0615 isz 0603
assem 0616 jmp 0615
assem 0617 iac
assem 0620 jmp 0615

proc startisz {} {
    setsw step 0
    startat 0615
}

proc stepisz {} {
    setsw step 1
    startat 0615
    dumpit
    while {! [ctrlcflag]} {
        stepit
    }
}

puts ""
puts "  startisz - start test and let it run"
puts "  stepisz  - start test and print each step"
