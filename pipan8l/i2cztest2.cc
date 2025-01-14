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
// Setup:
//  zturn36 board with zynq+5<->pdp8+5 jumper, not plugged into pdp, powered by usb
//  zturn35 board connected to zturn36 board and with +5 hooked up
//  i2c 3-wire connected to pipan8l/pcb3 board not plugged into pdp, with +5 hooked up

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "i2czlib.h"
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

#define ILADEPTH 4096   // total number of elements in ilaarray
#define ILAAFTER 4000   // number of samples to take after sample containing trigger

#define ILACTL 021
#define ILADAT 022

#define ILACTL_ARMED  0x80000000U
#define ILACTL_AFTER0 0x00010000U
#define ILACTL_INDEX0 0x00000001U
#define ILACTL_AFTER  (ILACTL_AFTER0 * (ILADEPTH - 1))
#define ILACTL_INDEX  (ILACTL_INDEX0 * (ILADEPTH - 1))

uint64_t previla[ILADEPTH];

static bool volatile aborted;
static uint32_t volatile *pdpat;
static uint32_t volatile *fpat;

void resetit ();
void write16 (uint8_t addr, uint8_t reg, uint16_t val);
uint16_t read16 (uint8_t addr, uint8_t reg);
uint64_t doi2ccycle (uint64_t cmd);
void ilaarm ();
void ilasave (uint64_t *save);
void iladump (uint64_t const *save);

int main ()
{
    setlinebuf (stdout);

    Z8LPage *z8p = new Z8LPage ();
    pdpat = z8p->findev ("8L", NULL, NULL, false);
    fpat  = z8p->findev ("FP", NULL, NULL, false);

    printf ("8L VERSION %08X\n", pdpat[0]);
    printf ("FP VERSION %08X\n", fpat[0]);

    // reset the I2C code (ZCLEAR)
    resetit ();

    // set all pins to be inputs
    write16 (I2CBA + 0, IODIRA, 0xFFFFU);
    write16 (I2CBA + 1, IODIRA, 0xFFFFU);
    write16 (I2CBA + 2, IODIRA, 0xFFFFU);
    write16 (I2CBA + 3, IODIRA, 0xFFFFU);

    while (true) {

        // read directions and values
        uint16_t dirs[4], vals[4];
        for (int i = 0; i < 4; i ++) {
            dirs[i] = 0xFFFFU; //// read16 (I2CBA + i, IODIRA);
            vals[i] = read16 (I2CBA + i, GPIOA);
        }

        Z8LPanel padsbuf;
        Z8LPanel *pads = &padsbuf;

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


        printf ("ir=%o link=%o fet=%o ion=%o pare=%o exe=%o def=%o prte=%o wct=%o cad=%o  stop=%o cont=%o start=%o ldad=%o exam=%o dep=%o  sr=%04o mprt=%o dfld=%o ifld=%o step=%o\n",
            pads->light.ir, pads->light.link, pads->light.fet, pads->light.ion, pads->light.pare, pads->light.exe, pads->light.def, pads->light.prte,
                pads->light.wct, pads->light.cad,
            pads->button.stop, pads->button.cont, pads->button.start, pads->button.ldad, pads->button.exam, pads->button.dep,
            pads->togval.sr, pads->togval.mprt, pads->togval.dfld, pads->togval.ifld, pads->togval.step);
    }
}

void resetit ()
{
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
    ABORT ();
datok:;
    fpat[5] = 0;
}

void write16 (uint8_t addr, uint8_t reg, uint16_t val)
{
    uint64_t cmdwr =
        (ZYNQWR << 62) | ((uint64_t) addr << 55) | (I2CWR << 54) |      // send address, write bit
        (ZYNQWR << 52) | ((uint64_t) OLATA << 44) |                     // send register number
        (ZYNQWR << 42) | ((uint64_t) (val & 0xFF) << 34) |              // send low order data
        (ZYNQWR << 32) | ((uint64_t) (val >> 8) << 24);                 // send high order data

    doi2ccycle (cmdwr);
}

uint16_t read16 (uint8_t addr, uint8_t reg)
{
    uint64_t cmdrd =
        (ZYNQWR << 62) | ((uint64_t) addr << 55) | (I2CWR << 54) |      // send address, write bit
        (ZYNQWR << 52) | ((uint64_t) reg  << 44) |                      // send register number
        (ZYNQST << 42) |                                                // send restart bit
        (ZYNQWR << 40) | ((uint64_t) addr << 33) | (I2CRD << 32) |      // send address, read bit
        (ZYNQRD << 30) | (ZYNQRD << 28);                                // receive 2 bytes

    uint16_t stsrd = doi2ccycle (cmdrd);

    return ((stsrd >> 8) & 0xFFU) | ((stsrd & 0xFFU) << 8);
}

// send command to MCP23017 via pdp8lfpi2c.v then read response
uint64_t doi2ccycle (uint64_t cmd)
{
    if (aborted) {
        fflush (stdout);
        fprintf (stderr, "\ndoi2ccycle: aborted\n");
        exit (0);
    }

    // writing high-order word triggers i/o, so write low-order first
    fpat[ZPI2CCMD+0] = cmd;
    fpat[ZPI2CCMD+1] = cmd >> 32;

    // wait for busy bit to clear
    for (int i = 1000000; -- i >= 0;) {
        uint32_t sts1 = fpat[ZPI2CSTS+1];
        if (! (sts1 & (1U << 31))) {

            uint32_t sts0 = fpat[ZPI2CSTS+0];

            // check for errors
            //  [30] = ack was not received
            //  [29] = data line stuck low during stop
            if (sts1 & (3U << 29)) {
                fprintf (stderr, "doi2ccycle: error %08X %08X\n", sts1, sts0);
                ABORT ();
            }

            return (((uint64_t) sts1) << 32) | sts0;
        }
    }
    fprintf (stderr, "doi2ccycle: i2c I/O timeout\n");
    ABORT ();
}

void ilaarm ()
{
    // tell zynq.v to start collecting samples
    // tell it to stop when collected trigger sample plus ILAAFTER thereafter
    pdpat[ILACTL] = ILACTL_ARMED | ILAAFTER * ILACTL_AFTER0;
}

void ilasave (uint64_t *save)
{
    // wait for sampling to stop
    uint32_t ctl;
    while (((ctl = pdpat[ILACTL]) & (ILACTL_ARMED | ILACTL_AFTER)) != 0) usleep (10000);

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
            uint64_t swapentry = (thisentry >> 32) | (thisentry << 32);
            printf ("%6.2f  %o  %o  %o %o  %03X  %08X\n",
                (i - ILADEPTH + ILAAFTER + 1) / 100.0,          // trigger shows as 0.00uS
                (unsigned) (swapentry >> 45) & 7,               // status[63:61]
                (unsigned) (swapentry >> 44) & 1,               // fpi2cwrite
                (unsigned) (swapentry >> 43) & 1,               // save_RVALID
                (unsigned) (swapentry >> 42) & 1,               // saxi_RREADY
                (unsigned) (swapentry >> 30) & 0xFFCU,          // readaddr
                (unsigned) (swapentry >>  0) & 0xFFFFFFFFU);    // saxi_RDATA

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
