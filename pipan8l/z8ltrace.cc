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
static uint32_t zrawrite, zrewrite;
static uint32_t volatile *cmemat;
static uint32_t volatile *pdpat;
static uint32_t volatile *xmemat;

static void clockit ();
static void fatalerr (char const *fmt, ...);

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    // access the zynq io page
    // hopefully it has our pdp8l.v code indicated by magic number in first word
    Z8LPage z8p;
    pdpat  = z8p.findev ("8L", NULL, NULL, false);
    cmemat = z8p.findev ("CM", NULL, NULL, false);
    xmemat = z8p.findev ("XM", NULL, NULL, false);

    printf ("8L version %08X\n", pdpat[Z_VER]);

    // select simulator with manual clocking
    pdpat[Z_RA] = zrawrite = ZZ_RA;
    pdpat[Z_RB] = 0;
    pdpat[Z_RC] = ZZ_RC;
    pdpat[Z_RD] = ZZ_RD;
    pdpat[Z_RE] = zrewrite = e_simit;
    pdpat[Z_RF] = 0;
    pdpat[Z_RG] = 0;
    pdpat[Z_RH] = 0;
    pdpat[Z_RI] = 0;
    pdpat[Z_RJ] = 0;
    pdpat[Z_RK] = 0;

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
        // field is being output by the pdp8lxmem.v module
        uint16_t madr = FIELD (Z_RI, i_oMA);
        uint16_t mfld = (xmemat[2] & XM2_FIELD) / XM2_FIELD0;
        bool _bfenab = FIELD (Z_RF, f_o_BF_ENABLE);
        bool _dfenab = FIELD (Z_RF, f_o_DF_ENABLE);
        bool _zfenab = FIELD (Z_RF, f_o_SP_CYC_NEXT);
        uint16_t dmafld = ((cmemat[1] & CM_ADDR) / CM_ADDR0) >> 12;

        // should now be fully transitioned to the major state
        uint8_t majstate = FIELD (Z_RK, k_majstate);

        // end of TS1 means it finished reading that memory location into MB
        for (int i = 0; FIELD (Z_RF, f_oBTS_1); i ++) {
            if (i > 1000) fatalerr ("timed out waiting for TS1 negated\n");
            clockit ();
        }

        // MB should now have the value that was read from memory
        uint16_t mbrd = FIELD (Z_RH, h_oBMB);

        // wait for it to enter TS3, meanwhile cpu (pdp8lsim.v) should possibly be modifying
        // the value and sending the value (modified or not) back to the memory
        for (int i = 0; ! FIELD (Z_RF, f_oBTS_3); i ++) {
            if (i > 1000) fatalerr ("timed out waiting fot TS3 asserted\n");
            clockit ();
        }

        // beginning of TS3, MB should now have the possibly modified value
        uint16_t mbwr = FIELD (Z_RH, h_oBMB);

        // wait for it to leave TS3, where pdp8lsim.v writes the value to memory
        for (int i = 0; FIELD (Z_RF, f_oBTS_3); i ++) {
            if (i > 1000) fatalerr ("timed out waiting for TS3 negated\n");
            clockit ();
        }

        // print cycle
        char linebuf[200];
        char *lineptr = linebuf;
        lineptr += sprintf (lineptr, "%10u  %-5s  L.AC=%o.%04o  _BDZ=%o%o%o BRKFLD=%o  MA=%o.%04o  MB=%04o",
                ++ instrno, majstatenames[majstate], linc, acum, _bfenab, _dfenab, _zfenab, dmafld, mfld, madr, mbrd);
        if (mbwr != mbrd) lineptr += sprintf (lineptr, "->%04o", mbwr);
        if (majstate == MS_FETCH) {
            lineptr += sprintf (lineptr, "  %s", disassemble (mbrd, madr).c_str ());
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
                        lineptr += sprintf (lineptr, " -> IOP%u -> %c%c%04o", m, (ioskp ? 'S' : ' '), (acclr ? 'C' : ' '), ac);
                        for (int i = 0; FIELD (Z_RF, m); i ++) {
                            if (i > 1000) fatalerr ("timed out waiting fot IOP%u negated\n", m);
                            clockit ();
                        }
                    }
                }
            }
        }
        strcpy (lineptr, "\n");
        fputs (linebuf, stdout);
    }
}

// gate through one fpga clock cycle
static void clockit ()
{
    pdpat[Z_RE] = zrewrite | e_nanotrigger;
    clockno ++;
    ////printf ("clockit: %10u bts_3=%o bwc_overflow=%o => ctlwcovf=%o\n",
    ////    clockno, FIELD (Z_RF, f_oBTS_3), FIELD (Z_RF, f_o_BWC_OVERFLOW), (cmemat[1] / CM_WCOVF) & 1);
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
