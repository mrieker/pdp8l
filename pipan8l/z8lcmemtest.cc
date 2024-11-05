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

//  ./z8lcmemtest.armv7l

#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "z8ldefs.h"
#include "z8lutil.h"

static bool brkwhenhltd = true;
static bool externlow4K = true;
static bool manualclock = true;

static bool volatile exitflag;
static uint32_t volatile *pdpat;

static void ldad (uint16_t addr);
static void depos (uint16_t data);
static void clockit (int n);

static void siginthand (int signum)
{
    exitflag = true;
}

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    Z8LPage z8p;
    pdpat = z8p.findev ("8L", NULL, NULL, true);
    if (pdpat == NULL) {
        fprintf (stderr, "pdp-8/l not found\n");
        return 1;
    }
    printf ("8L VERSION=%08X\n", pdpat[0]);

    uint32_t volatile *xmemat = z8p.findev ("XM", NULL, NULL, true);
    if (xmemat == NULL) {
        fprintf (stderr, "xmem not found\n");
        return 1;
    }
    printf ("XM VERSION=%08X\n", xmemat[0]);

    uint32_t volatile *cmemat = z8p.findev ("CM", NULL, NULL, true);
    if (cmemat == NULL) {
        fprintf (stderr, "cmem not found\n");
        return 1;
    }
    printf ("CM VERSION=%08X\n", cmemat[0]);

    uint32_t volatile *extmem = z8p.extmem ();

    // select simulator and reset it
    pdpat[Z_RA] = ZZ_RA;
    pdpat[Z_RB] = 0;
    pdpat[Z_RC] = 0;
    pdpat[Z_RD] = ZZ_RD;
    uint32_t re = e_simit;
    if (! manualclock) re |= e_nanocontin;
    if (brkwhenhltd) re |= e_brkwhenhltd;
    pdpat[Z_RE] = re | e_softreset;
    clockit (1000);
    pdpat[Z_RE] = re;
    clockit (1000);

    // externlow4K = true: use extmem fpga block memory for all 32K words
    //              false: extmem fpga block memory is the upper 28K words
    //                     sim: pdp8lsim.v localcore array is lower 4K
    //                    real: PDP-8/L real core stack is lower 4K
    xmemat[1] = externlow4K ? XM_ENLO4K : 0;
    clockit (1000);

    // range of addresses to test
    uint16_t startat = brkwhenhltd ? 00000 : 000004;
    uint16_t stopat  = 077777;

    if (! brkwhenhltd) {

        ldad (0); depos (02003);    //  ISZ  3
        ldad (1); depos (05000);    //  JMP  0
        ldad (2); depos (05000);    //  JMP  0
        ldad (3); depos (0);        // .WORD 0

        // do LOAD ADDRESS with zero in switch register
        ldad (0);

        // do START to get processor running so it will do break cycles
        pdpat[Z_RB] = b_swSTART;
        clockit (200000);
        pdpat[Z_RB] = 0;
        clockit (1000);
    }

    signal (SIGINT, siginthand);

    uint32_t errors = 0;
    uint32_t pass = 0;
    uint16_t shadow[32768];
    while (true) {
        ++ pass;

        // write memory using cmem interface (break cycles)
        for (int i = startat; i <= stopat; i ++) {
            uint16_t wdata = randbits (12);
            shadow[i] = wdata;
            for (int j = 0; cmemat[1] & CM_BUSY; j ++) {
                clockit (1);
                if (j > 1000000) {
                    printf ("timed out waiting for write done at %05o\n", i);
                    if (! brkwhenhltd && (pdpat[Z_RF] & f_o_B_RUN)) goto halted;
                    errors ++;
                    break;
                }
            }
            if (exitflag) goto done;
            cmemat[1] = CM_ENAB | (wdata * CM_DATA0) | CM_WRITE | (i * CM_ADDR0);
        }

        // finish clocking write to 077777 (stopat) so readback via extmem[] will work
        for (int j = 0; cmemat[1] & CM_BUSY; j ++) {
            clockit (1);
        }

        // readback memory using exemem interface (direct mapped memory)
        for (int i = startat; i <= stopat; i ++) {
            if (externlow4K || (i >= 010000)) {
                uint16_t wdata = shadow[i];
                uint16_t rdata = extmem[i];
                if (rdata != wdata) {
                    printf ("extmem error %05o was %04o should be %04o\n", i, rdata, wdata);
                    errors ++;
                }
            }
        }

        // readback memory using cmem interface (break cycles)
        for (int i = stopat; i < stopat; i ++) {
            for (int j = 0; cmemat[1] & CM_BUSY; j ++) {
                clockit (1);
                if (j > 1000000) {
                    printf ("timed out waiting for read done at %05o\n", i);
                    if (! brkwhenhltd && (pdpat[Z_RF] & f_o_B_RUN)) goto halted;
                    errors ++;
                    break;
                }
            }
            if (exitflag) goto done;
            cmemat[1] = CM_ENAB | i * CM_ADDR0;
            for (int j = 0; ! (cmemat[1] & CM_RRDY); j ++) {
                clockit (1);
                if (j > 1000000) {
                    printf ("timed out waiting for read ready at %05o\n", i);
                    if (! brkwhenhltd && (pdpat[Z_RF] & f_o_B_RUN)) goto halted;
                    errors ++;
                    break;
                }
            }
            uint16_t wdata = shadow[i];
            uint16_t rdata = (cmemat[1] & CM_DATA) / CM_DATA0;
            if (rdata != wdata) {
                printf ("cmem error %05o was %04o should be %04o\n", i, rdata, wdata);
                errors ++;
            }
        }

        printf ("\npass %u complete  %u errors\n\n", pass, errors);
    }
done:;
    putchar ('\n');
    return 0;

halted:;
    printf ("processor halted > ");
    fflush (stdout);
    char temp[8];
    return (fgets (temp, sizeof temp, stdin) == NULL);
}

// do LOAD ADDRESS with zero in switch register
static void ldad (uint16_t addr)
{
    pdpat[Z_RB] = addr * b_swSR0 | b_swLDAD;
    clockit (200000);
    pdpat[Z_RB] = addr * b_swSR0;
    clockit (1000);
}

// do DEPOSIT with 5000 in switch register (JMP 0)
static void depos (uint16_t data)
{
    pdpat[Z_RB] = data * b_swSR0 | b_swDEP;
    clockit (200000);
    pdpat[Z_RB] = data * b_swSR0;
    clockit (1000);
}

// gate through one fpga clock cycle
static void clockit (int n)
{
    if (manualclock) {
        while (-- n >= 0) pdpat[Z_RE] |= e_nanotrigger;
    } else {
        usleep (n);
    }
}
