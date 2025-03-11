//    Copyright (C) Mike Rieker, Beverly, MA USA
//    www.outerworldapps.com
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; version 2 of the License.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    EXPECT it to FAIL when someone's HeALTh or PROpeRTy is at RISk.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//
//    http://www.gnu.org/licenses/gpl-2.0.html

// Access the front panel lights and switches via I2C bus to PCB3 piggy-backed on backplane
// z8lpanel.cc -> i2czlib.cc -> pdp8lfpi2c.v -> i2cmaster.v -> i2c -> pcb3 -> mcp23017s -> lights & switches

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "i2czlib.h"
#include "z8ldefs.h"

// get switch/light value
#define GETVAL(bitno) ((vals[bitno/64] >> (bitno%64)) & 1)
// get whether toggle switch is being overidden
// = ! collectorpin | ! basepin
#define GETOVR(bitno) (((dirs[bitno/64] >> (bitno%64)) & GETVAL(R##bitno) & 1) ^ 1)

// write button switch value (args for writebut())
#define WRITEBUT(bitno) (bitno/64),(bitno%64)
// write toggle switch value (args for writetog())
#define WRITETOG(bitno) WRITEBUT(bitno),WRITEBUT(R##bitno)

#define IR00 0202
#define IR01 0216
#define IR02 0310

#define LINK 0016    // U1 GPB6
#define FET  0214    // U3 GPB4
#define ION  0215    // U3 GPB5
#define PARE 0201    // U3 GPA1
#define EXE  0200    // U3 GPA0
#define DEF  0311    // U4 GPB1
#define PRTE 0306    // U4 GPA6
#define WCT  0312    // U4 GPB3
#define CAD  0305    // U4 GPA5

#define STOP  0303
#define CONT  0314
#define START 0302
#define LDAD  0315
#define EXAM  0316
#define DEP   0300

#define SR00 0104
#define SR01 0000
#define SR02 0002
#define SR03 0005
#define SR04 0003
#define SR05 0213
#define SR06 0212
#define SR07 0206
#define SR08 0210
#define SR09 0103
#define SR10 0102
#define SR11 0304

#define MPRT 0010   // active low
#define DFLD 0012   // active low
#define IFLD 0006   // active low
#define STEP 0301   // active low

#define RSR00 0017
#define RSR01 0015
#define RSR02 0001
#define RSR03 0004
#define RSR04 0014
#define RSR05 0203
#define RSR06 0205
#define RSR07 0211
#define RSR08 0207
#define RSR09 0101
#define RSR10 0100
#define RSR11 0307

#define RMPRT 0011
#define RDFLD 0007
#define RIFLD 0013
#define RSTEP 0317

#define I2CUS(s,r) (10*2+15*(s)+10*(r)) // s=number bits sent, r=number bits rcvd

#define I2CBA  0x20     // I2C address of MCP23017 chip 0

#define IODIRA   0x00   // pin direction: '1'=input ; '0'=output
#define IODIRB   0x01   //   IO<7> must be set to 'output'
#define IPOLA    0x02   // input polarity: '1'=flip ; '0'=normal
#define IPOLB    0x03
#define GPINTENA 0x04   // interrupt on difference bitmask
#define GPINTENB 0x05
#define DEFVALA  0x06   // comparison value for interrupt on difference
#define DEFVALB  0x07
#define INTCONA  0x08   // more interrupt comparison
#define INTCONB  0x09
#define IOCONA   0x0A   // various control bits
#define IOCONB   0x0B
#define GPPUA    0x0C   // pullup enable bits
#define GPPUB    0x0D   //   '0' = disabled ; '1' = enabled
#define INTFA    0x0E   // interrupt flag bits
#define INTFB    0x0F
#define INTCAPA  0x10   // interrupt captured value
#define INTCAPB  0x11
#define GPIOA    0x12   // reads pin state
#define GPIOB    0x13
#define OLATA    0x14   // output pin latch
#define OLATB    0x15

