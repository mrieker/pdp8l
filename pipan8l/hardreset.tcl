#
# if PDP gets jammed:
#  1) pipan8l> setsw step 1
#  2) ./z8lpin hardreset.tcl
#  3) ./z8lreal [-enlo4k]
#  4) pipan8l> stopandreset
#
pin set bareit 1 x_MEM 0 i_MEMDONE 0
pin set bareit 1 x_MEM 0 i_MEMDONE 1
pin set bareit 1 x_MEM 0 i_STROBE 0
pin set bareit 1 x_MEM 0 i_STROBE 1
pin set bareit 0 x_MEM 1 i_STROBE 1
