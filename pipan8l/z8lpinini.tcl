# read AC as seen on the PIO bus
# oBAC comes directly from the PIOBUS
# so turn on the r_BAC 74T245s and read it
proc readac {} {
    return [pin set r_BAC 1 r_BMB 1 x_INPUTBUS 1 bareit 1 r_BAC 0 get oBAC set r_BAC 1]
}

# read MB as seen on the PIO bus
# there is a flipflop register between PIOBUS and oBMB that clocks whenever not doing an io pulse
# so reset everything so flipflops clock then turn on the r_BMB 74T254s and read it
proc readmb {} {
    pin set softreset 1 nanocontin 1 simit 0 softreset 0
    pin set r_BAC 1 r_BMB 1 x_INPUTBUS 1 bareit 1 r_BMB 0
    return [pin get oBMB set r_BMB 1]
}

# read MA as seen on the MEM bus
# there is a flipflop register between MEMBUS and oMA that clocks whenever r_MA is low
# so reset everything and set r_MA=0 so flipflops clock then read it
proc readma {} {
    pin set softreset 1 nanocontin 1 simit 0 softreset 0
    pin set x_MEM 1 r_MA 0 bareit 1 r_BMB 0
    return [pin get oMA set r_MA 1]
}

# send memory read data out to iMEM via the MEM bus
# iMEM gets gated out to MEMBUS as long as we have x_MEM=0
proc sendmd {md} {
    pin set r_MA 1 x_MEM 0 bareit 1 iMEM $md
}

# send io reply data out to iINPUTBUS via the PIO bus
# iINPUTBUS gets gated out to PIOBUS as long as we have x_INPUTBUS=0
proc sendio {io} {
    pin set r_BAC 1 r_BMB 1 x_INPUTBUS 0 bareit 1 iINPUTBUS $io
}
