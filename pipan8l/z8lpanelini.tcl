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
    set nobrk [pin get cm_nobrk]                    ;# save nobrk
    set os8zap [pin get xm_os8zap]                  ;# save os8zap
    set simit [pin get simit]                       ;# save simit
    setsw step 1                                    ;# tell processor to stop asap
    pin set nanocontin 1 fpgareset 1 fpgareset 0    ;# turn on fpga clocking, pulse fpga reset
    pin set simit $simit                            ;# restore simit, nobrk, os8zap, enlo4k
    pin set cm_nobrk $nobrk
    pin set xm_os8zap $os8zap
    pin set xm_enlo4k $enlo4k
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

# override the tty access functions in pipan8lini.tcl
# these access the tty directly via the pdp8ltty.v registers
proc closettypipes {} { }

proc openttypipes {} { }

proc readttychartimed {msec} {
    for {set i 0} {$i < $msec} {incr i} {
        after 1
        set prfull [pin get pr_full]
        if {$prfull} {
            set prchar [pin get pr_char set pr_full 0 pr_flag 1]
            set prchar [expr {$prchar & 0177}]
            if {$prchar != 0} {
                return [inttochar $prchar]
            }
        }
    }
    return ""
}

proc sendchartottykb {ch} {
    pin set kb_char [chartoint $ch] kb_flag 1
}

return $oldhelp
