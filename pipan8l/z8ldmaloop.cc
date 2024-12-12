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
    //uint32_t n = 0;
    Z8LPage z8p;
    while (true) {
        /*uint32_t cm =*/ z8p.dmacycle (CM_ENAB, 0);
        //printf ("%09o  %04o\n", ++ n, (cm & CM_DATA) / CM_DATA0);
    }
    return 0;
}
