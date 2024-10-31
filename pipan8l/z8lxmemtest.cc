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

// Test the pdp8lxmem.v and extmemmap.v modules
// - just writes random numbers to the 32K ram and reads them back and verifies
// - works all combinations of xmem and extmemmap interfaces

//  ./z8lxmemtest.armv7l
//  - fpga code must be enabled for sim mode and not being reset
//    ./z8l.sh does it

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>

#include "z8ldefs.h"
#include "z8lutil.h"

#define Z_TTYVER 0
#define Z_TTYKB 1
#define Z_TTYPR 2
#define Z_TTYPN 3

#define KB_FLAG 0x80000000
#define KB_ENAB 0x40000000
#define PR_FLAG 0x80000000
#define PR_FULL 0x40000000

#define ABORT() do { fprintf (stderr, "abort() %s:%d\n", __FILE__, __LINE__); abort (); } while (0)

int main (int argc, char **argv)
{
    Z8LPage z8p;

    uint32_t volatile *pdpat = z8p.findev ("8L", NULL, NULL, true);
    if (pdpat == NULL) {
        fprintf (stderr, "cpu not found\n");
        ABORT ();
    }
    printf ("8L version %08X\n", pdpat[Z_VER]);

    uint32_t volatile *xmemat = z8p.findev ("XM", NULL, NULL, true);
    if (xmemat == NULL) {
        fprintf (stderr, "xmem not found\n");
        return 1;
    }
    printf ("VERSION=%08X\n", xmemat[0]);

    uint32_t volatile *extmem = z8p.extmem ();

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

    // release the reset
    pdpat[Z_RA] = a_simit | a_i_AC_CLEAR | a_i_BRK_RQST | a_i_EA | a_i_EMA | a_i_INT_INHIBIT | a_i_INT_RQST | a_i_IO_SKIP | a_i_MEMDONE | a_i_STROBE;

    // make low 4K memory accesses go to the external memory block by leaving _EA asserted all the time
    // ...so we can directly access its contents via extmemptr, feeding in random numbers as needed
    xmemat[1] = XM_ENABLE | XM_ENLO4K;

    // do tests
    uint16_t shadow[32768];
    uint32_t pass = 0;
    while (true) {
        for (int xaddr = 0; xaddr < 32768; xaddr ++) {
            uint16_t wdata = randbits (12);
            shadow[xaddr] = wdata;
            if (pass & 1) {
                extmem[xaddr] = wdata;
            } else {
                xmemat[1] = (wdata * XM_DATA0) | XM_WRITE | (xaddr * XM_ADDR0);
                while (xmemat[1] & XM_BUSY) { }
            }
        }
        for (int xaddr = 0; xaddr < 32768; xaddr ++) {
            uint16_t wdata = shadow[xaddr];
            uint32_t rdata;
            if (pass & 2) {
                rdata = extmem[xaddr];
            } else {
                xmemat[1] = xaddr * XM_ADDR0;
                while ((rdata = xmemat[1]) & XM_BUSY) { }
                rdata = (rdata & XM_DATA) / XM_DATA0;
            }
            if (rdata != wdata) printf ("error %05o was %04o should be %04o\n", xaddr, rdata, wdata);
        }
        pass ++;
        printf ("pass %8u\n", pass);
    }

    return 0;
}
