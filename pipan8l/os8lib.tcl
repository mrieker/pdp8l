proc os8dpackloadandboot {{sourcedisk ""}} {
    stopandreset

    if {$sourcedisk == ""} {
        set sourcedisk "os8-rkab0.rk05"
        if {! [file exists $sourcedisk]} {
            exec -ignorestderr wget -nv -O $sourcedisk.temp https://tangentsoft.com/pidp8i/uv/ock.rk05
            exec mv $sourcedisk.temp $sourcedisk
            exec chmod a-w $sourcedisk
        }
    }

    set home [getenv HOME /tmp]
    exec cp $sourcedisk $home/rkab0.rk05
    exec chmod u+w $home/rkab0.rk05
    set rk8pid [exec ./z8lrk8je -killit -loadrw 0 $home/rkab0.rk05 &]
    atexit "exec kill $rk8pid"
    after 1000

    startat [os8dpacktoggleinboot]
}

proc os8dpacktoggleinboot {} {
    wrmem 023 06002     ;#  iof
    wrmem 024 06744     ;#  dlca
    wrmem 025 01032     ;#  tad unit
    wrmem 026 06746     ;#  dldc
    wrmem 027 06743     ;#  dlag
    wrmem 030 01032     ;#  tad unit
    wrmem 031 05031     ;#  jmp .
    wrmem 032 00000     ;# unit: 0
    return 023
}

proc os8dtapeloadandboot {{sourcetape ""}} {
    stopandreset

    if {$sourcetape == ""} {
        set sourcetape "os8-dta0.tu56"
        if {! [file exists $sourcetape]} {
            exec -ignorestderr wget -nv -O $sourcetape.temp https://www.pdp8online.com/ftp/images/misc_dectapes/AL-4711C-BA.tu56
            exec mv $sourcetape.temp $sourcetape
            exec chmod a-w $sourcetape
        }
    }

    set home [getenv HOME /tmp]
    exec cp $sourcetape $home/dta0.tu56
    exec chmod u+w $home/dta0.tu56
    set tc08pid [exec ./z8ltc08 -killit -loadrw 0 $home/dta0.tu56 &]
    atexit "exec kill $tc08pid"
    after 1000

    startat [os8dtapetoggleinboot]
}

proc os8dtapetoggleinboot {} {

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

    return 07613
}
