#
# Time memory cycles using pdp8lxmem.v memcycctr
# Prints cycle length: count
#  length is in 10nS units
#
# $ ./z8lpanel
# z8lpanel> source timemem.tcl
# z8lpanel> timemem
#
proc timemem {{nloop 10000}} {
    set counts [dict create]
    for {set i 0} {($i < $nloop) & ! [ctrlcflag]} {incr i} {
        # collect 6 samples
        pin set xm_memcycctrhi 0
        while true {
            set hi [pin get xm_memcycctrhi]
            if {$hi >= 0x60000000} break
        }
        set lo [pin get xm_memcycctrlo]
        # add counts to dictionary
        # fpga discards oldest cuz it's probably a runt cuz probably
        #  cleared memcycctrhi somewhere in middle of cycle
        for {set j 0} {$j < 5} {incr j} {
            set ctr [expr {$lo & 0x3FF}]
            dict incr counts $ctr
            set lo [expr {(($lo >> 12) & 0xFFFFF) | (($hi & 0xFFF) << 20)}]
            set hi [expr {$hi >> 12}]
        }
    }
    # print dictionary
    foreach key [dict keys $counts] {
        puts "$key: [dict get $counts $key]"
    }
}
