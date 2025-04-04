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

// Test AXI-mapped access to the 32K external memory (extmemmap.v)
// It is set up to address each 12-bit word in the low 32-bits of uint32_ts
// ...instead of uint16_ts so it can use the AXI-lite interface

// ./loadmod.sh
// ./z8lxmemtest.armv7l

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "z8ldefs.h"
#include "z8lutil.h"

int main (int argc, char **argv)
{
    if ((argc > 1) && (strcmp (argv[1], "-?") == 0)) {
        puts ("");
        puts ("     Test direct AXI--mapped access to 32K external memory (extmemmap.v)");
        puts ("     Does not need to be plugged into PDP-8/L");
        puts ("");
        puts ("  ./z8lxmemtest");
        puts ("");
        return 0;
    }

    uint32_t shadow[32768];

    Z8LPage z8lpage;

    uint32_t volatile *pdpat = z8lpage.findev ("8L", NULL, NULL, true);
    pdpat[Z_RA] = ZZ_RA;
    pdpat[Z_RB] = 0;
    pdpat[Z_RC] = ZZ_RC;
    pdpat[Z_RD] = ZZ_RD;
    pdpat[Z_RE] = e_simit | e_fpgareset;
    pdpat[Z_RF] = 0;
    pdpat[Z_RG] = 0;
    pdpat[Z_RH] = 0;
    pdpat[Z_RI] = 0;
    pdpat[Z_RJ] = 0;
    pdpat[Z_RK] = 0;

    uint32_t volatile *xmemat = z8lpage.findev ("XM", NULL, NULL, true);
    xmemat[1] = XM_ENABLE | XM_ENLO4K;

    uint32_t volatile *extmemptr = z8lpage.extmem ();

    pdpat[Z_RE] = e_simit;

    uint32_t stopat = 16;
    for (int j = 0; j < 4096; j ++) {

        // write randome to all locations
        for (uint32_t xaddr = 0; xaddr < stopat; xaddr ++) {
            uint32_t vdata = randbits (12);
            shadow[xaddr] = vdata;
            extmemptr[xaddr] = vdata;
        }

        // readback and verify
        for (uint32_t xaddr = 0; xaddr < stopat; xaddr ++) {
            uint16_t vdata = extmemptr[xaddr];
            if (vdata != shadow[xaddr]) {
                printf ("%05o was %04o expected %04o\n", xaddr, vdata, shadow[xaddr]);
            }
        }

        putchar ('*');
        fflush (stdout);
        stopat = 32768;
    }
    putchar ('\n');
    return 0;
}
