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

// Connects z8lpanel.cc to the Zynq which accesses the front panel lights and switches
// via I2C bus to PCB3 piggy-backed on backplane

#include <string.h>

#include "i2czlib.h"
#include "z8ldefs.h"

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

#define ZPI2CCMD 1      // i2cmaster.v command register (64 bits)
#define ZPI2CSTS 3      // i2cmaster.v status register (64 bits)
#define ZYNQWR 3ULL     // write command (2 bits)
#define ZYNQRD 2ULL     // read command (2 bits)
#define ZYNQST 1ULL     // send restart bit (2 bits)
#define I2CWR 0ULL      // 8th address bit indicating write
#define I2CRD 1ULL      // 8th address bit indicating read
#define ZCLEAR  4
#define ZSTEPON 2       // turn on manual stepping for debug
#define ZSTEPIT 1

I2CZLib::I2CZLib ()
{ }

I2CZLib::~I2CZLib ()
{ }

void I2CZLib::openpads (bool dislo4k, bool enlo4k, bool real, bool sim)
{
    z8p   = new Z8LPage ();
    pdpat = z8p->findev ("8L", NULL, NULL, false);
    fpat  = z8p->findev ("FP", NULL, NULL, false);

    uint32_t volatile *xmat = z8p->findev ("XM", NULL, NULL, false);
    if (dislo4k) xmat[1] &= ~ XM_ENLO4K;
    if (enlo4k)  xmat[1] |=   XM_ENLO4K;

    if (real) pdpat[Z_RE] &= ~ e_simit;
    if (sim)  pdpat[Z_RE] |=   e_simit;
    pdpat[Z_RE] |= e_nanocontin;
}

// read light bulbs and switches
void I2CZLib::readpads (Z8LPanel *pads)
{
    memset (pads, 0, sizeof *pads);

    // these values are available in the zynq directly, real or sim mode
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
        for (int i = 0; i < 4; i ++) {
            dirs[i] = read16 (I2CBA + i, IODIRA);
            vals[i] = read16 (I2CBA + i, GPIOA);
        }

        pads->light.ir   = (((vals[2] >> 002) & 1) << 2) |
                           (((vals[2] >> 016) & 1) << 1) |
                           (((vals[3] >> 010) & 1) << 0);
        pads->light.link = (vals[0] >> 016) & 1;    // U1 GPB6
        pads->light.fet  = (vals[2] >> 014) & 1;    // U3 GPB4
        pads->light.ion  = (vals[2] >> 015) & 1;    // U3 GPB5
        pads->light.pare = (vals[2] >> 001) & 1;    // U3 GPA1
        pads->light.exe  = (vals[2] >> 000) & 1;    // U3 GPA0
        pads->light.def  = (vals[3] >> 011) & 1;    // U4 GPB1
        pads->light.prte = (vals[3] >> 006) & 1;    // U4 GPA6
        pads->light.wct  = (vals[3] >> 012) & 1;    // U4 GPB3
        pads->light.cad  = (vals[3] >> 005) & 1;    // U4 GPA5

        // momentary button - just read the pin (active low)
        pads->button.stop  = ! ((vals[3] >> 003) & 1);
        pads->button.cont  = ! ((vals[3] >> 014) & 1);
        pads->button.start = ! ((vals[3] >> 002) & 1);
        pads->button.ldad  = ! ((vals[3] >> 015) & 1);
        pads->button.exam  = ! ((vals[3] >> 016) & 1);
        pads->button.dep   = ! ((vals[3] >> 000) & 1);

        //  func    base  coll
        //  sense     1    hiZ
        //  drive-0   1     0
        //  drive-1   0    hiZ

        // toggle value = collector
        pads->togval.sr =
            (((vals[1] >> 004) & 1) << 11) |
            (((vals[0] >> 000) & 1) << 10) |
            (((vals[0] >> 002) & 1) <<  9) |
            (((vals[0] >> 005) & 1) <<  8) |
            (((vals[0] >> 003) & 1) <<  7) |
            (((vals[2] >> 013) & 1) <<  6) |
            (((vals[2] >> 012) & 1) <<  5) |
            (((vals[2] >> 006) & 1) <<  4) |
            (((vals[2] >> 010) & 1) <<  3) |
            (((vals[1] >> 003) & 1) <<  2) |
            (((vals[1] >> 002) & 1) <<  1) |
            (((vals[3] >> 004) & 1) <<  0);

        pads->togval.mprt = (vals[0] >> 010) & 1;
        pads->togval.dfld = (vals[0] >> 012) & 1;
        pads->togval.ifld = (vals[0] >> 006) & 1;
        pads->togval.step = (vals[3] >> 001) & 1;

        // toggle being overidden = ! (collector-pin-hiz & base-pin-one)
        pads->togovr.sr = (
            (((dirs[1] >> 004) & (vals[0] >> 017) & 1) << 11) |
            (((dirs[0] >> 000) & (vals[0] >> 015) & 1) << 10) |
            (((dirs[0] >> 002) & (vals[0] >> 001) & 1) <<  9) |
            (((dirs[0] >> 005) & (vals[0] >> 004) & 1) <<  8) |
            (((dirs[0] >> 003) & (vals[0] >> 014) & 1) <<  7) |
            (((dirs[2] >> 013) & (vals[2] >> 003) & 1) <<  6) |
            (((dirs[2] >> 012) & (vals[2] >> 005) & 1) <<  5) |
            (((dirs[2] >> 006) & (vals[2] >> 011) & 1) <<  4) |
            (((dirs[2] >> 010) & (vals[2] >> 007) & 1) <<  3) |
            (((dirs[1] >> 003) & (vals[1] >> 001) & 1) <<  2) |
            (((dirs[1] >> 002) & (vals[1] >> 000) & 1) <<  1) |
            (((dirs[3] >> 004) & (vals[3] >> 007) & 1) <<  0)) ^ 07777;

        pads->togovr.mprt = ((dirs[0] >> 010) & (vals[0] >> 011) & 1) ^ 1;
        pads->togovr.dfld = ((dirs[0] >> 012) & (vals[0] >> 007) & 1) ^ 1;
        pads->togovr.ifld = ((dirs[0] >> 006) & (vals[0] >> 013) & 1) ^ 1;
        pads->togovr.step = ((dirs[3] >> 001) & (vals[3] >> 017) & 1) ^ 1;
    }
}

