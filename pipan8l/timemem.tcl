#
# Time memory cycles using pdp8lxmem.v memcycctr
# Prints cycle length: count
#  length is in 10nS units
#
proc timemem {} {
    set counts [dict create]
    for {set i 0} {($i < 10000) & ! [ctrlcflag]} {incr i} {
        pin set xm_memcycctrhi 0
        while true {
            set hi [pin get xm_memcycctrhi]
            if {$hi >= 0x60000000} break
        }
        set lo [pin get xm_memcycctrlo]
        for {set j 0} {$j < 5} {incr j} {
            set ctr [expr {$lo & 0x3FF}]
            dict incr counts $ctr
            set lo [expr {(($lo >> 12) & 0xFFFFF) | (($hi & 0xFFF) << 20)}]
            set hi [expr {$hi >> 12}]
        }
    }
    foreach key [dict keys $counts] {
        puts "$key: [dict get $counts $key]"
    }
}
