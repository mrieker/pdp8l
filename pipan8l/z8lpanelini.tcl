#
# initialization file for z8lpanel
#

# get regular pipan8l init file
set oldhelp [source "[file dirname [info script]]/pipan8lini.tcl"]
rename helpini old_helpini

# make sure fpga clocking is turned on
pin set nanocontin 1

# if the PDP gets jammed, try this to unstick it
proc hardreset {} {
    set enlo4k [pin get xm_enlo4k]                  ;# save enlo4k
    set simit [pin get simit]                       ;# save simit
    setsw step 1                                    ;# tell processor to stop asap
    pin set nanocontin 1 fpgareset 1 fpgareset 0    ;# turn on fpga clocking, pulse fpga reset
    pin set simit $simit xm_enlo4k $enlo4k          ;# restore simit, enlo4k
    if {! $simit} {
        ;# real PDP, pulse i_STROBE and i_MEMDONE lines
        pin set bareit 1 r_MA 1 x_MEM 0 i_MEMDONE 0 i_MEMDONE 1 x_MEM 1
        pin set bareit 1 r_MA 1 x_MEM 0 i_STROBE 0  i_STROBE 1  x_MEM 1
        pin set bareit 0
    }
    stopandreset                                    ;# make sure stopped and cleared
}

proc helpini {} {
    old_helpini
    puts "  hardreset  - unjam a jammed PDP"
    puts ""
}

return $oldhelp