#define ZYNQWR 3ULL     // write command (2 bits)
#define ZYNQRD 2ULL     // read command (2 bits)
#define ZYNQST 1ULL     // send restart bit (2 bits)
#define I2CWR 0ULL      // 8th address bit indicating write
#define I2CRD 1ULL      // 8th address bit indicating read

static pthread_mutex_t fpi2clock = PTHREAD_MUTEX_INITIALIZER;

I2CZLib::I2CZLib ()
{
    z8p   = NULL;
    pdpat = NULL;
    fpat  = NULL;

    z8p   = new Z8LPage ();
    pdpat = z8p->findev ("8L", NULL, NULL, false);
}

I2CZLib::~I2CZLib ()
{
    if (z8p != NULL) {
        delete z8p;
    }
    z8p   = NULL;
    pdpat = NULL;
    fpat  = NULL;
}

// see whether the i2c front panel is connected and powered on
bool I2CZLib::probei2c ()
{
    uint8_t addr = I2CBA;
    uint8_t reg  = 1;   // odd to leave '1' lingering on data line for ack bit

    // build 64-bit command
    //  send-8-bits-to-i2cbus 7-bit-address 0=write
    //  send-8-bits-to-i2cbus 8-bit-register-number
    //  send restart bit
    //  send-8-bits-to-i2cbus 7-bit-address 1=read
    //  read-8-bits-from-i2cbus (reg+0)
    //  read-8-bits-from-i2cbus (reg+1)
    //  0...
    uint64_t cmd =
        (ZYNQWR << 62) | ((uint64_t) addr << 55) | (I2CWR << 54) |
        (ZYNQWR << 52) | ((uint64_t) reg << 44) |
        (ZYNQST << 42) |
        (ZYNQWR << 40) | ((uint64_t) addr << 33) | (I2CRD << 32) |
        (ZYNQRD << 30) |
        (ZYNQRD << 28);

    locki2c ();
    uint64_t sts = doi2ccyclx (cmd, I2CUS(8+8+1+8+2,1+1+1+16), true);
    unlki2c ();
    return (sts >> 60) == 0;
}

void I2CZLib::openpads (bool brk, bool dislo4k, bool enlo4k, bool nobrk, bool real, bool sim)
{
    uint32_t volatile *cmat = z8p->findev ("CM", NULL, NULL, false);
    uint32_t volatile *xmat = z8p->findev ("XM", NULL, NULL, false);

    if (real) pdpat[Z_RE] &= ~ e_simit;
    if (sim)  pdpat[Z_RE] |=   e_simit;

    if (dislo4k) xmat[1] &= ~ XM_ENLO4K;
    if (enlo4k)  xmat[1] |=   XM_ENLO4K;

    if (brk)   cmat[2] &= ~ CM2_NOBRK;
    if (nobrk) cmat[2] |=   CM2_NOBRK;
}

