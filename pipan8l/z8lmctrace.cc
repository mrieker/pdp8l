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

// Print PDP-8/L memory cycle trace
// Uses MDHOLD/MDSTEP bits of pdp8lxmem.v to stop the processor at end of each memory cycle
// Must have ENLO4K set so it will be able to stop processor when it accesses low 4K memory

//  ./z8lmctrace

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

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    // access the zynq io page
    // hopefully it has our pdp8l.v code indicated by magic number in first word
    Z8LPage z8p;
    uint32_t volatile *pdpat  = z8p.findev ("8L", NULL, NULL, false);
    uint32_t volatile *xmemat = z8p.findev ("XM", NULL, NULL, false);

    printf ("8L version %08X\n", pdpat[Z_VER]);
    printf ("XM version %08X\n", xmemat[Z_VER]);

    if (! (xmemat[1] & XM_ENLO4K)) {
        fprintf (stderr, "does not have enlo4k set\n");
        ABORT ();
    }

    while (true) {

        // tell pdp8lxmem.v to let processor do one mem cycle then stop
        xmemat[1] |= XM_MDHOLD | XM_MDSTEP;
        for (int i = 0; xmemat[1] & XM_MDSTEP; i ++) {
            if (FIELD (Z_RF, f_o_B_RUN)) {
                printf ("processor halted\n");
                do usleep (10000);
                while (FIELD (Z_RF, f_o_B_RUN));
                i = 0;
            } else if (i > 10000) {
                fprintf (stderr, "timed out waiting for cycle to complete\n");
                ABORT ();
            }
        }

        // print out mem cycle info
        printf ("%08X:  %05o / %04o -> %04o\n",
            pdpat[Z_RN], FIELD (Z_RL, l_xbraddr), FIELD (Z_RM, m_xbrrdat), FIELD (Z_RM, m_xbrwdat));
    }
}
