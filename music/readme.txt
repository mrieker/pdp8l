
setup os/8 disk:

    ./setup-os8.sh      just once or to start over

boot os/8 disk:

    ./manual.sh real    using real pdp
    ./manual.sh sim     using simulator

compile music.pa (just once after doing setup-os8.sh):

    on another screen do: ./ldptr.sh music.pal

    then back on os/8 screen, press enter at ^ prompt:

        .R PIP
        *MUSIC.PA<PTR:
        ^

    press enter and tape reads in, then press control-C at * prompt:

    then you get . prompt:

        .PAL MUSIC,RKB0:MUSIC<MUSIC  (creates listing file, takes about 90 sec)
          or
        .PAL MUSIC<MUSIC             (no listing file, takes about 30 sec)

        .LOAD MUSIC
        .SAVE SYS MUSIC=2401         (creates SYS:MUSIC.SV file)

list music files and play something:

    .DIR RKB0:*.MU

    .R MUSIC
    *RKB0:FIFTH1
    (takes 5..10 sec then starts)
    control-Q to quit whatever it is playing and get * prompt for another file to play

to convert a midi file (4 tracks max):

    ./readmidi < somefile.midi > somefile.mu.txt
    ./ldptr.sh somefile.mu.txt

    .R PIP
    *RKB0:SOMEFL.MU<PRT:
    ^ enter * control-C

to hear music:

    tune radio 600..700kHz for real pdp
        can also hook wire to D36-B2 that can go to amplified speakers
            open-collector so needs 3K-5K pullup to +5V such as on D36-A1

    for sim, connect wire from zturn J11-26 to amplified speakers
        3.3V totem pole, do not put a pullup

to see it counting through the music:

    ../pipan8l/z8ldump

    look for xbraddr (extended block ram address) register
    should be able to see it count up as music plays

