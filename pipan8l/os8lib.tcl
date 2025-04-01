
# boot ~/os8-rkab0.rk05 disk
proc os8dpackloadandboot {} {
    global Z8LHOME

    hardreset

    set sourcedisk "os8-rkab0.rk05"
    if {! [file exists $Z8LHOME/$sourcedisk]} {
        exec -ignorestderr wget -nv -O $Z8LHOME/$sourcedisk.temp https://tangentsoft.com/pidp8i/uv/ock.rk05
        exec mv $Z8LHOME/$sourcedisk.temp $Z8LHOME/$sourcedisk
        exec chmod a-w $Z8LHOME/$sourcedisk
    }

    set home [getenv HOME /tmp]
    if {! [file exists $home/$sourcedisk]} {
        exec cp $Z8LHOME/$sourcedisk $home/$sourcedisk
        exec chmod u+w $home/$sourcedisk
    }

    set rk8pid [exec $Z8LHOME/z8lrk8je -killit -loadrw 0 $home/$sourcedisk &]
    atexit "exec kill $rk8pid"
    after 1000

    pin set xm_os8zap 1
    startat [os8dpack0toggleinboot]
}

# toggle in decpack long-form OS/8 boot
proc os8dpacktoggleinboot {{driveno 0}} {
    if {($driveno < 0) || ($driveno > 3)} {error "bad driveno $driveno"}
    wrmem 023 06002     ;#  iof
    wrmem 024 06744     ;#  dlca
    wrmem 025 01032     ;#  tad unit
    wrmem 026 06746     ;#  dldc
    wrmem 027 06743     ;#  dlag
    wrmem 030 01032     ;#  tad unit
    wrmem 031 05031     ;#  jmp .
    wrmem 032 [expr {$driveno * 2}] ;# unit<<1
    disas 023 031
    return 023
}

# toggle in decpack short-form OS/8 boot drive 0
proc os8dpack0toggleinboot {} {
    wrmem 030 06743     ;#  dlag
    wrmem 031 05031     ;#  jmp .
    disas 030 031
    return 030
}

# boot ~/os8-dta0.tu56 tape
proc os8dtapeloadandboot {} {
    global Z8LHOME

    hardreset

    set sourcetape "os8-dta0.tu56"
    if {! [file exists $Z8LHOME/$sourcetape]} {
        exec -ignorestderr wget -nv -O $Z8LHOME/$sourcetape.temp https://www.pdp8online.com/ftp/images/misc_dectapes/AL-4711C-BA.tu56
        exec mv $Z8LHOME/$sourcetape.temp $Z8LHOME/$sourcetape
        exec chmod a-w $Z8LHOME/$sourcetape
    }

    set home [getenv HOME /tmp]
    if {! [file exists $home/$sourcetape]} {
        exec cp $Z8LHOME/$sourcetape $home/$sourcetape
        exec chmod u+w $home/$sourcetape
    }

    set tc08pid [exec $Z8LHOME/z8ltc08 -killit -loadrw 0 $home/$sourcetape &]
    atexit "exec kill $tc08pid"
    after 1000

    pin set xm_os8zap 1
    startat [os8dtapetoggleinboot]
}

# toggle in dectape OS/8 boot
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