// deposit 05252: jmp 05252 in memory then start it
void I2CZLib::loop52 ()
{
    if ((pdpat[Z_RE] & e_simit) || probei2c ()) {

        // make sure processor halted
        Z8LPanel pads;
        for (int i = 0; i < 10; i ++) {
            this->readmin (&pads);
            if (! pads.light.run) goto halted;
            pads.togovr.step = true;
            pads.togval.step = true;
            this->writepads (&pads);
            usleep (200000);
        }
        fprintf (stderr, "I2CZLib::loop52: processor would not halt\n");
        fprintf (stderr, "I2CZLib::loop52: try ./z8lpanel hardreset\n");
        ABORT ();
    halted:;

        // set switch register to 5252, all others to 0
        memset (&pads.togovr, -1, sizeof pads.togovr);
        pads.togval.sr = DOTJMPDOT;
        pads.togval.step = false;
        this->writepads (&pads);

        // load address
        pads.button.ldad = true;
        this->writepads (&pads);
        usleep (200000);
        pads.button.ldad = false;
        this->writepads (&pads);
        usleep (200000);

        // deposit
        pads.button.dep = true;
        this->writepads (&pads);
        usleep (200000);
        pads.button.dep = false;
        this->writepads (&pads);
        usleep (200000);

        // load address
        pads.button.ldad = true;
        this->writepads (&pads);
        usleep (200000);
        pads.button.ldad = false;
        this->writepads (&pads);
        usleep (200000);

        // start
        pads.button.start = true;
        this->writepads (&pads);
        usleep (200000);
        pads.button.start = false;
        this->writepads (&pads);
        usleep (200000);

        // see if that worked
        this->readmin (&pads);
        if (! pads.light.run || (pads.light.ma != DOTJMPDOT) || (pads.light.mb != DOTJMPDOT)) {
            fprintf (stderr, "I2CZLib::loop52: processor did not start (run=%o ma=%04o mb=%04o)\n",
                pads.light.run, pads.light.ma, pads.light.mb);
            ABORT ();
        }

        // release all switches
        this->relall ();
    } else {
        printf ("I2CZLib::loop52:\n");
        printf ("  stop\n");
        printf ("  set sr to %04o\n", DOTJMPDOT);
        printf ("  set all other switches to 0\n");
        printf ("  load address\n");
        printf ("  deposit\n");
        printf ("  load address\n");
        printf ("  start\n");

        // wait for processor to start
        Z8LPanel pads;
        uint32_t i = 0;
        do {
            usleep (1000000);
            this->readmin (&pads);
            if (++ i % 16 == 0) printf (" run=%o ma=%04o mb=%04o\n", pads.light.run, pads.light.ma, pads.light.mb);
            else putchar ('.');
            fflush (stdout);
        } while (! pads.light.run || (pads.light.ma != DOTJMPDOT) || (pads.light.mb != DOTJMPDOT));
        if (++ i % 16 != 0) putchar ('\n');
    }
}

// release our override of all switches
void I2CZLib::relall ()
{
    if (! (pdpat[Z_RE] & e_simit)) {
        uint16_t dirs[4] = { 0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU };
        uint16_t vals[4] = { 0xFFFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU };
        locki2c ();
        writei2c (dirs, vals);
        unlki2c ();
    }
}

