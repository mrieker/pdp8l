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

// Loop DMA cycle to read memory location 0
//  ./z8ldmaloop.armv7l

#include <stdio.h>

#include "z8ldefs.h"
#include "z8lutil.h"

int main (int argc, char **argv)
{
    Z8LPage z8p;
    uint32_t volatile *extmem = z8p.extmem ();

    // clear extended memory
    for (int i = 0; i < 32768; i ++) {
        extmem[i] = 0;
    }

    // set up WC/CA pair at 01144

    // write 0000 to 01144
    z8p.dmacycle (CM_ENAB + 00000 * CM_DATA0 + CM_WRITE + 01144 * CM_ADDR0, 0);

    // write 2345 to 01145
    z8p.dmacycle (CM_ENAB + 02345 * CM_DATA0 + CM_WRITE + 01145 * CM_ADDR0, 0);

    // write 4321 to 12345
    extmem[012345] = 04321;

    int good = 0;
    while (true) {
        uint32_t cm = z8p.dmacycle (CM_ENAB + 011144 * CM_ADDR0, CM2_3CYCL);
        if ((cm & CM_DATA) != 04321 * CM_DATA0) {
            printf ("read %04o  (good %d)\n", (cm & CM_DATA) / CM_DATA0, good);
            return 1;
        }
        good ++;
    }
    return 0;
}
