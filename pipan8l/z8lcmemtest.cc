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

// Test the pdp8lcmem.v module
// It gives access to the processor's internal 4K core memory via dma cycles
// The test writes random numbers to the 4K and reads them back and verifies
// It also tests the same interface with the extended 28K memory

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "z8ldefs.h"
#include "z8lutil.h"

#define DOTJMPDOT 05252U

static bool volatile exitflag;
static uint16_t idwc, idca;
static uint16_t shadow[32768];
static uint32_t volatile *cmemat;
static uint32_t volatile *extmem;
static uint32_t volatile *pdpat;
static uint32_t volatile *xmemat;

static void siginthand (int signum);
static void writemem (uint16_t xaddr, uint16_t wdata, bool do3cyc, bool cainc);
static uint16_t readmem (uint16_t xaddr, bool do3cyc, bool cainc);
static void waitcmemidle ();
static bool getcmemwcovf ();
static uint16_t waitcmemread ();
static void clockit (int n);



int main (int argc, char **argv)
{
    setlinebuf (stdout);

    bool testxmem = false;
    bool test3cyc = false;
    bool testcain = false;
    for (int i = 0; ++ i < argc;) {
        if (strcmp (argv[i], "-?") == 0) {
            puts ("");
            puts ("    Test core memory and DMA circuitry");
            puts ("");
            puts ("  ./z8lcmemtest.armv7l [-3cycle] [-cainc] [-extmem]");
            puts ("    -3cycle : occasionally test 3-cycle DMAs");
            puts ("     -cainc : occasionally test ca-increment 3-cycle DMAs");
            puts ("    -extmem : test FPGA-provided extended memory for upper 28K (otherwise just test low 4K)");
            puts ("");
            return 0;
        }
        if (strcasecmp (argv[i], "-3cycle") == 0) {
            test3cyc = true;
            continue;
        }
        if (strcasecmp (argv[i], "-cainc") == 0) {
            testcain = true;
            continue;
        }
        if (strcasecmp (argv[i], "-extmem") == 0) {
            testxmem = true;
            continue;
        }
        fprintf (stderr, "unknown argument %s\n", argv[i]);
        return 1;
    }

    Z8LPage z8p;
    pdpat  = z8p.findev ("8L", NULL, NULL, true);
    cmemat = z8p.findev ("CM", NULL, NULL, true);
    xmemat = z8p.findev ("XM", NULL, NULL, true);
    extmem = z8p.extmem ();

    printf ("8L VERSION=%08X\n", pdpat[0]);
    printf ("CM VERSION=%08X\n", cmemat[0]);
    printf ("XM VERSION=%08X\n", xmemat[0]);

    cmemat[2] = cmemat[2] & CM2_NOBRK;

    // see if we are simulating
    bool simulate = (pdpat[Z_RE] & e_simit) != 0;
    pdpat[Z_RE] = (pdpat[Z_RE] & e_simit) | e_nanocontin;

    // range of addresses to test
    uint16_t startat = 000000;
    uint16_t stopat  = testxmem ? 077777 : 007777;

    // a real PDP needs to be running so we can do DMA
    // tell user to put a JMP . at 5252 and start it

    if (simulate) {
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0;
        clockit (1000);
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0 | b_swLDAD;
        clockit (1000);
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0;
        clockit (1000);
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0 | b_swDEP;
        clockit (1000);
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0;
        clockit (1000);
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0 | b_swLDAD;
        clockit (1000);
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0;
        clockit (1000);
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0 | b_swSTART;
        clockit (1000);
        pdpat[Z_RB] = DOTJMPDOT * b_swSR0;
        clockit (1000);
    } else {
        printf ("\n");
        printf ("  set sr to %04o\n", DOTJMPDOT);
        printf ("  set all other switches to 0\n");
        printf ("  load address\n");
        printf ("  deposit\n");
        printf ("  load address\n");
        printf ("  start\n");
        printf ("> ");
        fflush (stdout);
        char temp[8];
        if (fgets (temp, sizeof temp, stdin) == NULL) {
            printf ("\n");
            return 0;
        }
    }

    signal (SIGINT, siginthand);

    printf ("\n");

    uint32_t errors = 0;
    uint32_t pass = 0;
    shadow[DOTJMPDOT] = DOTJMPDOT;
    while (true) {

        ++ pass;

        // pick random location in first 4K for wordcount/currentaddress locations
        // don't overlap DOTJMPDOT
        if (test3cyc) {
            do {
                idwc = randbits (12);
                idca = (idwc + 1) & 07777;
            } while ((idwc == DOTJMPDOT) || (idca == DOTJMPDOT));
            printf ("idwc=%04o idca=%04o\n", idwc, idca);
        } else {
            idwc = 0xFFFFU;
            idca = 0xFFFFU;
        }

        // write memory using cmem interface (break cycles)
        for (int i = startat; i <= stopat; i ++) {
            if (exitflag) goto done;

            // don't overwrite the DOTJMPDOT instruction
            if (i == DOTJMPDOT) continue;
            if (i == DOTJMPDOT+1) continue;

            // don't work on the idwc/idca words
            if (i == idwc) continue;
            if (i == idca) continue;

            // get a random number and tell cmem interface to write it to memory via dma cycle
            bool do3cycls = test3cyc && randbits (1);
            bool docaincr = do3cycls && testcain && randbits (1);
            uint16_t randata;
            do randata = randbits (12);
            while (randata == DOTJMPDOT);
            writemem (i, randata, do3cycls, docaincr);
        }

        // finish clocking write to 077777 (stopat) so readback via extmem[] will work
        waitcmemidle ();

        // readback memory using extmem interface (direct mapped memory)
        // if XM_ENLO4K, we are using FPGA-provided externmem for low 4K so we can check it here
        // otherwise, we are using PDP-provided core memory for low 4K which we can't access directly
        for (int i = (xmemat[1] & XM_ENLO4K) ? startat : 010000; i <= stopat; i ++) {
            if (i == DOTJMPDOT+1) continue;
            uint16_t wdata = shadow[i];
            uint16_t rdata = extmem[i];
            if (rdata != wdata) {
                printf ("extmem error %05o was %04o should be %04o\n", i, rdata, wdata);
                errors ++;
                ABORT ();
            }
        }

        // readback memory using cmem interface (break cycles) which can access all memory
        for (int i = startat; i <= stopat; i ++) {
            if (exitflag) goto done;

            if (i == DOTJMPDOT) continue;
            if (i == DOTJMPDOT+1) continue;

            // don't work on the idwc/idca words
            if (i == idwc) continue;
            if (i == idca) continue;

            // read memory via DMA cycle
            bool do3cycls = test3cyc && randbits (1);
            bool docaincr = do3cycls && testcain && randbits (1);
            uint16_t rdata = readmem (i, do3cycls, docaincr);

            // verify value read vs value written
            uint16_t wdata = shadow[i];
            if (rdata != wdata) {
                printf ("cmem error %05o was %04o should be %04o\n", i, rdata, wdata);
                errors ++;
                ABORT ();
            }
        }

        printf ("\npass %u complete %u errors\n\n", pass, errors);
    }
done:;
    putchar ('\n');
    return 0;
}

