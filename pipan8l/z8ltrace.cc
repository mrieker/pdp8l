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

// Clock the PDP-8/L simulator (pdp8lsim.v) and print out cycle traces

//  ./z8ltrace

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "disassemble.h"
#include "z8ldefs.h"
#include "z8lutil.h"

#define FIELD(index,mask) ((pdpat[index] & mask) / (mask & - mask))

static char const *const majstatenames[] = { MS_NAMES };
static char const *const timestatenames[] = { TS_NAMES };

static uint32_t clockno;
static uint32_t zrawrite, zrcwrite, zrdwrite, zrewrite;
static uint32_t volatile *extmemptr;
static uint32_t volatile *pdpat;
static uint32_t volatile *cmemat;
static uint32_t volatile *xmemat;

static void clockit ();
static void fatalerr (char const *fmt, ...);

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    // access the zynq io page
    // hopefully it has our pdp8l.v code indicated by magic number in first word
    Z8LPage z8p;
    pdpat = z8p.findev ("8L", NULL, NULL, false);
    if (pdpat == NULL) {
        fprintf (stderr, "z8lsimtest: bad magic number\n");
        ABORT ();
    }
    printf ("8L version %08X\n", pdpat[Z_VER]);
    xmemat = z8p.findev ("XM", NULL, NULL, false);
    if (xmemat == NULL) {
        fprintf (stderr, "z8lsimtest: can't find xmem device\n");
        ABORT ();
    }
    printf ("XM version %08X\n", xmemat[0]);
    cmemat = z8p.findev ("CM", NULL, NULL, false);
    if (cmemat == NULL) {
        fprintf (stderr, "z8lsimtest: can't find cmem device\n");
        ABORT ();
    }
    printf ("CM version %08X\n", cmemat[0]);
    cmemat[1] = 0;  // disable outputs so it doesn't interfere with arm_i_DMAADDR, arm_i_DMADATA, arm_iDATA_IN

    // get pointer to the 32K-word ram
    // maps each 12-bit word into low 12 bits of 32-bit word
    // upper 20 bits discarded on write, readback as zeroes
    extmemptr = z8p.extmem ();

    // select simulator with manual clocking
    pdpat[Z_RA] = zrawrite = ZZ_RA;
    pdpat[Z_RB] = 0;
    pdpat[Z_RC] = zrcwrite = 0;
    pdpat[Z_RD] = zrdwrite = ZZ_RD;
    pdpat[Z_RE] = zrewrite = e_simit;
    pdpat[Z_RF] = 0;
    pdpat[Z_RG] = 0;
    pdpat[Z_RH] = 0;
    pdpat[Z_RI] = 0;
    pdpat[Z_RJ] = 0;
    pdpat[Z_RK] = 0;

    // make low 4K memory accesses go to the external memory block by leaving _EA asserted all the time
    // ...so we can directly access its contents via extmemptr
    xmemat[1] = XM_ENABLE | XM_ENLO4K;
    for (int i = 0; i < 5; i ++) clockit ();

    uint32_t instrno = 0;
    while (true) {

        // clock until we see TS1
        while (! FIELD (Z_RF, f_oBTS_1)) {
            clockit ();
        }

        bool linc = FIELD (Z_RG, g_lbLINK);
        uint16_t acum = FIELD (Z_RH, h_oBAC);

        // we should have MEMSTART now
        if (! FIELD (Z_RF, f_oMEMSTART)) fatalerr ("MEMSTART missing at beginning of TS1\n");

        // MA holds the address
        uint16_t madr = FIELD (Z_RI, i_oMA);

        // should now be fully transitioned to the major state
        uint8_t majstate = FIELD (Z_RK, k_majstate);

        // end of TS1 means it finished reading that memory location into MB
        for (int i = 0; FIELD (Z_RF, f_oBTS_1); i ++) {
            if (i > 1000) fatalerr ("timed out waiting for TS1 negated\n");
            clockit ();
        }

        // MB should now have the value we wrote to the memory location
        uint16_t mbrd = FIELD (Z_RH, h_oBMB);

        // wait for it to enter TS3, meanwhile cpu (pdp8lsim.v) should possibly be modifying
        // the value and sending the value (modified or not) back to the memory
        for (int i = 0; ! FIELD (Z_RF, f_oBTS_3); i ++) {
            if (i > 1000) fatalerr ("timed out waiting fot TS3 asserted\n");
            clockit ();
        }

        // beginning of TS3, MB should now have the possibly modified value
        uint16_t mbwr = FIELD (Z_RH, h_oBMB);

        // wait for it to leave TS3, where pdp8lxmem.v writes the value to memory
        for (int i = 0; FIELD (Z_RF, f_oBTS_3); i ++) {
            if (i > 1000) fatalerr ("timed out waiting for TS3 negated\n");
            clockit ();
        }

        // print cycle
        printf ("%10u  %-5s  L.AC=%o.%04o  MA=%04o  MB=%04o", ++ instrno, majstatenames[majstate], linc, acum, madr, mbrd);
        if (mbwr != mbrd) printf ("->%04o", mbwr);
        if (majstate == MS_FETCH) {
            printf ("  %s", disassemble (mbrd, madr).c_str ());
            if ((mbrd & 07000) == 06000) {
                ASSERT ((f_oBIOP1 == 1) && (f_oBIOP2 == 2) && (f_oBIOP4 == 4));
                for (uint16_t m = 1; m <= 4; m += m) {
                    if (m & mbrd) {
                        for (int i = 0; ! FIELD (Z_RF, m); i ++) {
                            if (i > 1000) fatalerr ("timed out waiting fot IOP%u asserted\n", m);
                            clockit ();
                        }
                        bool ioskp = ! FIELD (Z_RA, a_i_IO_SKIP);
                        bool acclr = ! FIELD (Z_RA, a_i_AC_CLEAR);
                        uint16_t ac = FIELD (Z_RH, h_oBAC);
                        printf (" -> IOP%u -> %c%c%04o", m, (ioskp ? 'S' : ' '), (acclr ? 'C' : ' '), ac);
                        for (int i = 0; FIELD (Z_RF, m); i ++) {
                            if (i > 1000) fatalerr ("timed out waiting fot IOP%u negated\n", m);
                            clockit ();
                        }
                    }
                }
            }
        }
        putchar ('\n');
    }
}

// gate through one fpga clock cycle
static void clockit ()
{
    pdpat[Z_RE] = zrewrite | e_nanotrigger;
    clockno ++;
}

static void fatalerr (char const *fmt, ...)
{
    printf ("\n");
    va_list ap;
    va_start (ap, fmt);
    vprintf (fmt, ap);
    va_end (ap);
    ABORT ();
}
