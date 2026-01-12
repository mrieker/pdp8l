
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
    ^ enter
    * control-C

to hear music:

    real pdp only:
        tune radio 600..700kHz placed near backplane with cover off

    real pdp or sim (zturn plugged into pdp backplane with real pdp turned on):
        hook wire from D36-B2 backplane to amplified speakers
            open-collector so needs 3K-5K pullup to +5V such as on D36-A1

    real pdp or sim (zturn plugged into pdp backplane or not):
        run '../pipan8l/z8lpbit -server &' on a Linux PC or RasPI with headphones or speaker
            adjust volume with PC/RasPI sound control panel
        run '../pipan8l/z8lpbit -sound <ipaddressofpc> &' on the zturn

    sim only (zturn not plugged into pdp backplane):
        connect wire from zturn J11-26 to amplified speakers
        3.3V totem pole, do not use a pullup

to see it counting through the music:

    ../pipan8l/z8ldump

    look for xbraddr (extended block ram address) register
    should be able to see it count up as music plays

