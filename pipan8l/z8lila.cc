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

#define WIDTH 12
#define DEPTH 4096
#define AFTER 4000

#define ILACTL 021
#define ILADAT 022

#define CTL_ARMED  0x80000000U
#define CTL_AFTER  0x0FFF0000U
#define CTL_INDEX  0x00000FFFU
#define CTL_AFTER0 0x00010000U
#define CTL_INDEX0 0x00000001U

int main (int argc, char **argv)
{
    setlinebuf (stdout);

    Z8LPage z8p;
    uint32_t volatile *pdpat = z8p.findev ("8L", NULL, NULL, false);

    if ((pdpat[ILACTL] & (CTL_ARMED | CTL_AFTER)) != 0) {
        printf ("already armed\n");
    } else {
        pdpat[ILACTL] = CTL_ARMED | AFTER * CTL_AFTER0;
        printf ("newly armed\n");
    }

    uint32_t ctl;
    while (((ctl = pdpat[ILACTL]) & (CTL_ARMED | CTL_AFTER)) != 0) sleep (1);

    bool indotdotdot = false;
    uint32_t lastentry = 0xFFFFFFFFU;
    pdpat[ILACTL] = ctl & CTL_INDEX;
    uint32_t thisentry = pdpat[ILADAT];
    for (int i = 0; i < DEPTH; i ++) {
        pdpat[ILACTL] = (ctl + i * CTL_INDEX0 + CTL_INDEX0) & CTL_INDEX;
        uint32_t nextentry = pdpat[ILADAT];
        if ((thisentry != lastentry) || (thisentry != nextentry)) {
            printf ("%6.2f ", (i - DEPTH + AFTER + 1) / 100.0);
            for (int j = 0; j < WIDTH; j ++) {
                printf (" %o", (thisentry >> (WIDTH - 1 - j)) & 1);
            }
            printf ("\n");
            indotdotdot = false;
        } else if (! indotdotdot) {
            printf ("    ...\n");
            indotdotdot = true;
        }
        lastentry = thisentry;
        thisentry = nextentry;
    }
    return 0;
}