// read light bulbs and switches
void I2CZLib::readpads (Z8LPanel *pads)
{
    readmin (pads);

    // if using simulator, get everything else from pdp8lsim.v
    if (pdpat[Z_RE] & e_simit) {
        uint32_t zrb = pdpat[Z_RB];
        uint32_t zrg = pdpat[Z_RG];

        pads->light.ir   = (zrg & g_lbIR) / g_lbIR0;
        pads->light.link = (zrg & g_lbLINK) != 0;
        pads->light.fet  = (zrg & g_lbFET)  != 0;
        pads->light.ion  = (zrg & g_lbION)  != 0;
        pads->light.exe  = (zrg & g_lbEXE)  != 0;
        pads->light.def  = (zrg & g_lbDEF)  != 0;
        pads->light.wct  = (zrg & g_lbWC)   != 0;
        pads->light.cad  = (zrg & g_lbCA)   != 0;

        pads->button.stop  = (zrb & b_swSTOP)  != 0;
        pads->button.cont  = (zrb & b_swCONT)  != 0;
        pads->button.start = (zrb & b_swSTART) != 0;
        pads->button.ldad  = (zrb & b_swLDAD)  != 0;
        pads->button.exam  = (zrb & b_swEXAM)  != 0;
        pads->button.dep   = (zrb & b_swDEP)   != 0;

        pads->togval.sr   = (zrb & b_swSR) / b_swSR0;
        pads->togval.mprt = (zrb & b_swMPRT) != 0;
        pads->togval.dfld = (zrb & b_swDFLD) != 0;
        pads->togval.ifld = (zrb & b_swIFLD) != 0;
        pads->togval.step = (zrb & b_swSTEP) != 0;

        pads->togovr.sr   = 07777;
        pads->togovr.mprt = true;
        pads->togovr.dfld = true;
        pads->togovr.ifld = true;
        pads->togovr.step = true;
    }

    // if using real PDP-8/L, get remaining values using I2C bus to MCP23017s
    else {
        uint16_t dirs[4], vals[4];
        locki2c ();
        readi2c (dirs, vals, false);
        unlki2c ();

        // light bulbs - all active low
        pads->light.ir   = ((GETVAL(IR00) << 2) |
                            (GETVAL(IR01) << 1) |
                            (GETVAL(IR02) << 0)) ^ 7;
        pads->light.link = ! GETVAL(LINK);  // U1 GPB6
        pads->light.fet  = ! GETVAL(FET);   // U3 GPB4
        pads->light.ion  = ! GETVAL(ION);   // U3 GPB5
        pads->light.pare = ! GETVAL(PARE);  // U3 GPA1
        pads->light.exe  = ! GETVAL(EXE);   // U3 GPA0
        pads->light.def  = ! GETVAL(DEF);   // U4 GPB1
        pads->light.prte = ! GETVAL(PRTE);  // U4 GPA6
        pads->light.wct  = ! GETVAL(WCT);   // U4 GPB3
        pads->light.cad  = ! GETVAL(CAD);   // U4 GPA5

        // momentary button - just read the pin (active low)
        pads->button.stop  = ! GETVAL(STOP);
        pads->button.cont  = ! GETVAL(CONT);
        pads->button.start = ! GETVAL(START);
        pads->button.ldad  = ! GETVAL(LDAD);
        pads->button.exam  = ! GETVAL(EXAM);
        pads->button.dep   = ! GETVAL(DEP);

        //  func    base  coll
        //  sense     1    hiZ
        //  drive-0   1     0
        //  drive-1   0    hiZ

        // toggle value = collector
        pads->togval.sr =                               // active high
            (GETVAL(SR00) << 11) |
            (GETVAL(SR01) << 10) |
            (GETVAL(SR02) <<  9) |
            (GETVAL(SR03) <<  8) |
            (GETVAL(SR04) <<  7) |
            (GETVAL(SR05) <<  6) |
            (GETVAL(SR06) <<  5) |
            (GETVAL(SR07) <<  4) |
            (GETVAL(SR08) <<  3) |
            (GETVAL(SR09) <<  2) |
            (GETVAL(SR10) <<  1) |
            (GETVAL(SR11) <<  0);

        pads->togval.mprt = ! GETVAL(MPRT);   // active low
        pads->togval.dfld = ! GETVAL(DFLD);   // active low
        pads->togval.ifld = ! GETVAL(IFLD);   // active low
        pads->togval.step = ! GETVAL(STEP);   // active low

        // toggle being overidden = ! (collector-pin-hiz & base-pin-one)
        pads->togovr.sr =
            (GETOVR(SR00) << 11) |
            (GETOVR(SR01) << 10) |
            (GETOVR(SR02) <<  9) |
            (GETOVR(SR03) <<  8) |
            (GETOVR(SR04) <<  7) |
            (GETOVR(SR05) <<  6) |
            (GETOVR(SR06) <<  5) |
            (GETOVR(SR07) <<  4) |
            (GETOVR(SR08) <<  3) |
            (GETOVR(SR09) <<  2) |
            (GETOVR(SR10) <<  1) |
            (GETOVR(SR11) <<  0);

        pads->togovr.mprt = GETOVR(MPRT);
        pads->togovr.dfld = GETOVR(DFLD);
        pads->togovr.ifld = GETOVR(IFLD);
        pads->togovr.step = GETOVR(STEP);
    }
}

// read pads that come directly from the zynq without I2C bus
// works with simulator or real pdp
void I2CZLib::readmin (Z8LPanel *pads)
{
    memset (pads, 0, sizeof *pads);

    uint32_t zra = pdpat[Z_RA];
    uint32_t zrf = pdpat[Z_RF];
    uint32_t zrh = pdpat[Z_RH];
    uint32_t zri = pdpat[Z_RI];

    pads->light.ma  = (zri & i_oMA)  / i_oMA0;
    pads->light.mb  = (zrh & h_oBMB) / h_oBMB0;
    pads->light.ac  = (zrh & h_oBAC) / h_oBAC0;
    pads->light.ema = (zra & a_iEMA) != 0;
    pads->light.run = (zrf & f_oB_RUN) != 0;
    pads->light.brk = (zrf & f_o_B_BREAK) == 0;
}