static void siginthand (int signum)
{
    exitflag = true;
}



// write data to memory via DMA cycle
//  input:
//   xaddr = 15-bit address to write to
//   wdata = 12-bit data to write to that location
//   do3cyc = false: do 1-cycle DMA
//             true: do 3-cycle DMA
//   cainc = ignored if not d3cyc
//           else false: use ca pointer directly
//                 true: start with ca pointer decremented so it supposedly gets incremented to correct value
//  output:
//   value written to memory
//   value written to shadow[]
static void writemem (uint16_t xaddr, uint16_t wdata, bool do3cyc, bool cainc)
{
    ASSERT (xaddr != DOTJMPDOT);

    // maybe use 3-cycle dma to write the location
    if (do3cyc) {

        // pick a random value for wordcount, it always gets incremented
        uint16_t oldwordcount = randbits (6);
        if (oldwordcount & 00040) oldwordcount |= 07700;
        uint16_t newwordcount = (oldwordcount + 1) & 07777;
        writemem (idwc, oldwordcount, false, false);

        // use the called-with address for the currentaddress value
        // decrement it if we are going to tell processor to increment it
        uint16_t oldcurraddrs = (xaddr - cainc) & 07777;
        uint16_t newcurraddrs =  xaddr          & 07777;
        writemem (idca, oldcurraddrs, false, false);

        // tell pdp8lcmem.v to start writing the value to memory
        shadow[idwc] = newwordcount;                        // 3-cycle DMA writes these values to memory
        shadow[idca] = newcurraddrs;
        shadow[xaddr] = wdata;                              // ...as well as writing the data word
        waitcmemidle ();
        cmemat[2] = (cmemat[2] & CM2_NOBRK) | CM2_3CYCL | (cainc ? CM2_CAINC : 0);  // select 3-cycle DMA, possibly with CA increment
        cmemat[1] = CM_ENAB | (wdata * CM_DATA0) | CM_WRITE | ((xaddr & 070000) + idwc) * CM_ADDR0;

        // get wordcount overflow status
        bool wcovf = getcmemwcovf ();

        // verify the possibly updated idwc and idca values
        uint16_t finalwc = readmem (idwc, false, false);
        if (finalwc != newwordcount) {
            printf ("idwc %04o / %04o => %04o sb %04o\n", idwc, oldwordcount, finalwc, newwordcount);
            ABORT ();
        }
        if ((newwordcount == 0) ^ wcovf) {
            printf ("idwc %04o / %04o => %04o got overflow %d\n", idwc, oldwordcount, newwordcount, wcovf);
            ABORT ();
        }
        uint16_t finalca = readmem (idca, false, false);
        if (finalca != newcurraddrs) {
            printf ("idca %04o / %04o => %04o sb %04o\n", idca, oldcurraddrs, finalca, newcurraddrs);
            ABORT ();
        }
    }

    // tell cmem interface to start writing data to memory via dma cycle
    else {
        shadow[xaddr] = wdata;
        waitcmemidle ();
        cmemat[2] = cmemat[2] & CM2_NOBRK;
        cmemat[1] = CM_ENAB | wdata * CM_DATA0 | CM_WRITE | xaddr * CM_ADDR0;
        getcmemwcovf ();
    }
}

