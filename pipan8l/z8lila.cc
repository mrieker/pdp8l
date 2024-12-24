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

// Arm then dump zynq.v ilaarray when triggered

//  ./z8lila.armv7l

#include <alloca.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "z8ldefs.h"
#include "z8lutil.h"

#define DEPTH 4096  // total number of elements in ilaarray
#define AFTER 4000  // number of samples to take after sample containing trigger

#define ILACTL 021
#define ILADAT 022

#define CTL_ARMED  0x80000000U
#define CTL_AFTER0 0x00010000U
#define CTL_INDEX0 0x00000001U
#define CTL_AFTER  (CTL_AFTER0 * (DEPTH - 1))
#define CTL_INDEX  (CTL_INDEX0 * (DEPTH - 1))

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    Z8LPage z8p;
    uint32_t volatile *pdpat = z8p.findev ("8L", NULL, NULL, false);

    // tell zynq.v to start collecting samples
    // tell it to stop when collected trigger sample plus AFTER thereafter
    pdpat[ILACTL] = CTL_ARMED | AFTER * CTL_AFTER0;
    printf ("armed\n");

    // wait for sampling to stop
    uint32_t ctl;
    while (((ctl = pdpat[ILACTL]) & (CTL_ARMED | CTL_AFTER)) != 0) sleep (1);

    // read array[index] = next entry to be overwritten = oldest entry
    pdpat[ILACTL] = ctl & CTL_INDEX;
    uint64_t thisentry = (((uint64_t) pdpat[ILADAT+1]) << 32) | (uint64_t) pdpat[ILADAT+0];

    // loop through all entries in the array
    bool indotdotdot = false;
    uint64_t preventry = 0;
    for (int i = 0; i < DEPTH; i ++) {

        // read array[index+i+1] = array entry after thisentry
        pdpat[ILACTL] = (ctl + i * CTL_INDEX0 + CTL_INDEX0) & CTL_INDEX;
        uint64_t nextentry = (((uint64_t) pdpat[ILADAT+1]) << 32) | (uint64_t) pdpat[ILADAT+0];

        // print thisentry - but use ... if same as prev and next
        if ((i == 0) || (i == DEPTH - 1) || (thisentry != preventry) || (thisentry != nextentry)) {
            printf ("%6.2f  %2u  %o %o  %o %o %o  %o %o %o  %05o\n",
                (i - DEPTH + AFTER + 1) / 100.0,    // trigger shows as 0.00uS
                (unsigned) (thisentry >> 25) & 15,
                (unsigned) (thisentry >> 24) & 1,
                (unsigned) (thisentry >> 23) & 1,
                (unsigned) (thisentry >> 22) & 1,
                (unsigned) (thisentry >> 21) & 1,
                (unsigned) (thisentry >> 20) & 1,
                (unsigned) (thisentry >> 17) & 7,
                (unsigned) (thisentry >> 16) & 1,
                (unsigned) (thisentry >> 15) & 1,
                (unsigned)  thisentry & 077777);
            indotdotdot = false;
        } else if (! indotdotdot) {
            printf ("    ...\n");
            indotdotdot = true;
        }

        // shuffle entries for next time through
        preventry = thisentry;
        thisentry = nextentry;
    }
    return 0;
}
