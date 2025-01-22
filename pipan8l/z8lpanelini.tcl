;# get regular pipan8l init file
set oldhelp [source "[file dirname [info script]]/pipan8lini.tcl"]

rename helpini old_helpini

# if the PDP gets jammed, try this to unstick it
proc hardreset {} {
    setsw step 1
    pin set softreset 1 softreset 0
    pin set xm_mrhold 0 xm_mwhold 0
    pin set bareit 1 x_MEM 0 i_MEMDONE 0
    pin set bareit 1 x_MEM 0 i_MEMDONE 1
    pin set bareit 1 x_MEM 0 i_STROBE 0
    pin set bareit 1 x_MEM 0 i_STROBE 1
    pin set bareit 0 x_MEM 1 i_STROBE 1
    stopandreset
}

proc helpini {} {
    old_helpini
    puts "  hardreset  - unjam a jammed PDP"
    puts ""
}

return $oldhelp