// write overidden switches
//  buttons are overidden whenever being asserted
//          the real switch is assumed to be negated during this time
//            ie, no one is pressing any of the switches
//          so there are no transistors on the lines
//  toggles are overidden whenever togovr is true
//          real switch will be overidden regardless of real switch position
//          a transistor is used when outputting a '1' in case it is overriding a switch of '0'
void I2CZLib::writepads (Z8LPanel const *pads)
{
    if (pdpat[Z_RE] & e_simit) {

        // using pdp8lsim.v simulator - set switch bits directly
        pdpat[Z_RB] =
            (pads->button.stop  ? b_swSTOP  : 0) |
            (pads->button.cont  ? b_swCONT  : 0) |
            (pads->button.start ? b_swSTART : 0) |
            (pads->button.ldad  ? b_swLDAD  : 0) |
            (pads->button.exam  ? b_swEXAM  : 0) |
            (pads->button.dep   ? b_swDEP   : 0) |
            (pads->togval.mprt  ? b_swMPRT  : 0) |
            (pads->togval.dfld  ? b_swDFLD  : 0) |
            (pads->togval.ifld  ? b_swIFLD  : 0) |
            (pads->togval.step  ? b_swSTEP  : 0) |
            ((pads->togval.sr * b_swSR0) & b_swSR);
    } else {

        // using real pdp, set switches via z8lpanel board on i2c bus

        // read current settings
        uint16_t dirs[4], vals[4];
        locki2c ();
        readi2c (dirs, vals, true);

        // buttons are all active low
        // when asserted, drive the line low overriding the presumed high state of physical switch
        //   should be < 10mA each well under 25mA per pin limit, and even if all on, 60mA well within 125mA per-chip limit
        // when negated, hi-Z the pin, allowing phys switch to operate, and so readpads() can read phys switch state
        writebut (dirs, vals, pads->button.stop,  WRITEBUT(STOP));
        writebut (dirs, vals, pads->button.cont,  WRITEBUT(CONT));
        writebut (dirs, vals, pads->button.start, WRITEBUT(START));
        writebut (dirs, vals, pads->button.ldad,  WRITEBUT(LDAD));
        writebut (dirs, vals, pads->button.exam,  WRITEBUT(EXAM));
        writebut (dirs, vals, pads->button.dep,   WRITEBUT(DEP));

        // toggles are all active high
        // if not overriding, base line driven high to shut off transistor and value line put in hi-Z
        // if outputting 0, base line driven high to shut off transistor and value line driven low to output 0
        //   should be < 10mA each well under 25mA per pin limit, and even if all on, 60mA well within 125mA per-chip limit
        // if outputting 1, base line driven low to turn on transistor and value line put in hi-Z
        //   would be > 15mA each, and if all on, 90mA would push the per-chip 125mA total limit, so we use a transistor

        // phase 1: hi-Z things; 2: drive things
        for (int phase = 1; phase <= 2; phase ++) {
            writetog (dirs, vals, (pads->togval.sr >> 11) & 1, (pads->togovr.sr >> 11) & 1, WRITETOG(SR00));    // active high
            writetog (dirs, vals, (pads->togval.sr >> 10) & 1, (pads->togovr.sr >> 10) & 1, WRITETOG(SR01));    // active high
            writetog (dirs, vals, (pads->togval.sr >>  9) & 1, (pads->togovr.sr >>  9) & 1, WRITETOG(SR02));    // active high
            writetog (dirs, vals, (pads->togval.sr >>  8) & 1, (pads->togovr.sr >>  8) & 1, WRITETOG(SR03));    // active high
            writetog (dirs, vals, (pads->togval.sr >>  7) & 1, (pads->togovr.sr >>  7) & 1, WRITETOG(SR04));    // active high
            writetog (dirs, vals, (pads->togval.sr >>  6) & 1, (pads->togovr.sr >>  6) & 1, WRITETOG(SR05));    // active high
            writetog (dirs, vals, (pads->togval.sr >>  5) & 1, (pads->togovr.sr >>  5) & 1, WRITETOG(SR06));    // active high
            writetog (dirs, vals, (pads->togval.sr >>  4) & 1, (pads->togovr.sr >>  4) & 1, WRITETOG(SR07));    // active high
            writetog (dirs, vals, (pads->togval.sr >>  3) & 1, (pads->togovr.sr >>  3) & 1, WRITETOG(SR08));    // active high
            writetog (dirs, vals, (pads->togval.sr >>  2) & 1, (pads->togovr.sr >>  2) & 1, WRITETOG(SR09));    // active high
            writetog (dirs, vals, (pads->togval.sr >>  1) & 1, (pads->togovr.sr >>  1) & 1, WRITETOG(SR10));    // active high
            writetog (dirs, vals, (pads->togval.sr >>  0) & 1, (pads->togovr.sr >>  0) & 1, WRITETOG(SR11));    // active high

            writetog (dirs, vals, ! pads->togval.mprt, pads->togovr.mprt, WRITETOG(MPRT));                      // active low
            writetog (dirs, vals, ! pads->togval.dfld, pads->togovr.dfld, WRITETOG(DFLD));                      // active low
            writetog (dirs, vals, ! pads->togval.ifld, pads->togovr.ifld, WRITETOG(IFLD));                      // active low
            writetog (dirs, vals, ! pads->togval.step, pads->togovr.step, WRITETOG(STEP));                      // active low

            writei2c (dirs, vals);
        }
        unlki2c ();
    }
}