// read data from memory via DMA cycle
//  input:
//   xaddr = 15-bit address to write to
//   do3cyc = false: do 1-cycle DMA
//             true: do 3-cycle DMA
//   cainc = ignored if not d3cyc
//           else false: use ca pointer directly
//                 true: start with ca pointer decremented so it supposedly gets incremented to correct value
//  output:
//   returns value read from memory
static uint16_t readmem (uint16_t xaddr, bool do3cyc, bool cainc)
{
    uint16_t rdata;

    // maybe use 3-cycle dma to write the location
    if (do3cyc) {

        // pick a random value for wordcount, it always gets incremented
        uint16_t oldwordcount = randbits (6);
        if (oldwordcount & 00040) oldwordcount |= 07700;
        uint16_t newwordcount = (oldwordcount + 1) & 07777;
        writemem (idwc, oldwordcount, false, false);

        // use the called-with address for the currentaddress value
        // decrement it if we are going to tell processor to increment it
        uint16_t oldcurraddrs = (xaddr - cainc) & 07777;
        uint16_t newcurraddrs =  xaddr          & 07777;
        writemem (idca, oldcurraddrs, false, false);

        // tell pdp8lcmem.v to start reading the value from memory
        shadow[idwc] = newwordcount;                        // 3-cycle DMA writes these values to memory
        shadow[idca] = newcurraddrs;
        waitcmemidle ();
        cmemat[2] = (cmemat[2] & CM2_NOBRK) | CM2_3CYCL | (cainc ? CM2_CAINC : 0);  // select 3-cycle DMA, possibly with CA increment
        cmemat[1] = CM_ENAB | ((xaddr & 070000) + idwc) * CM_ADDR0;

        // get wordcount overflow status
        bool wcovf = getcmemwcovf ();

        // wait for it to arrive and retrieve it
        rdata = waitcmemread ();

        // verify the possibly updated idwc and idca values
        uint16_t finalwc = readmem (idwc, false, false);
        if (finalwc != newwordcount) {
            printf ("idwc %04o / %04o => %04o sb %04o\n", idwc, oldwordcount, finalwc, newwordcount);
            ABORT ();
        }
        if ((newwordcount == 0) ^ wcovf) {
            printf ("idwc %04o / %04o => %04o got overflow %d\n", idwc, oldwordcount, newwordcount, wcovf);
            ABORT ();
        }
        uint16_t finalca = readmem (idca, false, false);
        if (finalca != newcurraddrs) {
            printf ("idca %04o / %04o => %04o sb %04o\n", idca, oldcurraddrs, finalca, newcurraddrs);
            ABORT ();
        }
    }

    // tell cmem interface to start reading data from memory via dma cycle
    // then wait for it to arrive and retrieve it
    else {
        waitcmemidle ();
        cmemat[2] = cmemat[2] & CM2_NOBRK;
        cmemat[1] = CM_ENAB | xaddr * CM_ADDR0;
        rdata = waitcmemread ();
    }

    return rdata;
}

// wait for pdp8lcmem.v interface able to accept new command
static void waitcmemidle ()
{
    for (int j = 0; cmemat[1] & CM_BUSY; j ++) {
        clockit (1);
        if (j > 10000) {
            fprintf (stderr, "timed out waiting for cmem ready\n");
            ABORT ();
        }
    }
}

// wait for pdp8lcmem.v wordcount overflow status
static bool getcmemwcovf ()
{
    uint32_t cm1;
    for (int j = 0; ! ((cm1 = cmemat[1]) & CM_DONE); j ++) {
        clockit (1);
        if (j > 10000) {
            fprintf (stderr, "timed out waiting for cmem done\n");
            ABORT ();
        }
    }
    return (cm1 & CM_WCOVF) != 0;
}

// wait for pdp8lcmem.v read data ready
static uint16_t waitcmemread ()
{
    uint32_t cm1;
    for (int j = 0; ! ((cm1 = cmemat[1]) & CM_DONE); j ++) {
        clockit (1);
        if (j > 10000) {
            fprintf (stderr, "timed out waiting for cmem ready\n");
            ABORT ();
        }
    }
    return (cm1 & CM_DATA) / CM_DATA0;
}

// gate through at least n fpga clock cycles
static void clockit (int n)
{
    usleep (n / 100 + 1);
}
