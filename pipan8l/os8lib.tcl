proc os8dpackloadandboot {} {
    stopandreset

    set home [getenv HOME /tmp]
    exec cp rkab0.rk05 $home/rkab0.rk05
    exec chmod o+w $home/rkab0.rk05
    set rk8pid [exec ./z8lrk8je -killit -loadrw 0 $home/rkab0.rk05 &]
    atexit "exec kill $rk8pid"
    after 1000

    wrmem 023 06007     ;#  caf
    wrmem 024 06744     ;#  dlca
    wrmem 025 01032     ;#  tad unit
    wrmem 026 06746     ;#  dldc
    wrmem 027 06743     ;#  dlag
    wrmem 030 01032     ;#  tad unit
    wrmem 031 05031     ;#  jmp .
    wrmem 032 00000     ;# unit: 0

    startat 023
}

proc os8dtapeloadandboot {} {
    stopandreset

    set home [getenv HOME /tmp]
    exec cp dta0.tu56 $home/dta0.tu56
    exec chmod o+w $home/dta0.tu56
    set tc08pid [exec ./z8ltc08 -killit -loadrw 0 $home/dta0.tu56 &]
    atexit "exec kill $tc08pid"
    after 1000

    ;# from OS8 Handbook p41

    wrmem 07613 06774   ;# 7613 load status register B (with zeroes)
    wrmem 07614 01222   ;# 7614 TAD 7622 / 0600 (rewind tape)
    wrmem 07615 06766   ;# 7615 clear and load status register A
    wrmem 07616 06771   ;# 7616 skip on flag
    wrmem 07617 05216   ;# 7617 JMP .-1
    wrmem 07620 01223   ;# 7620 TAD 7623 / 0220 (read tape)
    wrmem 07621 05215   ;# 7621 JMP 7615
    wrmem 07622 00600   ;# 7622
    wrmem 07623 00220   ;# 7623
    wrmem 07754 07577   ;# 7754 read 129 words
    wrmem 07755 07577   ;# 7755 read into 7600..0000

    startat 07613
}