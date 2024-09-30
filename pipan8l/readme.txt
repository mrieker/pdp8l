
RasPI-Zero-W uses /dev/i2c-1 to access the I2C bus going to the PCB.
Any RasPI that runs recent RasPI OS and has a 40-pin GPIO connector
should work, but I use a RasPI Zero because it is small, allowing me
to flip the processor over without any issue.

Here is a pic of everything running:

    https://www.outerworldapps.com/pdp8l-with-pipan8l.jpg

The PIPAN8L board is in the lower right corner plugged into the 8/L downside-up.
The PC screen on the left shows the pipan8l program on the lower terminal window,
with the ttpan8l program running on the upper terminal window.

- - - - - - - - - - - - -

The PCB plugs into slot 1 of the PDP-8/L in place of the console.

    RasPI-Zero-W ./pipan8l  ==>  PCB MCP23017s  ==>  PDP-8/L slot 1  ==>  rest of PDP-8/L

The RasPI can be powered from its own USB cable or from the PDP's 5V supply.
To power it from the PDP, install the jumper on the right end of the PCB.
I use the USB power so I can turn the PDP off and the RasPI will not need rebooting.

- - - - - - - - - - - - -

./pipan8l runs the test program.  (You can type ./pip then press tab).
It reads commands from the prompt and can run scripts written in TCL.
The commands echo the switches and lights:

    setsw <switchname> <value>      - sets switch to a value
    getsw <switchname>              - gets value of a switch
    getreg <registername>           - gets value of a register (light bulbs)

And there is a command used to flush series of setsw commands:

    flushit                         - writes out any pending switch changes to MCP23017s
                                    - sets up to read MCP23017s when needed

There are a couple utility commands:

    ctrlcflag                       - sense if control-C has been pressed, optionallly clearing it
                                    - used for test scripts
                                    - otherwise, two control-Cs will abort out of pipan8l

    assemop <mnemonic> <operand>    - assemble the given assembly language instruction
    disasop <opcode>                - disassemble the given opcode to assemly language

And the pipan8lini.tcl script adds some higher-level commands:

    assem <address> <mnemonic> <operand>    - assemble the instruction and deposit in memory
    disas <start> <stop>                    - disassemble the instructions in given memory range
    dumpit                                  - print out all registers and switches
    flicksw <switchname>                    - flick the given switch on then off
    loadbin <filename>                      - load the given bin file into memory
    octal <value>                           - print the given value as 4-digit octal number
    rdmem <address>                         - read memory location
    startat <address>                       - does load address followed by start
    stepit                                  - do a single step then dump switches & lights
    wrmem <address> <value>                 - write the given value to memory

Note:
    The command processor is a TCL interpreter, so all numbers default to decimal.
    Prefix numbers with a 0 if you want to input an octal number.
    Use the octal command to convert number to octal for printing.

So when it's all running, you can do commands like:

    ./pipan8l               ;# start it running
    assem 0200 isz 030      ;# input a simple test program
    assem 0201 jmp 0200
    assem 0202 iac
    assem 0203 jmp 0200
    disas 0200 0203         ;# print it out to verify it
    startat 0200            ;# start it running
    flicksw stop            ;# stop it wherever it is
    setsw step 1            ;# turn on the step mode switch
    stepit                  ;# single cycle and print lights & switches
    stepit
    stepit                  ;# ... 3 times
    setsw step 0            ;# turn single cycle off
    flicksw cont            ;# continue
    dumpit                  ;# see it running
    dumpit
    flicksw stop            ;# stop processor so we can access memory
    octal [rdmem 030]       ;# read the counter and print in octal
    startat 0200            ;# restart program

Logged into RasPI:

$ sudo apt install tcl-dev
$ make
    ...
$ ./pipan8l

          ST=F  AC=0.7777 MA=0.7240 MB=7777 IR=7  OPR  SR=0000 DFLD IFLD MPRT STEP

TCL scripting, do 'help' for pipan8l-specific commands
  do 'exit' to exit pipan8l