// write button pin (active low)
//  input:
//   val = true: button pressed (on, 0V)
//        false: button released (off, +5V)
void I2CZLib::writebut (uint16_t *dirs, uint16_t *vals,
    bool val, int validx, int valbit)
{
    if (val) {
        // assert by driving output pin to zero
        dirs[validx] &= ~ (1U << valbit);
        vals[validx] &= ~ (1U << valbit);
    } else {
        // negate by putting output pin at hi-Z
        // real panel switch provides pullup
        // using hi-Z lets real panel switch operate
        dirs[validx] |=   (1U << valbit);
        vals[validx] |=   (1U << valbit);
    }
}

// write toggle pins
//  there is a PNP transistor for each toggle switch
//  for each transistor we have
//   a pin going through a resistor to its base
//   a pin going to its collector then going to the switch line
//  all the emitters go to +5
//  to let the real switch operate:
//   drive base high (turn off transistor)
//   hi-Z the collector line
//  to drive switch line low:
//   drive base high (turn off transistor)
//   drive the collector line low (5mA if real switch on)
//  to drive switch line high:
//   drive base low (turn on transistor, 18mA if real switch off)
//   hi-Z the collector line
//  input:
//   ovr = true: drive the output pin with 'val'
//        false: hi-Z the output pin
//   val = value to drive if ovr
//  output:
//   dirs<base> = 0, ie, always driving base with vals<base>
//   vals<base> = 0: driving collector pin high; 1: deferring to collector pin settings
//   dirs<coll> = 0: driving collector pin with vals<coll>; 1: sensing real switch with collector pin
//   vals<coll> = 0: driving collector pin low; 1: driving collector pin high
//  note:
//   cannot have the transistor on (base low) and driving collector line low at same time
//   or it will cook the collector line, so writetog() must be called twice to do transition
void I2CZLib::writetog (uint16_t *dirs, uint16_t *vals, bool val, bool ovr,
    int collidx, int collbit, int baseidx, int basebit)
{
    uint16_t basemsk = 1U << basebit;
    uint16_t collmsk = 1U << collbit;
    dirs[baseidx] &= ~ basemsk;             // base line is always an output
    vals[collidx] &= ~ collmsk;             // collector line is always 0 or hi-Z
    if (! ovr) {
        dirs[collidx] |= collmsk;           // hi-Z the collector line
        vals[baseidx] |= basemsk;           // drive base line high to shut off transistor
    } else if (val) {
        if (dirs[collidx] & collmsk) {      // see if collector line already hi-Z
            vals[baseidx] &= ~ basemsk;     // drive base line low to turn on transistor
        } else {
            dirs[collidx] |= collmsk;       // hi-Z the collector line
        }
    } else {
        if (vals[baseidx] & basemsk) {      // see if transistor already shut off
            dirs[collidx] &= ~ collmsk;     // turn on collector line, value is always 0
        } else {
            vals[baseidx] |= basemsk;       // drive base line high to shut off transistor
        }
    }
}

