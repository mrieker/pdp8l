
# run kaleidoscope on zynq with simulated PDP-8/L
#  ./z8lsim kaleid.tcl

if {! [file exists kaleid.bin] || ([file mtime kaleid.bin] < [file mtime kaleid.pal])} {
    exec pdp8v/asm/assemble -pal kaleid.pal kaleid.obj > kaleid.lis
    exec pdp8v/asm/link -o kaleid.oct kaleid.obj > kaleid.map
    exec pdp8v/asm/octtobin < kaleid.oct > kaleid.bin
}
set startaddr [loadbin kaleid.bin]
setsw sr 0
startat $startaddr
exec ./z8lvc8 e -pms 120 -size 512
exit
