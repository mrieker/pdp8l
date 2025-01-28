;# get regular pipan8l init file
set oldhelp [source "[file dirname [info script]]/pipan8lini.tcl"]

rename helpini old_helpini

# if the PDP gets jammed, try this to unstick it
proc hardreset {} {
    set enlo4k [pin get xm_enlo4k]
    set simit [pin get simit]
    setsw step 1
    pin set fpgareset 1 fpgareset 0
    pin set simit $simit xm_enlo4k $enlo4k
    if {! $simit} {
        pin set bareit 1 r_MA 1 x_MEM 0 i_MEMDONE 0 i_MEMDONE 1 x_MEM 1
        pin set bareit 1 r_MA 1 x_MEM 0 i_STROBE 0  i_STROBE 1  x_MEM 1
        pin set bareit 0
    }
    stopandreset
}

proc helpini {} {
    old_helpini
    puts "  hardreset  - unjam a jammed PDP"
    puts ""
}

return $oldhelp