void I2CZLib::locki2c ()
{
    if (pthread_mutex_lock (&fpi2clock) != 0) ABORT ();

    if (fpat == NULL) {
        fpat = z8p->findev ("FP", NULL, NULL, false);
        mypid = getpid ();
    }

    ASSERT (FP6_LOCK == 0xFFFFFFFFU);
    for (int i = 0; i < 1000000; i ++) {
        fpat[6] = mypid;
        int lkpid = fpat[6];
        if (lkpid == mypid) return;
        if ((lkpid > 0) && (kill (lkpid, 0) < 0) && (errno == ESRCH)) {
            fprintf (stderr, "I2CZLib::locki2c: releasing i2c lock from pid %d\n", lkpid);
            fpat[6] = lkpid;
        }
        usleep (100);
    }
    fprintf (stderr, "I2CZLib::locki2c: timed out locking i2c bus\n");
    ABORT ();
}

void I2CZLib::unlki2c ()
{
    ASSERT (FP6_LOCK == 0xFFFFFFFFU);
    if ((int) fpat[6] != mypid) ABORT ();
    fpat[6] = mypid;
    if ((int) fpat[6] == mypid) ABORT ();
    if (pthread_mutex_unlock (&fpi2clock) != 0) ABORT ();
}

void I2CZLib::readi2c (uint16_t *dirs, uint16_t *vals, bool latch)
{
    for (int i = 0; i < 4; i ++) {
        dirs[i] = read16 (I2CBA + i, IODIRA);
        vals[i] = read16 (I2CBA + i, latch ? OLATA : GPIOA);
    }
}

void I2CZLib::writei2c (uint16_t *dirs, uint16_t *vals)
{
    for (int i = 0; i < 4; i ++) {
        write16 (I2CBA + i, IODIRA, dirs[i]);
        write16 (I2CBA + i, OLATA,  vals[i]);
    }
}

// read 16-bit value from MCP23017 on I2C bus
//  input:
//   addr = MCP23017 address
//   reg = starting register within MCP23017
//  output:
//   returns [07:00] = reg+0 contents
//           [15:08] = reg+1 contents
uint16_t I2CZLib::read16 (uint8_t addr, uint8_t reg)
{
    // build 64-bit command
    //  send-8-bits-to-i2cbus 7-bit-address 0=write
    //  send-8-bits-to-i2cbus 8-bit-register-number
    //  send restart bit
    //  send-8-bits-to-i2cbus 7-bit-address 1=read
    //  read-8-bits-from-i2cbus (reg+0)
    //  read-8-bits-from-i2cbus (reg+1)
    //  0...
    uint64_t cmd =
        (ZYNQWR << 62) | ((uint64_t) addr << 55) | (I2CWR << 54) |
        (ZYNQWR << 52) | ((uint64_t) reg << 44) |
        (ZYNQST << 42) |
        (ZYNQWR << 40) | ((uint64_t) addr << 33) | (I2CRD << 32) |
        (ZYNQRD << 30) |
        (ZYNQRD << 28);

    uint64_t sts = doi2ccycle (cmd, I2CUS(8+8+1+8+2,1+1+1+16));

    // values in low 16 bits of status register
    //  <15:08> = reg+0
    //  <07:00> = reg+1
    return (uint16_t) ((sts << 8) | ((sts >> 8) & 0xFFU));
}

