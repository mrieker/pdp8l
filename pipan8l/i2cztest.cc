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
// Uses pdp8lfpi2c.v and i2cmaster.v instead of /dev/i2c-0 or 1
// Setup:
//  zturn36 board with 3-wire i2c cable connected to pipan8l/pcb3 board
//  neither needs to be plugged into pdp

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

static uint32_t volatile *pdpat;
static uint32_t volatile *fpat;

uint64_t doi2ccycle (uint64_t cmd);

int main ()
{
    setlinebuf (stdout);

    Z8LPage *z8p = new Z8LPage ();
    pdpat = z8p->findev ("8L", NULL, NULL, false);
    fpat  = z8p->findev ("FP", NULL, NULL, false);

    printf ("8L VERSION %08X\n", pdpat[0]);
    printf ("FP VERSION %08X\n", fpat[0]);

    // reset the I2C code (ZCLEAR)
    pdpat[Z_RE] = e_simit | e_nanocontin;
    fpat[5] = ZCLEAR;
    usleep (1000);
    fpat[5] = 0;

    // clock and data lines shoule be high (open)
    for (int i = 0; i < 100; i ++) {
        fpat[5] = SDAO | SCLK | MANUAL;
        usleep (1000);
        if (fpat[5] & SDAI) goto datok;
        fprintf (stderr, "data line stuck low\n");
        fpat[5] = SDAO | MANUAL;
        usleep (1000);
    }
    fprintf (stderr, "failed to clear data line\n");
    return 1;
datok:;
    fpat[5] = 0;

    // order of addressing MCP23017s
    uint16_t ns[4] = { 0, 1, 2, 3 };

    uint16_t writeval = 0;
    while (true) {

        // write each MCP23017 OLAT registers
        printf ("    write");
        for (int i = 0; i < 4; i ++) {

            // select an MCP23017
            uint8_t n = ns[i];
            uint8_t addr = I2CBA + n;

            // write new value to MCP23017 OLAT registers
            ++ writeval;
            uint64_t cmdwr =
                (ZYNQWR << 62) | ((uint64_t) addr << 55) | (I2CWR << 54) |      // send address, write bit
                (ZYNQWR << 52) | ((uint64_t) OLATA << 44) |                     // send register number
                (ZYNQWR << 42) | ((uint64_t) (writeval >> 8) << 34) |           // send high order data
                (ZYNQWR << 32) | ((uint64_t) (writeval & 255) << 24);           // send low order data

            printf ("  [%u]=%04X=", n, writeval);

            uint64_t stswr = doi2ccycle (cmdwr);
            if (stswr == (uint64_t) -1LL) printf ("fail");
            else printf ("good");
        }

        ++ writeval;

        // read each MCP23017 OLAT registers
        printf ("  read");
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

            printf ("  [%u]=", n);

            uint64_t stsrd = doi2ccycle (cmdrd);
            if (stsrd == (uint64_t) -1LL) printf ("fail");
            else printf ("%04X", (unsigned) stsrd & 0xFFFFU);
        }

        printf ("\n");
    }
}

// send command to MCP23017 via pdp8lfpi2c.v and i2cmaster.v then read response
uint64_t doi2ccycle (uint64_t cmd)
{
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
                return (uint64_t) -1LL;
            }

            uint32_t sts0 = fpat[ZPI2CSTS+0];
            return (((uint64_t) sts1) << 32) | sts0;
        }
    }
    fprintf (stderr, "doi2ccycle: i2c I/O timeout\n");
    ABORT ();
}
