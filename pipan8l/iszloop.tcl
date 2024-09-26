
assem 015 isz 3
assem 016 jmp 015
assem 017 iac
assem 020 jmp 015

setsw sr 015
flicksw ldad
while {! [ctrlcflag]} {
    stepit
}