// write overidden switches
void I2CZLib::writepads (Z8LPanel const *pads)
{
    if (pdpat[Z_RE] & e_simit) {
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
        uint16_t dirs[4], vals[4];
        for (int i = 0; i < 4; i ++) {
            dirs[i] = read16 (I2CBA + i, IODIRA);
            vals[i] = read16 (I2CBA + i, GPIOA);
        }

        writebut (dirs, vals, pads->button.stop,  3, 003);
        writebut (dirs, vals, pads->button.cont,  3, 014);
        writebut (dirs, vals, pads->button.start, 3, 002);
        writebut (dirs, vals, pads->button.ldad,  3, 015);
        writebut (dirs, vals, pads->button.exam,  3, 016);
        writebut (dirs, vals, pads->button.dep,   3, 000);

        writetog (dirs, vals, (pads->togval.sr >> 11) & 1, (pads->togovr.sr >> 11) & 1, 1, 004, 0, 017);
        writetog (dirs, vals, (pads->togval.sr >> 10) & 1, (pads->togovr.sr >> 10) & 1, 0, 000, 0, 015);
        writetog (dirs, vals, (pads->togval.sr >>  9) & 1, (pads->togovr.sr >>  9) & 1, 0, 002, 0, 001);
        writetog (dirs, vals, (pads->togval.sr >>  8) & 1, (pads->togovr.sr >>  8) & 1, 0, 005, 0, 004);
        writetog (dirs, vals, (pads->togval.sr >>  7) & 1, (pads->togovr.sr >>  7) & 1, 0, 003, 0, 014);
        writetog (dirs, vals, (pads->togval.sr >>  6) & 1, (pads->togovr.sr >>  6) & 1, 2, 013, 2, 003);
        writetog (dirs, vals, (pads->togval.sr >>  5) & 1, (pads->togovr.sr >>  5) & 1, 2, 012, 2, 005);
        writetog (dirs, vals, (pads->togval.sr >>  4) & 1, (pads->togovr.sr >>  4) & 1, 2, 006, 2, 011);
        writetog (dirs, vals, (pads->togval.sr >>  3) & 1, (pads->togovr.sr >>  3) & 1, 2, 010, 2, 007);
        writetog (dirs, vals, (pads->togval.sr >>  2) & 1, (pads->togovr.sr >>  2) & 1, 1, 003, 1, 001);
        writetog (dirs, vals, (pads->togval.sr >>  1) & 1, (pads->togovr.sr >>  1) & 1, 1, 002, 1, 000);
        writetog (dirs, vals, (pads->togval.sr >>  0) & 1, (pads->togovr.sr >>  0) & 1, 3, 004, 3, 007);

        writetog (dirs, vals, pads->togval.mprt, pads->togovr.mprt, 0, 010, 0, 011);
        writetog (dirs, vals, pads->togval.dfld, pads->togovr.dfld, 0, 012, 0, 007);
        writetog (dirs, vals, pads->togval.ifld, pads->togovr.ifld, 0, 006, 0, 013);
        writetog (dirs, vals, pads->togval.step, pads->togovr.step, 3, 001, 3, 017);

        for (int i = 0; i < 4; i ++) {
            write16 (I2CBA + i, IODIRA, dirs[i]);
            write16 (I2CBA + i, GPIOA,  vals[i]);
        }
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

// write toggle pins (active high)
//  input:
//   ovr = true: drive the output pin with 'val'
//        false: hi-Z the output pin
//   val = value to drive if ovr
void I2CZLib::writetog (uint16_t *dirs, uint16_t *vals, bool val, bool ovr,
    int baseidx, int basebit, int collidx, int collbit)
{
    dirs[baseidx] &= ~ (1U << basebit);     // base line is always an output
    vals[collidx] &= ~ (1U << collbit);     // collector line is always 0 or hi-Z
    if (! ovr) {
        dirs[collidx] |= 1U << collbit;     // hi-Z the collector line
        vals[baseidx] |= 1U << basebit;     // drive base line high to shut off transistor
    } else if (val) {
        dirs[collidx] |=    1U << collbit;  // hi-Z the collector line
        vals[baseidx] &= ~ (1U << basebit); // drive base line low to turn on transistor
    } else {
        dirs[collidx] &= ~ (1U << collbit); // turn on collector line, value is always 0
        vals[baseidx] |=    1U << basebit;  // drive base line high to shut off transistor
    }
}

// read 8-bit value from mcp23017 reg at addr
uint8_t I2CZLib::read8 (uint8_t addr, uint8_t reg)
{
    // build 64-bit command
    //  send-8-bits-to-i2cbus 7-bit-address 0=write
    //  send-8-bits-to-i2cbus 8-bit-register-number
    //  send restart bit
    //  send-8-bits-to-i2cbus 7-bit-address 1=read
    //  read-8-bits-from-i2cbus
    //  0...
    uint64_t cmd =
        (ZYNQWR << 62) | ((uint64_t) addr << 55) | (I2CWR << 54) |
        (ZYNQWR << 52) | ((uint64_t) reg << 44) |
        (ZYNQST << 42) |
        (ZYNQWR << 40) | ((uint64_t) addr << 33) | (I2CRD << 32) |
        (ZYNQRD << 30);

    doi2ccycle (cmd);

    // value in low 8 bits of status register
    uint32_t sts0 = fpat[ZPI2CSTS+0];
    return (uint8_t) sts0;
}

// write 8-bit value to mcp23017 reg at addr
void I2CZLib::write8 (uint8_t addr, uint8_t reg, uint8_t byte)
{
    // build 64-bit command
    //  send-8-bits-to-i2cbus 7-bit-address 0=write
    //  send-8-bits-to-i2cbus 8-bit-register-number
    //  send-8-bits-to-i2cbus 8-bit-register-value
    //  0...
    uint64_t cmd =
        (ZYNQWR << 62) | ((uint64_t) addr << 55) | (I2CWR << 54) |
        (ZYNQWR << 52) | ((uint64_t) reg << 44) |
        (ZYNQWR << 42) | ((uint64_t) byte << 34);

    doi2ccycle (cmd);
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

    doi2ccycle (cmd);

    // values in low 16 bits of status register
    //  <15:08> = reg+0
    //  <07:00> = reg+1
    uint32_t sts0 = fpat[ZPI2CSTS+0];
    return (uint16_t) ((sts0 << 8) | ((sts0 >> 8) & 0xFFU));
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

    doi2ccycle (cmd);
}

// send command to MCP23017 via pdp8lfpi2c.v and wait for response
uint32_t I2CZLib::doi2ccycle (uint64_t cmd)
{
    // writing high-order word triggers i/o, so write low-order first
    fpat[ZPI2CCMD+0] = cmd;
    fpat[ZPI2CCMD+1] = cmd >> 32;

    // wait for busy bit to clear
    for (int i = 1000000; -- i >= 0;) {
        uint32_t sts1 = fpat[ZPI2CSTS+1];
        if (! (sts1 & (1U << 31))) {

            // make sure ack was received from mcp23017
            if (sts1 & (1U << 30)) {
                fprintf (stderr, "I2CZLib::doi2ccycle: mcp23017 did not respond\n");
                ABORT ();
            }
            return sts1;
        }
    }
    fprintf (stderr, "I2CZLib::write16: i2c I/O timeout\n");
    ABORT ();
}

/***
    capture[capcount++] = fpat[5] >> 24;

static void printcap ()
{
    printbit ("SCL", 0x80);
    printbit ("SDO", 0x40);
    printbit ("SDI", 0x20);
}

static void printbit (char const *name, uint8_t bit)
{
    bool lastbit, thisbit, nextbit;

    putchar ('\n');

    fputs ("   ", stdout);

    lastbit = (capture[0] & bit) != 0;
    for (int i = 1; (i < capcount - 1) && (i < 2000); i ++) {
        thisbit = (capture[i+0] & bit) != 0;
        nextbit = (capture[i+1] & bit) != 0;
        putchar ((thisbit & lastbit & nextbit) ? '_' : ' ');
        lastbit = thisbit;
    }
    putchar ('\n');

    fputs (name, stdout);

    lastbit = (capture[0] & bit) != 0;
    for (int i = 1; (i < capcount - 1) && (i < 2000); i ++) {
        thisbit = (capture[i+0] & bit) != 0;
        nextbit = (capture[i+1] & bit) != 0;
        if (! lastbit &   nextbit) putchar ('/');
        if (  lastbit & ! nextbit) putchar ('\\');
        if (! lastbit & ! nextbit) putchar ('_');
        if (  lastbit &   nextbit) putchar (' ');
        lastbit = thisbit;
    }
    putchar ('\n');
}
***/