// write 16-bit value to MCP23017 registers on I2C bus
//  input:
//   addr = MCP23017 address
//   reg = starting register within MCP23017
//   word = value to write
//          word[07:00] => reg+0
//          word[15:08] => reg+1
void I2CZLib::write16 (uint8_t addr, uint8_t reg, uint16_t word)
{
    // build 64-bit command
    //  send-8-bits-to-i2cbus 7-bit-address 1=read
    //  send-8-bits-to-i2cbus 8-bit-register-number
    //  send-8-bits-to-i2cbus 8-bit-reg+0-value
    //  send-8-bits-to-i2cbus 8-bit-reg+1-value
    //  0...
    uint64_t cmd =
        (ZYNQWR << 62) | ((uint64_t) addr << 55) | (I2CWR << 54) |
        (ZYNQWR << 52) | ((uint64_t) reg << 44) |
        (ZYNQWR << 42) | ((uint64_t) (word & 0xFFU) << 34) |
        (ZYNQWR << 32) | ((uint64_t) (word >> 8) << 24);

    doi2ccycle (cmd, I2CUS(8+8+16,1+1+2));
}

// send command to MCP23017 via pdp8lfpi2c.v then read response
// - print and abort on error
uint64_t I2CZLib::doi2ccycle (uint64_t cmd, int i2cus)
{
    uint64_t sts = doi2ccyclx (cmd, i2cus, false);
    if ((sts >> 60) != 0) {
        fprintf (stderr, "I2CZLib::doi2ccycle: MCP23017 I/O error %016llX\n", (long long unsigned) sts);
        ABORT ();
    }
    return sts;
}

// send command to MCP23017 via pdp8lfpi2c.v then read response
// - return error status, don't print or abort
uint64_t I2CZLib::doi2ccyclx (uint64_t cmd, int i2cus, bool quiet)
{
    uint32_t sts1;
    for (int retry = 0; retry < 3; retry ++) {

        if (retry > 0) reseti2c (quiet);

        // writing high-order word triggers i/o, so write low-order first
        ASSERT (FP1_CMDLO == 0xFFFFFFFFU);
        ASSERT (FP2_CMDHI == 0xFFFFFFFFU);
        fpat[1] = cmd;
        fpat[2] = cmd >> 32;

        // do some sleeping
        usleep (i2cus - 50);

        // finish wait for busy bit to clear
        for (int i = 1000000; -- i >= 0;) {
            ASSERT (FP4_STSHI == 0xFFFFFFFFU);
            sts1 = fpat[4];
            if (! (sts1 & (1U << 31))) break;
        }

        // return if success
        if ((sts1 >> 28) == 0) break;
    }

    ASSERT (FP3_STSLO == 0xFFFFFFFFU);
    uint32_t sts0 = fpat[3];
    return (((uint64_t) sts1) << 32) | sts0;
}

void I2CZLib::reseti2c (bool quiet)
{
    // data line shoule be high (open)
    // if not, try pulsing clock line low then high then re-check
    for (int i = 0; i < 100; i ++) {
        fpat[5] = FP5_I2CDAO | FP5_I2CCLO | FP5_MANUAL;
        usleep (10);
        if (fpat[5] & FP5_I2CDAI) {
            if ((i != 0) && ! quiet) fprintf (stderr, "I2CZLib::reseti2c: i2c data line unstuck\n");
            goto datok;
        }
        if ((i == 0) && ! quiet) fprintf (stderr, "I2CZLib::reseti2c: i2c data line stuck low\n");
        fpat[5] = FP5_I2CDAO | FP5_MANUAL;
        usleep (10);
    }
    if (! quiet) {
        fprintf (stderr, "I2CZLib::reseti2c: failed to clear i2c data line\n");
        ABORT ();
    }
    return;

    // pulse data low then high (stop bit)
    // also reset i2cmaster.v
datok:;
    fpat[5] = FP5_I2CCLO | FP5_MANUAL | FP5_CLEAR;
    usleep (10);
    fpat[5] = FP5_I2CDAO | FP5_I2CCLO | FP5_MANUAL;
    usleep (10);

    // turn the i2c lines over to i2cmaster.v
    fpat[5] = 0;
}
