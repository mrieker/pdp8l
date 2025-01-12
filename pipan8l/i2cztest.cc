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

// Tests I2C code in Zynq that accesses front panel board that is piggy-backed to backplane
// Does not require board to be actually plugged into backplane, just tests the I2C bus connection

#include <signal.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "z8ldefs.h"
#include "z8lutil.h"

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
#define SCLK 0x80000000U
#define SDAO 0x40000000U
#define SDAI 0x20000000U
#define MANUAL  8U      // 1=use SCLK,SDAO directly; 0=use i2cmaster.v
#define ZCLEAR  4U      // reset i2cmaster.v
#define ZSTEPON 2U      // 1=use ZSTEPIT to clock i2cmaster.v; 0=use 100MHz FPAG clock
#define ZSTEPIT 1U

#define ILADEPTH 32768  // total number of elements in ilaarray
#define ILAAFTER 32760  // number of samples to take after sample containing trigger

#define ILACTL 021
#define ILADAT 022

#define ILACTL_ARMED  0x80000000U
#define ILACTL_ILAAFTER0 0x00010000U
#define ILACTL_INDEX0 0x00000001U
#define ILACTL_ILAAFTER  (ILACTL_ILAAFTER0 * (ILADEPTH - 1))
#define ILACTL_INDEX  (ILACTL_INDEX0 * (ILADEPTH - 1))

static bool volatile aborted;
static uint32_t volatile *pdpat;
static uint32_t volatile *fpat;

uint8_t read8 (uint8_t addr, uint8_t reg);
void write8 (uint8_t addr, uint8_t reg, uint8_t byte);
uint16_t read16 (uint8_t addr, uint8_t reg);
void write16 (uint8_t addr, uint8_t reg, uint16_t word);
uint64_t doi2ccycle (uint64_t cmd);
void ilaarm ();
void ilasave (uint64_t *save);
void iladump (uint64_t const *save);

// when using software I2C code
// fpga I2C code has its own internal timing
void bitdelay ()
{
    uint32_t beg, now;
    asm volatile ("mrc p15,0,%0,c9,c13,0" : "=r" (beg));
    do asm volatile ("mrc p15,0,%0,c9,c13,0" : "=r" (now));
    while (now - beg < 666);   // 1uS
    //                3333);   // 5uS
}

// have ctrl-C abort on I2C message boundaries
// only useful when using software I2C code
// fpga I2C code always tries to complete the message
void siginthand (int signum)
{
    if (aborted) exit (1);
    aborted = true;
}

int main ()
{
    setlinebuf (stdout);

    Z8LPage *z8p = new Z8LPage ();
    pdpat = z8p->findev ("8L", NULL, NULL, false);
    fpat  = z8p->findev ("FP", NULL, NULL, false);

    // reset the I2C code (ZCLEAR) then leave SCLK,SDAO set
    pdpat[Z_RE] = e_simit | e_nanocontin;
    fpat[5] = SCLK | SDAO | ZCLEAR;
    usleep (1000);
    fpat[5] = SCLK | SDAO;

    // order of addressing MCP23017s
    uint16_t ns[4] = { 0, 1, 2, 3 };

    // set up to detect ctrl-C
    signal (SIGINT, siginthand);

    uint16_t writeval = 0;
    while (true) {

        // process each MCP23017
        for (int i = 0; i < 4; i ++) {

            // select an MCP23017
            uint8_t n = ns[i];
            uint8_t addr = I2CBA + n;

            // read what is in the MCP23017 OLAT registers
            uint64_t cmdrd =
                (ZYNQWR << 62) | ((uint64_t) addr << 55) | (I2CWR << 54) |      // send address, write bit
                (ZYNQWR << 52) | ((uint64_t) OLATA << 44) |                     // send register number
                (ZYNQST << 42) |                                                // send restart bit
                (ZYNQWR << 40) | ((uint64_t) addr << 33) | (I2CRD << 32) |      // send address, read bit
                (ZYNQRD << 30) | (ZYNQRD << 28);                                // receive 2 bytes

            printf ("    %u read=", n);

            uint64_t stsrd = doi2ccycle (cmdrd);
            if (stsrd == (uint64_t) -1LL) printf ("fail");
            else printf ("%04X", (unsigned) stsrd & 0xFFFFU);

            // write new value to MCP23017 OLAT registers
            ++ writeval;
            uint64_t cmdwr =
                (ZYNQWR << 62) | ((uint64_t) addr << 55) | (I2CWR << 54) |      // send address, write bit
                (ZYNQWR << 52) | ((uint64_t) OLATA << 44) |                     // send register number
                (ZYNQWR << 42) | ((uint64_t) (writeval >> 8) << 34) |           // send high order data
                (ZYNQWR << 32) | ((uint64_t) (writeval & 255) << 24);           // send low order data

            printf ("   write=%04X=", writeval);

            uint64_t stswr = doi2ccycle (cmdwr);
            if (stswr == (uint64_t) -1LL) printf ("fail");
            else printf ("good");

            usleep (1000);
        }

        ++ writeval;

        printf ("\n");
    }
}