pipan8l> assem 0200 isz 030
pipan8l> assem 0201 jmp 0200
pipan8l> assem 0202 iac
pipan8l> assem 0203 jmp 0200
pipan8l> disas 0200 0203
0200  2030  ISZ   0030
0201  5200  JMP   0200
0202  7001  IAC
0203  5200  JMP   0200
pipan8l> startat 0200
pipan8l> flicksw stop
pipan8l> setsw step 1
pipan8l> stepit
          ST=E  AC=0.0000 MA=0.0030 MB=0000 IR=2  ISZ  SR=0203 DFLD IFLD MPRT STEP
pipan8l> stepit
          ST=F  AC=0.0000 MA=0.5027 MB=5747 IR=2  ISZ  SR=0203 DFLD IFLD MPRT STEP
pipan8l> stepit
          ST=F  AC=0.0000 MA=0.0200 MB=2577 IR=5  JMP  SR=0203 DFLD IFLD MPRT STEP
pipan8l> setsw step 0
pipan8l> flicksw cont
pipan8l> dumpit
  RUN     ST=F  AC=0.0003 MA=0.4230 MB=5755 IR=3  DCA  SR=0203 DFLD IFLD MPRT
pipan8l> dumpit
  RUN     ST=E  AC=0.0005 MA=0.0100 MB=4177 IR=2  ISZ  SR=0203 DFLD IFLD MPRT
pipan8l> dumpit
  RUN     ST=F  AC=0.0006 MA=0.0233 MB=0560 IR=3  DCA  SR=0203 DFLD IFLD MPRT
pipan8l> flicksw stop
pipan8l> octal [rdmem 030]
4521
pipan8l> startat 0200
pipan8l>

- - - - - - - - - - - - -

It is somewhat tedious to keep entering dumpit commands to see what the panel is doing.
So there is a program called ttpan8l that will continuously dump the panel contents in
similar format to the actual PDP-8/L panel:


  PDP-8/L       -   - - -   - * -   - * *   - * -  0.0232  MA   IR [ - * *  DCA ]           1804 fps

                    * - -   * - *   * - *   - * -    4552  MB   ST [ f E d wc ca b ]

                -   - - -   - * *   - * -   - * -  0.0322  AC   ion per prt RUN

  MPRT  DFLD IFLD   - - -   - * -   - - -   - * *    0203  SR   ldad  start cont stop step exam  dep


Capital letters indicate the light is ON.
Small letters incdicate it is off.
Also, the ON lights are highlighted in green.

Leave pipan8l running in one terminal window of the RasPI.

Then, you can either run ttpan8l from another terminal window of the RasPI:

    ./ttpan8l

Or you can run it from a terminal window of a PC:

    ./ttpan8l <ip-address-of-raspi>

It does UDP to the pipan8l program to read the panel state as fast as it can.

- - - - - - - - - - - - -

There is a simple memory test script, testmem.tcl.
To use it:

    ./pipan8l testmem.tcl

    testzeroes 0 077            ;# test writing zeroes to memory 0000..0077
                                ;# writes then verifies

    testrands 0100 0177         ;# writes random numbers to 0100..0177 then verifies

    looptest testrands          ;# writes random numbers to 0000..0037 then verifies
                                ;#  then does 0040..0077 and verifies
                                ;#  then does 0100..0137 and verifies
                                ;#      ...
                                ;#  all the way through to 7740..7777
                                ;#  then repeats forever

Note:
    These tests run by simulating deposit and examine console operations so do not require that
    the processor be able to run a program.  It is just like someone manually doing load address,
    deposit and examines, flicking switches and reading lights, but it's all automated.

    The PDP-8/L circuitry limits the rate of panel operations (load address, examine, deposit, etc)
    to about 10 per second (for switch debouncing).  So these tests run S-L-O-W-L-Y.  But it beats
    doing it by hand!


