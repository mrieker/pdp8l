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

// g++ -Wall -O2 -o extmemtest extmemtest.cc lib.armv7l.a
// ./loadmod.sh
// ./extmemtest

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "z8ldefs.h"
#include "z8lutil.h"

int main ()
{
    uint32_t shadow[32768];

    Z8LPage z8lpage;

    uint32_t volatile *pdpat = z8lpage.findev ("8L", NULL, NULL, true);
    if (pdpat == NULL) ABORT ();
    pdpat[Z_RA] = a_softreset | a_simit | a_i_AC_CLEAR | a_i_BRK_RQST | a_i_EA | a_i_EMA | a_i_INT_INHIBIT | a_i_INT_RQST | a_i_IO_SKIP | a_i_MEMDONE | a_i_STROBE;
    pdpat[Z_RB] = 0;
    pdpat[Z_RC] = 0;
    pdpat[Z_RD] = d_i_DMAADDR | d_i_DMADATA;
    pdpat[Z_RE] = 0;
    pdpat[Z_RF] = 0;
    pdpat[Z_RG] = 0;
    pdpat[Z_RH] = 0;
    pdpat[Z_RI] = 0;
    pdpat[Z_RJ] = 0;
    pdpat[Z_RK] = 0;

    uint32_t volatile *xmemat = z8lpage.findev ("XM", NULL, NULL, true);
    if (xmemat == NULL) ABORT ();
    xmemat[1] = XM_ENABLE | XM_ENLO4K;

    uint32_t volatile *extmemptr = z8lpage.extmem ();

    pdpat[Z_RA] = a_simit | a_i_AC_CLEAR | a_i_BRK_RQST | a_i_EA | a_i_EMA | a_i_INT_INHIBIT | a_i_INT_RQST | a_i_IO_SKIP | a_i_MEMDONE | a_i_STROBE;

    srand (0);
    for (int j = 0; j < 4096; j ++) {

        // write via extmemptr (direct mapping)
        for (uint32_t xaddr = 0; xaddr < 32768; xaddr ++) {
            uint32_t vdata = rand () & 07777;
            shadow[xaddr] = vdata;
            extmemptr[xaddr] = vdata;
        }

        // readback via xmem (via pdp8lxmem.v register)
        for (uint32_t xaddr = 0; xaddr < 32768; xaddr ++) {
            xmemat[1] = XM_ENABLE | XM_ENLO4K | (xaddr * XM_ADDR0);
            for (int i = 0; xmemat[1] & XM_BUSY; i ++) {
                if (i > 1000) ABORT ();
            }
            uint16_t vdata = (xmemat[1] & XM_DATA) / XM_DATA0;

            if (vdata != shadow[xaddr]) {
                printf ("%05o was %04o expected %04o\n", xaddr, vdata, shadow[xaddr]);
            }
        }

        // write via xmem (via pdp8lxmem.v register)
        for (uint32_t xaddr = 0; xaddr < 32768; xaddr ++) {
            uint32_t vdata = rand () & 07777;

            xmemat[1] = XM_ENABLE | XM_ENLO4K | (vdata * XM_DATA0) | XM_WRITE | (xaddr * XM_ADDR0);
            for (int i = 0; xmemat[1] & XM_BUSY; i ++) {
                if (i > 1000) ABORT ();
            }

            shadow[xaddr] = vdata;
            extmemptr[xaddr] = vdata;
        }

        // readback via extmemptr (direct mapping)
        for (uint32_t xaddr = 0; xaddr < 32768; xaddr ++) {
            uint16_t vdata = extmemptr[xaddr];
            if (vdata != shadow[xaddr]) {
                printf ("%05o was %04o expected %04o\n", xaddr, vdata, shadow[xaddr]);
            }
        }

        putchar ('*');
        fflush (stdout);
    }
    putchar ('\n');
    return 0;
}
