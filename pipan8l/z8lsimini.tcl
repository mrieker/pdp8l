;# get regular pipan8l init file
set ret [source "[file dirname [info script]]/pipan8lini.tcl"]

;# wait 20mS for flick switches
;# - 20mS in case z8lmctrace is running
;#   10mS misses some flicks
set bncyms 20

return $ret
