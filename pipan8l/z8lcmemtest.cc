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
// - just writes random numbers to the 32K ram and reads them back and verifies

//  ./z8lcmemtest.armv7l

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "z8ldefs.h"
#include "z8lutil.h"

#define CM_ADDR  (077777U << 0)
#define CM_WRITE (1U << 15)
#define CM_DATA  (07777U << 16)
#define CM_BUSY  (1U << 28)
#define CM_RRDY  (1U << 29)
#define CM_ADDR0 (1U <<  0)
#define CM_DATA0 (1U << 16)
#define CM_ENAB  (1U << 31)

#define XM_ADDR   (077777U << 0)
#define XM_WRITE  (1U << 15)
#define XM_DATA   (07777U << 16)
#define XM_BUSY   (1U << 28)
#define XM_ENLO4K (1U << 30)
#define XM_ENABLE (1U << 31)
#define XM_ADDR0  (1U << 0)
#define XM_DATA0  (1U << 16)

static bool manualclock;
static uint32_t volatile *pdpat;

static void ldad (uint16_t addr);
static void depos (uint16_t data);
static void clockit ();

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    manualclock = false;

    // true: stay halted and do breaks while halted
    // false: have cpu do a 0: JMP 0 loop
    bool brkwhenhltd = false;

    // range of addresses to test
    uint16_t startat = 000004;
    uint16_t stopat  = 077777;

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

    // select simulator and reset it
    uint32_t ra = a_i_AC_CLEAR | a_i_BRK_RQST | a_i_EA | a_i_EMA | a_i_INT_INHIBIT |
        a_i_INT_RQST | a_i_IO_SKIP | a_i_MEMDONE | a_i_STROBE | a_simit | a_softreset;
    if (manualclock) ra |= a_nanocycle;
    if (brkwhenhltd) ra |= a_brkwhenhltd;
    pdpat[Z_RA]  = ra;
    pdpat[Z_RB]  = 0;
    pdpat[Z_RC]  = 0;
    pdpat[Z_RD]  = d_i_DMAADDR | d_i_DMADATA;
    pdpat[Z_RE]  = 0;
    for (int i = 0; i < 1000; i ++) clockit ();
    usleep (1000);
    pdpat[Z_RA] &= ~ a_softreset;
    for (int i = 0; i < 1000; i ++) clockit ();
    usleep (1000);

    // tell pdp8lxmem.v we want to use the core stack (sets enlo4K = 0)
    xmemat[0] = 0;
    for (int i = 0; i < 1000; i ++) clockit ();
    usleep (1000);

    if (! brkwhenhltd) {

        ldad (0); depos (02003);
        ldad (1); depos (05000);
        ldad (2); depos (05000);
        ldad (3); depos (0);

        // do LOAD ADDRESS with zero in switch register
        ldad (0);

        // do START to get processor running so it will do break cycles
        pdpat[Z_RB] = b_swSTART;
        for (int i = 0; i < 1000; i ++) clockit ();
        usleep (200000);
        pdpat[Z_RB] = 0;
        for (int i = 0; i < 1000; i ++) clockit ();
        usleep (1000);
    }

    uint32_t errors = 0;
    uint32_t pass = 0;
    uint16_t shadow[32768];
    while (true) {

        uint16_t wdata = ++ pass;
        for (int i = startat; i <= stopat; i ++) {
            wdata = randbits (12);
            shadow[i] = wdata;
            for (int j = 0; cmemat[1] & CM_BUSY; j ++) {
                clockit ();
                if (j > 1000000) {
                    printf ("timed out waiting for write done at %05o\n", i);
                    if (! brkwhenhltd && (pdpat[Z_RF] & f_o_B_RUN)) goto halted;
                    errors ++;
                    break;
                }
            }
            cmemat[1] = CM_ENAB | (wdata * CM_DATA0) | CM_WRITE | (i * CM_ADDR0);
        }

        for (int i = startat; i <= stopat; i ++) {
            if (i >= 010000) {
                xmemat[1] = i * XM_ADDR0;
                for (int j = 0; xmemat[1] & XM_BUSY; j ++) {
                    clockit ();
                    if (j > 1000000) {
                        printf ("timed out waiting for xmem read at %05o\n", i);
                        if (! brkwhenhltd && (pdpat[Z_RF] & f_o_B_RUN)) goto halted;
                        errors ++;
                        break;
                    }
                }
                wdata = shadow[i];
                uint16_t rdata = (xmemat[1] & XM_DATA) / XM_DATA0;
                if (rdata != wdata) {
                    printf ("xmem error %05o was %04o should be %04o\n", i, rdata, wdata);
                    errors ++;
                }
            }
        }

        for (int i = stopat; i < stopat; i ++) {
            for (int j = 0; cmemat[1] & CM_BUSY; j ++) {
                clockit ();
                if (j > 1000000) {
                    printf ("timed out waiting for read done at %05o\n", i);
                    if (! brkwhenhltd && (pdpat[Z_RF] & f_o_B_RUN)) goto halted;
                    errors ++;
                    break;
                }
            }
            cmemat[1] = CM_ENAB | i * CM_ADDR0;
            for (int j = 0; ! (cmemat[1] & CM_RRDY); j ++) {
                clockit ();
                if (j > 1000000) {
                    printf ("timed out waiting for read ready at %05o\n", i);
                    if (! brkwhenhltd && (pdpat[Z_RF] & f_o_B_RUN)) goto halted;
                    errors ++;
                    break;
                }
            }
            wdata = shadow[i];
            uint16_t rdata = (cmemat[1] & CM_DATA) / CM_DATA0;
            if (rdata != wdata) {
                printf ("error %05o was %04o should be %04o\n", i, rdata, wdata);
                errors ++;
            }
        }

        printf ("\npass %u complete  %u errors\n\n", pass, errors);
    }

    return 0;

halted:;
    printf ("processor halted > ");
    fflush (stdout);
    char temp[8];
    fgets (temp, sizeof temp, stdin);
    return 0;
}

// do LOAD ADDRESS with zero in switch register
static void ldad (uint16_t addr)
{
    pdpat[Z_RE] = addr * e_swSR0;
    pdpat[Z_RB] = b_swLDAD;
    for (int i = 0; i < 1000; i ++) clockit ();
    usleep (200000);
    pdpat[Z_RB] = 0;
    for (int i = 0; i < 1000; i ++) clockit ();
    usleep (1000);
}

// do DEPOSIT with 5000 in switch register (JMP 0)
static void depos (uint16_t data)
{
    pdpat[Z_RE] = data * e_swSR0;
    pdpat[Z_RB] = b_swDEP;
    for (int i = 0; i < 1000; i ++) clockit ();
    usleep (200000);
    pdpat[Z_RB] = 0;
    for (int i = 0; i < 1000; i ++) clockit ();
    usleep (1000);
}

// gate through one fpga clock cycle
static void clockit ()
{
    if (manualclock) {
        pdpat[Z_RA] |=   a_nanostep;
        pdpat[Z_RA] &= ~ a_nanostep;
    }
}
