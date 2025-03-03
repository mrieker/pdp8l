
# run doubleroll-8l on zynq with real or simulated PDP-8/L
#  ./z8lpanel doubleroll-8l.tcl     real PDP-8/L
#  ./z8lsim doubleroll-8l.tcl       simulator

exec -ignorestderr make doubleroll-8l.bin
hardreset
set startaddr [loadbin doubleroll-8l.bin]
setsw sr 0140
startat $startaddr
relsw all
exec -ignorestderr ./z8lpanel -status < /dev/tty > /dev/tty
exit