$ ./pipan8l testmem.tcl

          ST=E  AC=0.1314 MA=0.0030 MB=0000 IR=2  ISZ  SR=0030 DFLD IFLD MPRT

  looptest - wrap one of the below test to run continuously
  testzeroes [beg [end]] - writes zeroes to all memory then verifies
  testones [beg [end]] - writes ones to all memory then verifies
  testrands [beg [end]] - writes randoms to all memory then verifies

TCL scripting, do 'help' for pipan8l-specific commands
  do 'exit' to exit pipan8l
pipan8l> looptest testrands

PASS 1

write randoms 0000..0037...
 0000
verify randoms...
 0000: 5476       0453       6021       3630       3042       7403       7603       7006      
 0010: 6343       2240       0554       5052       6070       4357       0443       1714      
 0020: 4525       2447       5673       5475       5153       7647       4334       4777      
 0030: 3427       5527       0065       0560       4074       4412       6431       2176      
write randoms 0040..0077...

verify randoms...
 0040: 6633       5621       7064       6502       4116       5460       6045       5404      
 0050: 2353       3512       1610       4655       7162       1455       0204       2604      
 0060: 6124       4103       6756       5633       3630       1045       0376       2535      
 0070: 6567       0310       5311       3551       4376       6131       6133       2620      
write randoms 0100..0137...
 0100
verify randoms...
 0100: 7356       6277       7265       5057       4612       2453       7610       5016      
 0110: 2501       1631       1034       3662       6530       6421       7232       5563      
 0120: 6133       7601       2607       0475       5310       1223       0230       0133      
 0130: 3302       1425       2162       0576       5154       3105       0162       0132      
write randoms 0140..0177...

verify randoms...
 0140: 6554       3664       2233       0117       0740       4636       3725       7377      
 0150: 7375       3102       3445       3022       6025       4306       6576       4673      
 0160: 2404       7613       1611       3307       2613       1267       3407       2102      
 0170: 1546       5553       2043       5753       4444       0007       6152       2125      
write randoms 0200..0237...


Here is example output with a broken memory:

verify randoms...
 0000: 7732       5502       0231!=1771 7464       3560       1064       5730       2377      
 0010: 7375       4246       1242!=1666 3224       4575       1537       3413       1176      
 0020: 7073       3573       0006!=1027 1612       0216       0744       2370       3205!=3204
 0030: 1007       5501       2044!=7144 7655       5171       6066       3023       2437      
 0040: 6355       4405       2353!=2757 6220       0563       6101       7601       6625      
 0050: 7147       3323       2014!=3614 3347       4360       5405       2725       0326      
 0060: 6773       6361!=2360 2757!=0714 7663       6745       6574       4167       0760      
 0070: 3444       2046       1104!=3104 2461       1606       7437       2360       4746      
 0100: 2425       0710       4006!=7327 0163       6610       7104       2370       3022      
 0110: 0246       1307       2014!=3515 3146       7261       0212       1023       5405      
 0120: 1526       1515       4102!=4123 3770       1405       0464       3416       2317!=2316
 0130: 0432       2531       5246!=5756 0713       4227       5537       0561       7164      
 0140: 0735       7736       0110!=3314 0066       5141       0377       6707       4753      
 0150: 6760       6203       1025!=3025 3551       5451       5634       7334       4476      
 0160: 7416       6567!=4567 7757!=7756 1442       7417       2605       1020       6734      
 0170: 3106       1414       0137!=0537 5142       2133       2045       3725       4332      
 0200: 3162       1570       0000!=0212 5752       3554       1153       4414       1204      
 0210: 3345       2605       0000!=3200 0417       5403       0227       1676       3160      
 0220: 5074       2470       0006!=6617 2367       3341       3555       5204       2455      
 0230: 1016       5226       2401!=2621 6533       3131       7554       1325       1741      
 0240: 6507       0162       0001!=2373 5762       1014       6243       7537       1353      
 0250: 7167       2177       0020!=0023 4406       7050       0275       0236       5150      
 0260: 7351       5001!=5000 4651!=4251 1177       1361       5122       6003       2647      
                                    ^ shorted diode                                        ^ bad inhibit driver resistor
                         ^ shorted diode

- - - - - - - - - - - - -