// read 8-bit value from mcp23017 reg at addr
uint8_t read8 (uint8_t addr, uint8_t reg)
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

    uint64_t sts = doi2ccycle (cmd);

    // value in low 8 bits of status register
    return (uint8_t) sts;
}

// write 8-bit value to mcp23017 reg at addr
void write8 (uint8_t addr, uint8_t reg, uint8_t byte)
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
uint16_t read16 (uint8_t addr, uint8_t reg)
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

    uint64_t sts = doi2ccycle (cmd);

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
void write16 (uint8_t addr, uint8_t reg, uint16_t word)
{
    // build 64-bit command
    //  send-8-bits-to-i2cbus 7-bit-address 0=write
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

int ntherror = 5;
uint64_t previla[ILADEPTH];

// send command to MCP23017 via pdp8lfpi2c.v then read response
uint64_t doi2ccycle (uint64_t cmd)
{
    if (aborted) {
        fflush (stdout);
        fprintf (stderr, "\ndoi2ccycle: aborted\n");
        exit (0);
    }
#if 000///111
    fpat[5] = SCLK | SDAO | MANUAL | ZCLEAR;
    bitdelay ();
    fpat[5] = SCLK | MANUAL | ZCLEAR;
    bitdelay ();
    fpat[5] = MANUAL | ZCLEAR;

    uint64_t sts = 0;
    while (true) {
        switch (cmd >> 62) {

            // send stop bit and return status
            // starts with SCLK=0, SDAO=0
            // wait 5uS, set SCLK
            // wait 5uS, set SDAT
            case 0: {
                bitdelay ();
                fpat[5] = SCLK | MANUAL | ZCLEAR;
                bitdelay ();
                fpat[5] = SCLK | SDAO | MANUAL | ZCLEAR;
                return sts;
            }

            // send a restart bit
            // starts with SCLK=0, SDAO=0
            // wait 5uS, set SDAO
            // wait 5uS, set SCLK
            // wait 5uS, clear SDAO
            // wait 5uS, clear SCLK
            case ZYNQST: {
                bitdelay ();
                fpat[5] = SDAO | MANUAL | ZCLEAR;
                bitdelay ();
                fpat[5] = SCLK | SDAO | MANUAL | ZCLEAR;
                bitdelay ();
                fpat[5] = SCLK | MANUAL | ZCLEAR;
                bitdelay ();
                fpat[5] = MANUAL | ZCLEAR;
                break;
            }

            // read 8 bits then send ack bit
            // starts with SCLK=0, SDAO=0
            // wait 5uS, set SDAT to open-drain the data line
            // repeat 8 times:
            //   wait 5uS, set SCLK
            //   wait 5uS, shift SDAI into sts, clear SCLK
            // wait 5uS, clear SDAT to send ACK bit
            // wait 5uS, set SCLK
            // wait 5uS, clear SCLK
            // ends with SCLK=0, SDAO=0
            case ZYNQRD: {
                bitdelay ();
                fpat[5] = SDAO | MANUAL | ZCLEAR;
                for (int i = 0; i < 8; i ++) {
                    bitdelay ();
                    fpat[5] = SCLK | SDAO | MANUAL | ZCLEAR;
                    bitdelay ();
                    sts = (sts << 1) | ((fpat[5] / SDAI) & 1);
                    fpat[5] = SDAO | MANUAL | ZCLEAR;
                }
                bitdelay ();
                fpat[5] = MANUAL | ZCLEAR;
                bitdelay ();
                fpat[5] = SCLK | MANUAL | ZCLEAR;
                bitdelay ();
                fpat[5] = MANUAL | ZCLEAR;
                break;
            }

            // write 8 bits then read ack bit
            // starts with SCLK=0, SDAO=0
            // repeat 8 times:
            //   wait 5uS, set SDAO to top data bit
            //   wait 5uS, set SCLK
            //   wait 5uS, clear SCLK
            // wait 5uS, set SDAO to recv ACK bit
            // wait 5uS, set SCLK
            // wait 5uS, check SDAI is low, clear SCLK
            // wait 5uS, clear SDAO
            // ends with SCLK=0, SDAO=0
            case ZYNQWR: {
                for (int i = 0; i < 8; i ++) {
                    uint32_t x = ((cmd >> 61) & 1) * SDAO | MANUAL | ZCLEAR;
                    bitdelay ();
                    fpat[5] = x;
                    bitdelay ();
                    fpat[5] = SCLK | x;
                    bitdelay ();
                    fpat[5] = x;
                    cmd <<= 1;
                }
                bitdelay ();
                fpat[5] = SDAO | MANUAL | ZCLEAR;
                bitdelay ();
                fpat[5] = SCLK | SDAO | MANUAL | ZCLEAR;
                bitdelay ();
                if (fpat[5] & SDAI) {
                    return (uint64_t) -1LL;
                }
                fpat[5] = SDAO | MANUAL | ZCLEAR;
                bitdelay ();
                fpat[5] = MANUAL | ZCLEAR;
                break;
            }

            default: ABORT ();
        }
        cmd <<= 2;
    }
#endif
#if 111///000

    ilaarm ();

    // writing high-order word triggers i/o, so write low-order first
    fpat[ZPI2CCMD+0] = cmd;
    fpat[ZPI2CCMD+1] = cmd >> 32;

    // wait for busy bit to clear
    for (int i = 1000000; -- i >= 0;) {
        uint32_t sts1 = fpat[ZPI2CSTS+1];
        if (! (sts1 & (1U << 31))) {

            // check for errors
            //  [30] = ack was not received
            //  [29] = data line stuck low during stop
            if (sts1 & (3U << 29)) {
                if (-- ntherror <= 0) {
                    printf ("\ndumping prev ila\n");
                    iladump (previla);
                    printf ("\ndumping this ila\n");
                    ilasave (previla);
                    iladump (previla);
                    exit (0);
                }
                return (uint64_t) -1LL;
            }

            uint32_t sts0 = fpat[ZPI2CSTS+0];
            ilasave (previla);
            return (((uint64_t) sts1) << 32) | sts0;
        }
    }
    fprintf (stderr, "doi2ccycle: i2c I/O timeout\n");
    ABORT ();
#endif
}

void ilaarm ()
{
    // tell zynq.v to start collecting samples
    // tell it to stop when collected trigger sample plus ILAAFTER thereafter
    pdpat[ILACTL] = ILACTL_ARMED | ILAAFTER * ILACTL_ILAAFTER0;
}

void ilasave (uint64_t *save)
{
    // wait for sampling to stop
    uint32_t ctl;
    while (((ctl = pdpat[ILACTL]) & (ILACTL_ARMED | ILACTL_ILAAFTER)) != 0) usleep (10000);

    // copy oldest to newest entries
    // ctl<index>+0 = next entry to be overwritten = oldest entry
    for (int i = 0; i < ILADEPTH; i ++) {
        pdpat[ILACTL] = (ctl + i * ILACTL_INDEX0) & ILACTL_INDEX;
        save[i] = (((uint64_t) pdpat[ILADAT+1]) << 32) | (uint64_t) pdpat[ILADAT+0];
    }
}

void iladump (uint64_t const *save)
{
    uint64_t thisentry = *(save ++);

    // loop through all entries in the array
    bool indotdotdot = false;
    uint64_t preventry = 0;
    for (int i = 0; i < ILADEPTH; i ++) {

        // read array entry after thisentry
        uint64_t nextentry = (i == ILADEPTH - 1) ? 0 : *(save ++);

        // print thisentry - but use ... if same as prev and next
        if ((i == 0) || (i == ILADEPTH - 1) || (thisentry != preventry) || (thisentry != nextentry)) {
            printf ("%6.2f  %o  %o  %o %o  %o %o\n",
                (i - ILADEPTH + ILAAFTER + 1) / 100.0,  // trigger shows as 0.00uS
                (unsigned) (thisentry >>  5) & 1,       // fpi2cwrite
                (unsigned) (thisentry >>  4) & 1,       // fpi2cdao
                (unsigned) (thisentry >>  3) & 1,       // iFPI2CCLK
                (unsigned) (thisentry >>  2) & 1,       // bFPI2CDATA
                (unsigned) (thisentry >>  1) & 1,       // i_FPI2CDENA
                (unsigned) (thisentry >>  0) & 1);      // iFPI2CDDIR

            indotdotdot = false;
        } else if (! indotdotdot) {
            printf ("    ...\n");
            indotdotdot = true;
        }

        // shuffle entries for next time through
        preventry = thisentry;
        thisentry = nextentry;
    }
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
